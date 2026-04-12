#include "mm/kasan.h"
#include "kernel/printk.h"
#include "kernel/panic.h"
#include "kernel/sched.h"
#include "lib/locking.h"
#include "lib/stack_unwind.h"
#include "mm/paging.h"
#include "mm/page_alloc.h"
#include "mm/vmem.h"
#include "linker.h"

namespace kasan {

#define KASAN_OBJ_ALLOCATED (1 << 0)
#define KASAN_OBJ_FREED     (1 << 1)

class AllocationTracker {
public:
    struct TrackedObject {
        uintptr_t addr;
        size_t size;

        uint32_t flags;
        size_t alloc_cpu;
        size_t free_cpu;
        int32_t alloc_pid;
        int32_t free_pid;
        unwind::UnwindedStack alloc_stack;
        unwind::UnwindedStack free_stack;
        size_t alloc_id;
    };

private:
    static constexpr size_t MAX_TRACKED_OBJECTS = 10000;

    SpinLock lock_;
    TrackedObject* objs_ = nullptr;
    size_t objs_cnt_ = 0;
    size_t alloc_cnt_ = 0;

public:
    void Init() {
        size_t pagesCount = DIV_ROUNDUP(MAX_TRACKED_OBJECTS * sizeof(TrackedObject), PAGE_SIZE);

        void* chunk = mm::EarlyAllocPage(pagesCount);
        if (!chunk) {
            panic("kasan: cannot allocate enough space for allocation tracking");
        }
        objs_ = new (chunk) TrackedObject[MAX_TRACKED_OBJECTS];
        printk("[kasan] allocated %lu pages for allocation tracking\n", pagesCount);
    }

    TrackedObject* AllocObjLocked() {
        TrackedObject* obj = &objs_[objs_cnt_];
        objs_cnt_ = (objs_cnt_ + 1) % MAX_TRACKED_OBJECTS;
        return obj;
    }

    void TrackAllocation(uintptr_t addr, size_t sz) {
        Unpoison(addr, sz);

        IrqSafeScopeLocker locker(lock_);

        TrackedObject* obj = nullptr;
        for (size_t i = 0; i < MAX_TRACKED_OBJECTS; i++) {
            // TODO: check for allocations of same address
            if (objs_[i].addr == addr) {
                obj = &objs_[i];
                break;
            }
        }
        if (!obj) {
            obj = AllocObjLocked();
        }
        obj->addr = addr;
        obj->size = sz;
        obj->flags = KASAN_OBJ_ALLOCATED;
        obj->alloc_cpu = PER_CPU_GET(cpu_id);
        obj->alloc_pid = sched::Current() ? sched::Current()->pid : 0;
        obj->alloc_id = alloc_cnt_++;
        obj->alloc_stack = unwind::UnwindedStack::FromStackIter(unwind::StackIter::FromHere(), 1);
    }

    void TrackFree(uintptr_t addr, size_t sz) {
        Poison(addr, sz);

        IrqSafeScopeLocker locker(lock_);

        TrackedObject* obj = nullptr;
        for (size_t i = 0; i < MAX_TRACKED_OBJECTS; i++) {
            TrackedObject* ptr = &objs_[i];
            if (ptr->addr == addr) {
                obj = ptr;
                break;
            }
        }
        if (!obj) {
            obj = AllocObjLocked();
            obj->flags = 0;
        }

        obj->addr = addr;
        obj->size = sz;
        obj->flags |= KASAN_OBJ_FREED;
        obj->free_cpu = PER_CPU_GET(cpu_id);
        obj->free_pid = sched::Current() ? sched::Current()->pid : 0;
        obj->alloc_id = alloc_cnt_;
        obj->free_stack = unwind::UnwindedStack::FromStackIter(unwind::StackIter::FromHere(), 1);
    }

    TrackedObject* FindObject(uintptr_t addr) noexcept {
        IrqSafeScopeLocker locker(lock_);

        TrackedObject* obj = nullptr;
        for (size_t i = 0; i < MAX_TRACKED_OBJECTS; i++) {
            TrackedObject* ptr = &objs_[i];
            if (ptr->addr <= addr && addr < ptr->addr + ptr->size) {
                obj = ptr;
                break;
            }
        }
        return obj;
    }
};

AllocationTracker alloc_tracker;

static std::atomic<uint64_t> EnabledId;

inline volatile uint8_t* AddrToShadow(uintptr_t addr) {
    if (KERNEL_IMAGE_START <= addr) {
        return (volatile uint8_t*)(KERNEL_KASAN_SHADOW_IMAGE_MEMORY_START + ((addr - KERNEL_IMAGE_START) >> 3));
    } else if (KERNEL_DIRECT_PHYS_MAPPING_START <= addr && addr <= KERNEL_DIRECT_PHYS_MAPPING_START + KERNEL_DIRECT_PHYS_MAPPING_SIZE) {
        return (volatile uint8_t*)(KERNEL_KASAN_SHADOW_MEMORY_START + ((addr - KERNEL_DIRECT_PHYS_MAPPING_START) >> 3));
    }
    panic("KASAN: invalid address reference %p", addr);
}

void Enable() {
    EnabledId.store(0xdeadbeef, std::memory_order_relaxed);
}

void Disable() {
    EnabledId.store(0, std::memory_order_relaxed);
}

bool IsEnabled() {
    return EnabledId.load(std::memory_order_relaxed) == 0xdeadbeef;
}

// TODO: check for atomicity
void Poison(uintptr_t addr, size_t sz) {
    volatile uint8_t* shadow = AddrToShadow((uintptr_t)addr);
    volatile uint8_t* end = AddrToShadow((uintptr_t)addr + sz);
    for (; shadow < end; shadow++) {
        *shadow = 0xff;
    }
    if (sz % 8 == 0) {
        return;
    }
    *shadow = 8 - (sz % 8);
}

void Unpoison(uintptr_t addr, size_t sz) {
    volatile uint8_t* shadow = AddrToShadow((uintptr_t)addr);
    volatile uint8_t* end = AddrToShadow((uintptr_t)addr + sz);
    for (; shadow < end; shadow++) {
        *shadow = 0;
    }
    if (sz % 8 == 0) {
        return;
    }
    *shadow = 0;
}

inline bool Access1(uintptr_t addr) {
    int8_t val = *(volatile int8_t*)AddrToShadow(addr);
    if (val == 0) {
        return true;
    }
    int8_t last = (int8_t)(addr & 7);
    return last < val;
}

inline bool Access248(uintptr_t addr, size_t sz) {
    if (addr % 8 == 0) {
        return Access1(addr + sz - 1);
    }
    return *(volatile uint8_t*)AddrToShadow(addr) == 0 && Access1(addr + sz - 1);
}

inline bool Access16(uintptr_t addr) {
    if (*(volatile uint16_t*)AddrToShadow(addr) != 0) {
        return false;
    }
    if (addr % 16 == 0) {
        return true;
    }
    return Access1(addr + 15);
}

inline bool AccessN(uintptr_t addr, size_t sz) {
    volatile uint8_t* shadow = AddrToShadow(addr);
    volatile uint8_t* shadow_end = AddrToShadow(addr + sz);
    while (shadow != shadow_end) {
        if (*shadow != 0) {
            return false;
        }
        shadow++;
    }
    return Access1(addr + sz - 1);
}

inline bool AccessOk(uintptr_t addr, size_t sz) {
    if (!IsEnabled()) {
        return true;
    }
    switch (sz) {
        case 0:
        case 1:
            return Access1(addr);
        case 2:
        case 4:
        case 8:
            return Access248(addr, sz);
        case 16:
            return Access16(addr);
        default:
            return AccessN(addr, sz);
    }
}

[[noreturn]] static void ReportFinish(AllocationTracker::TrackedObject* obj) {
    unwind::PrintStack(unwind::StackIter::FromHere(), 2);

    if (obj) {
        if (obj->alloc_stack.size != 0) {
            printk("Allocated on CPU#%x in PID %d id=%lu at:\n", obj->alloc_cpu, obj->alloc_pid, obj->alloc_id);
            obj->alloc_stack.Print();
        }
        if (obj->free_stack.size != 0) {
            printk("Freed on CPU#%x in PID %d id=%lu at:\n", obj->free_cpu, obj->free_pid, obj->alloc_id);
            obj->free_stack.Print();
        }
    }

    kern::PanicFinish();
}

[[noreturn]] static void Report(uintptr_t addr, size_t size, const char* type) {
    kern::PanicStart();
    printk("KASAN: invalid %s %lu byte(s) at %p on CPU#%x in PID %d at:\n", type, size, addr, PER_CPU_GET(cpu_id), sched::Current() ? sched::Current()->pid : 0);
    ReportFinish(alloc_tracker.FindObject(addr));
}

void RecordAlloc(uintptr_t addr, size_t size) {
    alloc_tracker.TrackAllocation(addr, size);
}

void RecordFree(uintptr_t addr, size_t size) {
    alloc_tracker.TrackFree(addr, size);
}

void PreFree(uintptr_t addr, size_t size) {
    AllocationTracker::TrackedObject* obj = alloc_tracker.FindObject(addr);

    if (!obj) {
        return;
    }

    if (obj->addr != addr) {
        kern::PanicStart();
        printk("KASAN: free called on mismatched address %p (should be %p) on CPU#%u in PID %d at:\n", addr, obj->addr, PER_CPU_GET(cpu_id), sched::Current() ? sched::Current()->pid : 0);
        ReportFinish(obj);
    }

    if (obj->flags & KASAN_OBJ_FREED) {
        kern::PanicStart();
        printk("KASAN: double-free detected on object %p on CPU#%x in PID %d id=%lu at:\n", addr, PER_CPU_GET(cpu_id), sched::Current() ? sched::Current()->pid : 0, obj->alloc_id);
        ReportFinish(obj);
    }

    if (obj->size != size) {
        printk("KASAN: free size mismatch: %lu bytes were allocated for object, trying to free %lu bytes, at:\n", obj->size, size);
        ReportFinish(obj);
    }
}

// Kernel address sanitizer interface.

extern "C" void __asan_before_dynamic_init() {}
extern "C" void __asan_after_dynamic_init() {}

extern "C" void __asan_handle_no_return() {}

#define KASAN_LOAD_N(N) extern "C" void __asan_load##N##_noabort(uintptr_t addr) { \
    if (!AccessOk(addr, N)) { \
        Report(addr, N, "READ"); \
    } \
}

#define KASAN_STORE_N(N) extern "C" void __asan_store##N##_noabort(uintptr_t addr) { \
    if (!AccessOk(addr, N)) { \
        Report(addr, N, "WRITE"); \
    } \
}

KASAN_LOAD_N(1)
KASAN_LOAD_N(2)
KASAN_LOAD_N(4)
KASAN_LOAD_N(8)
KASAN_LOAD_N(16)
KASAN_STORE_N(1)
KASAN_STORE_N(2)
KASAN_STORE_N(4)
KASAN_STORE_N(8)
KASAN_STORE_N(16)

extern "C" void __asan_storeN_noabort(uintptr_t addr, size_t sz) {
    if (!AccessOk(addr, sz)) {
        Report(addr, sz, "WRITE");
    }
}

extern "C" void __asan_loadN_noabort(uintptr_t addr, size_t sz) {
    if (!AccessOk(addr, sz)) {
        Report(addr, sz, "READ");
    }
}

// Init routines.

static void InitRegion(uintptr_t virt_addr, uintptr_t shadow_pages, size_t pages_needed, uint8_t filler) {
    printk("[kasan] shadow memory at %p-%p\n", virt_addr, virt_addr + pages_needed * PAGE_SIZE);
    for (size_t j = 0; j < pages_needed; j++) {
        memset((void*)shadow_pages, filler, PAGE_SIZE);
        auto res = mm::Vmem::GLOBAL.Map4KbPage(virt_addr, (uintptr_t)VIRT_TO_PHYS(shadow_pages), PTE_WRITE, mm::AllocFlag::SkipKasan);
        if (!res.Ok()) {
            panic("cannot map pages for shadow memory: %e", res.Err().Code());
        }
        virt_addr += PAGE_SIZE;
        shadow_pages += PAGE_SIZE;
    }
}

void InitAreas(mm::PageAllocArea* areas, size_t area_cnt) {
    size_t pagesTotal = 0;
    for (size_t i = 0; i < area_cnt; i++) {
        mm::PageAllocArea* area = &areas[i];
        pagesTotal += area->pages_total;
    }
    size_t image_size_pages = DIV_ROUNDUP((uint64_t)&_phys_end_hh - (uint64_t)&_phys_start_hh, PAGE_SIZE);
    pagesTotal += image_size_pages;
    size_t shadow_pages_cnt = pagesTotal / 8;
    uintptr_t shadow_pages = (uintptr_t)mm::EarlyAllocPage(shadow_pages_cnt, mm::AllocFlag::SkipKasan);
    if (!shadow_pages) {
        panic("kasan: cannot allocate enough shadow memory");
    }

    for (size_t i = 0; i < area_cnt; i++) {
        mm::PageAllocArea* area = &areas[i];
        volatile uintptr_t virt_addr = (uintptr_t)AddrToShadow(area->base);
        size_t pages_needed = DIV_ROUNDUP(areas[i].pages_total, 8);
        InitRegion(virt_addr, shadow_pages, pages_needed, 0xff);
        shadow_pages += pages_needed * PAGE_SIZE;
    }

    size_t pages_needed = DIV_ROUNDUP(image_size_pages, 8);
    InitRegion(KERNEL_KASAN_SHADOW_IMAGE_MEMORY_START, shadow_pages, pages_needed, 0);

    for (size_t i = 0; i < area_cnt; i++) {
        mm::PageAllocArea* area = &areas[i];

        // Unpoison early allocated pages in this area.
        size_t early_pages = area->pages_total - area->size_in_pages;
        if (early_pages > 0) {
            Unpoison(area->base + area->size_in_pages * PAGE_SIZE, area->pages_total - area->size_in_pages);
        }
    }

    printk("[kasan] initialized\n");

    Disable();
}

void InitFinish() {
    alloc_tracker.Init();
}

} // namespace kasan

int memcmp(const void* str1, const void* str2, size_t sz) noexcept {
    if (!kasan::AccessOk((uintptr_t)str1, sz)) {
        kasan::Report((uintptr_t)str1, sz, "READ");
    }

    if (!kasan::AccessOk((uintptr_t)str2, sz)) {
        kasan::Report((uintptr_t)str2, sz, "READ");
    }

    return KASAN_NO_INTERCEPT(memcmp)(str1, str2, sz);
}

void memset(void* p, int ch, size_t sz) noexcept {
    if (!kasan::AccessOk((uintptr_t)p, sz)) {
        kasan::Report((uintptr_t)p, sz, "WRITE");
    }
    KASAN_NO_INTERCEPT(memset)(p, ch, sz);
}

void memcpy(void* dst, const void* src, size_t sz) noexcept {
    if (!kasan::AccessOk((uintptr_t)src, sz)) {
        kasan::Report((uintptr_t)src, sz, "READ");
    }
    if (!kasan::AccessOk((uintptr_t)dst, sz)) {
        kasan::Report((uintptr_t)dst, sz, "WRITE");
    }
    KASAN_NO_INTERCEPT(memcpy)(dst, src, sz);
}

void memmove(void* dst, const void* src, size_t sz) noexcept {
    if (!kasan::AccessOk((uintptr_t)src, sz)) {
        kasan::Report((uintptr_t)src, sz, "READ");
    }
    if (!kasan::AccessOk((uintptr_t)dst, sz)) {
        kasan::Report((uintptr_t)dst, sz, "WRITE");
    }
    KASAN_NO_INTERCEPT(memmove)(dst, src, sz);
}

bool mm::CopyToUser(void* uptr, const void* kptr, size_t n) noexcept {
    if (!kasan::AccessOk((uintptr_t)kptr, n)) {
        kasan::Report((uintptr_t)kptr, n, "READ");
    }
    return KASAN_NO_INTERCEPT(CopyToUser)(uptr, kptr, n);
}

bool mm::CopyFromUser(void* kptr, const void* uptr, size_t n) noexcept {
    if (!kasan::AccessOk((uintptr_t)kptr, n)) {
        kasan::Report((uintptr_t)kptr, n, "WRITE");
    }
    return KASAN_NO_INTERCEPT(CopyFromUser)(kptr, uptr, n);
}

kern::Result<size_t> mm::CopyStringFromUser(char* kptr, const char* uptr, size_t n) noexcept {
    if (!kasan::AccessOk((uintptr_t)kptr, n)) {
        kasan::Report((uintptr_t)kptr, n, "WRITE");
    }
    return KASAN_NO_INTERCEPT(CopyStringFromUser)(kptr, uptr, n);
}
