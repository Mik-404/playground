#include "arch/x86/exc_asm.h"
#include "arch/x86/exc_entry.h"
#include "arch/x86/gdt.h"
#include "arch/x86/lapic.h"
#include "arch/x86/msr.h"
#include "arch/x86/rtc.h"
#include "arch/x86/serial.h"
#include "arch/x86/vga.h"
#include "arch/x86/x86.h"
#include "drivers/acpi.h"
#include "drivers/hpet.h"
#include "drivers/ioapic.h"
#include "kernel/irq.h"
#include "kernel/multiboot.h"
#include "kernel/panic.h"
#include "kernel/per_cpu.h"
#include "kernel/sched.h"
#include "lib/common.h"
#include "lib/stack_unwind.h"
#include "linker.h"
#include "mm/page_alloc.h"
#include "mm/paging.h"

// Used by AP bootstraping code.
void* tmp_bootstrap_stack;

uint32_t cpu_ids[MAX_CPUS];
extern size_t cpu_count;

PER_CPU_DEFINE(x86::Tss, cpu_tss);
PER_CPU_DEFINE(x86::Gdt, cpu_gdt);

namespace {

extern "C" void SyscallEntry();

void InitSyscall() noexcept {
    // Enable syscall/sysret instructions.
    x86::Wrmsr(IA32_EFER, x86::Rdmsr(IA32_EFER) | IA32_EFER_SCE);

    // Syscall entry RIP.
    x86::Wrmsr(IA32_LSTAR, (uint64_t)SyscallEntry);

    // Mask interrupts on syscall enter.
    x86::Wrmsr(IA32_FMASK, x86::RFLAGS_IF);

    // Here is a tricky moment.
    // SYSRET loads predefined values into CS and SS. Despite that GDT is not referenced at this stage,
    // new segment selectors must correspond to valid descriptors,
    // otherwise IRET from interrupt will lead to #GP due to invalid segment selector.
    // SYSRET assumes that user code segment follows user data segment:
    // * CS = IA32_STAR[63:48] + 16
    // * SS = IA32_STAR[63:48] + 8
    uint64_t star = (uint64_t)GDT_SEGMENT_SELECTOR(KERNEL_CODE_SEG, RPL_RING0) << 32;
    star |= (uint64_t)GDT_SEGMENT_SELECTOR(USER_DATA_SEG - 1, RPL_RING3) << 48;
    x86::Wrmsr(IA32_STAR, star);
}

void GdtLoad(x86::Gdt* gdt) noexcept {
    // Load GDT.
    x86::GdtPointer ptr = {
        .size = sizeof(x86::Gdt),
        .base = (uint64_t)gdt,
    };

    __asm__ volatile (
        "lgdtq (%0)"
        : : "r"(&ptr)
    );

    // Reload segment registers. Don't touch gs, because it's already loaded with per-cpu section base.
    __asm__ volatile (
        "mov $0, %ax\n"
        "mov %ax, %ds\n"
        "mov %ax, %es\n"
        "mov %ax, %ss\n"
        "mov %ax, %fs\n"
    );
}

void TssLoad() noexcept {
    __asm__ volatile (
        "mov %0, %%ax\n"
        "ltr %%ax\n"
        : : "g"((uint16_t)GDT_SEGMENT_SELECTOR(TSS_SEG, RPL_RING0))
    );
}

void SetupGdt() noexcept {
    x86::Gdt* gdt = PER_CPU_PTR(cpu_gdt);

    // 64-bit kernel code segment.
    GDT_DESCRIPTOR_64(gdt, KERNEL_CODE_SEG, GDT_GRANULARITY | GDT_LONG | GDT_SYSTEM | GDT_CODE_SEG | GDT_READ);
    // Data ring0 segment.
    GDT_DESCRIPTOR_64(gdt, KERNEL_DATA_SEG, GDT_GRANULARITY | GDT_SYSTEM | GDT_READ);

    // Data ring3 segment.
    GDT_DESCRIPTOR_64(gdt, USER_DATA_SEG, GDT_GRANULARITY | GDT_SYSTEM | GDT_DPL_RING3 | GDT_READ);
    // 64-bit ring3 code segment.
    GDT_DESCRIPTOR_64(gdt, USER_CODE_SEG, GDT_GRANULARITY | GDT_LONG | GDT_SYSTEM | GDT_DPL_RING3 | GDT_CODE_SEG | GDT_READ);

    GdtLoad(gdt);
}

void SetupTss() noexcept {
    x86::Tss* tss = PER_CPU_PTR(cpu_tss);
    memset(tss, '\0', sizeof(x86::Tss));
    tss->rsp0 = 0;

    x86::Gdt* gdt = PER_CPU_PTR(cpu_gdt);
    TSS_DESCRIPTOR_64(gdt, TSS_SEG, (uint64_t)tss, sizeof(x86::Tss), GDT_GRANULARITY | (1<<20) | ((0b1001) << 8) | GDT_PRESENT);
    TssLoad();
}

void SetupPerCpu() noexcept {
    uint32_t apic_id = lapic::CpuId();
    size_t cid = 0;
    while (cid < MAX_CPUS) {
        if (cpu_ids[cid] == apic_id) {
            break;
        }
        cid++;
    }

    BUG_ON(cid >= cpu_count);

    // Point GS to the start of per-cpu section.
    x86::Wrmsr(IA32_GS_BASE, (uint64_t)per_cpu_sections[cid]);
    x86::Wrmsr(IA32_KERNEL_GS_BASE, (uint64_t)per_cpu_sections[cid]);

    PER_CPU_SET(cpu_id, cid);
}

void ScanMadt() noexcept {
    auto header = acpi::LookupRsdt<acpi::MadtHeader>("APIC");
    if (!header) {
        panic("MADT not found");
    }

    const acpi::MadtEntry* entry = &header->FirstEntry;
    const acpi::MadtEntryLapic* lapic_entry = nullptr;

    cpu_count = 0;

    for (;;) {
        if ((uint8_t*)entry >= (uint8_t*)header + header->Acpi.Length)  {
            break;
        }

        switch (entry->Type) {
            case MADT_TYPE_LAPIC:
                lapic_entry = (const acpi::MadtEntryLapic*)&entry->Data;
                cpu_ids[cpu_count++] = (uint32_t)lapic_entry->ApicId;
                break;
            case MADT_TYPE_IOAPIC:
                ioapic::Init((const acpi::MadtEntryIoapic*)&entry->Data);
                break;
        }

        entry = (acpi::MadtEntry*)((uint8_t*)entry + entry->Length);
    }

    if (header->Flags & 1) {
        // Mask all interrupts in old PIC.
        x86::Outb(0xa1, 0xff);
        x86::Outb(0x21, 0xff);
    }
}

void InitCpuCommon() noexcept {
    uint64_t cr0 = x86::ReadCr0();

    // Enable write-protection to prevent supervisor from writing to read-only pages.
    cr0 |= x86::CR0_WP;
    cr0 |= x86::CR0_MP;
    cr0 &= ~x86::CR0_EM;
    x86::WriteCr0(cr0);

    uint64_t cr4 = x86::ReadCr4();

    cr4 |= x86::CR4_OSFXSR;
    cr4 |= x86::CR4_OSXMMEXCPT;
    x86::WriteCr4(cr4);


    // Enable NX bit.
    x86::Wrmsr(IA32_EFER, x86::Rdmsr(IA32_EFER) | IA32_EFER_NXE);

    // Setup syscalls handling.
    InitSyscall();
}

atomic_uint64_t cpus_online;

extern "C" void InitApCpu() noexcept {
    mm::Vmem::GLOBAL.SwitchTo();

    InitCpuCommon();

    SetupPerCpu();
    x86::IdtLoad();
    SetupGdt();
    SetupTss();
    lapic::Init();

    printk("[kernel] CPU#%d is alive and running\n", PER_CPU_GET(cpu_id));

    cpus_online.fetch_add(1, std::memory_order_relaxed);

    kern::IrqEnable();
    lapic::StartTimer();
    sched::Start();
}

void StartAndWaitCpu(uint32_t lapic_id) noexcept {
    uint32_t prev_online = cpus_online.load(std::memory_order_relaxed);

    lapic::StartCpu(lapic_id);

    // Wait until CPU becomes online.
    while (prev_online == cpus_online.load(std::memory_order_relaxed)) {
    }
}

typedef void (*CtorFuncPtr)(void);

// Run global constructors before we start initialization. For more information refer to:
//  * https://gcc.gnu.org/onlinedocs/gccint/Initialization.html
//  * https://github.com/gcc-mirror/gcc/blob/master/libgcc/crtstuff.c
static void DoCtors() {
    uintptr_t fn_addr = (uintptr_t)&_ctors_end - sizeof(CtorFuncPtr);
    while (fn_addr >= (uintptr_t)&_ctors_start) {
        (*(CtorFuncPtr*)fn_addr)();
        fn_addr -= sizeof(CtorFuncPtr);
    }
}

} // anonymous namespace

namespace arch {

void InitTimers() noexcept {
    hpet::Init();
    lapic::CalibrateTimer();
    rtc::Init();
}

void StartBootCpu() noexcept {
    lapic::StartTimer();
    sched::InitOnCpu();
    sched::Start();
}

void StartAllCpus() noexcept {
    kasan::Disable();

    cpus_online.store(1, std::memory_order_relaxed);

    // Copy AP startup code.
    size_t startup_code_sz = (uint8_t*)&_phys_end_cpu_startup - (uint8_t*)&_phys_start_cpu_startup;
    void* startup_code_addr = PHYS_TO_VIRT(&_phys_start_cpu_startup);
    memcpy(PHYS_TO_VIRT((void*)0x8000), startup_code_addr, startup_code_sz);

    // Boot APs one by one.
    uint32_t bspLapicId = lapic::CpuId();
    for (size_t i = 0; i < cpu_count; i++) {
        if (cpu_ids[i] == bspLapicId) {
            continue;
        }

        void* bootstrap_stack = mm::AllocPageSimple(2, mm::AllocFlag::NoSleep);
        if (!bootstrap_stack) {
            panic("cannot allocate memory for boostrap stack");
        }

        tmp_bootstrap_stack = (void*)((uintptr_t)(bootstrap_stack) + PAGE_SIZE * 2);
        StartAndWaitCpu(cpu_ids[i]);
    }

    kasan::Enable();
}

void WakeAllCpus() noexcept {
    lapic::BroadcastIpi(X86_SCHED_BROADCAST_IRQ);
}

void BroadcastPanic() noexcept {
    if (lapic::IsInitialized()) {
        lapic::BroadcastIpi(X86_PANIC_BROADCAST_IRQ);
    }
}

[[noreturn]] void ExitTesting(uint8_t code) noexcept {
    x86::Outw(0x501, code);
    x86::HltForever();
}

[[noreturn]] void PanicFinish() noexcept {
    x86::HltForever();
}

} // namespace arch

struct EarlyData {
    // Contains physical address of multiboot info.
    void* boot_info;
    uint32_t multiboot_magic;
};

extern "C" void X86Init(EarlyData* early_data) noexcept {
    DoCtors();

    vga_init();
    serial::Init();

    PrintkRegisterPrinter({ .print = [](const char* buf, size_t sz) { serial::Com1.Write(std::string_view(buf, sz)); } });
#ifdef CONFIG_DUPLICATE_PRINTK_TO_COM2
    PrintkRegisterPrinter({ .print = [](const char* buf, size_t sz) { serial::Com2.Write(std::string_view(buf, sz)); }, .min_level = LogLevel::Trace });
#endif
    PrintkRegisterPrinter({ .print = [](const char* buf, size_t sz) { vga_write(buf, sz, vga_entry(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK)); } });

    kern::PanicInit();

    InitCpuCommon();

    early_data = (EarlyData*)PHYS_TO_VIRT(early_data);
    multiboot::Init(early_data->multiboot_magic, early_data->boot_info);

    acpi::Init();

    ScanMadt();

    lapic::Init();

    mm::PreserveMemoryArea((uintptr_t)PHYS_TO_VIRT(&_phys_start_early), &_phys_end_hh - &_phys_start_early);

    mm::InitEarlyPageAlloc();

    multiboot::BootInfoRelocate();

    kern::InitPerCpu();

    SetupPerCpu();
    SetupGdt();
    x86::IdtInit();
    x86::IdtLoad();
    SetupTss();
}

