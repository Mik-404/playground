#include <cstring>

#include "mm/obj_alloc.h"
#include "kernel/printk.h"
#include "tests/common.h"
#include "kernel/kernel_thread.h"

namespace {

mm::ObjectAllocator alloc(511);
std::atomic<uint64_t> total_allocated_objs;
SpinLock trace_lock;

void TestAllocator(void*) noexcept {
    while (true) {
        void* ptr = alloc.Alloc();
        if (!ptr) {
            return;
        }
        FAIL_IF_UNALIGNED(ptr, 64, "ObjectAllocator::Alloc returned non-aligned pointer %p", ptr);
        WithIrqSafeLocked(trace_lock, [ptr]() {
            printk<LogLevel::Trace>("[test] allocated ptr: %p\n", ptr);
        });
        uint64_t prev = total_allocated_objs.fetch_add(1);
        if (prev % 10000 == 0) {
            printk("done %d allocations\n", prev);
        }
    }
}

}

void KernelPid1() noexcept {

    printk("==== Running SLAB allocator test ====\n");

    const int N_CHILDREN = 100;

    sched::TaskPtr children[N_CHILDREN];
    for (int i = 0; i < N_CHILDREN; i++) {
        auto child = kern::CreateKthread(TestAllocator, nullptr);
        if (!child.Ok()) {
            panic("cannot create testing kernel thread: %e", child.Err().Code());
        }
        children[i] = std::move(*child);
    }

    for (int i = 0; i < N_CHILDREN; i++) {
        if (auto err = kern::WaitKthread(children[i].Get()); !err.Ok()) {
            panic("wait on testing kernel thread failed: %e", err.Err().Code());
        }
    }

    uint64_t allocations = total_allocated_objs.load(std::memory_order_relaxed);
    printk("done %d allocations in total\n", allocations);
    // 95 Mbytes in test (KASAN pages accounted), 511 – size of object.
    const uint64_t MIN_ALLOCATIONS = 95 * (1 << 20) / 511;
    FAIL_ON(allocations < MIN_ALLOCATIONS, "too few allocations %ld, should be at least %ld", allocations, MIN_ALLOCATIONS);

    printk("[test] done\n");

    TEST_OK();
}
