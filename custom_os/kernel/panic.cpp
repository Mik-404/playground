#include <stdarg.h>

#include "kernel/irq.h"
#include "kernel/panic.h"
#include "kernel/sched.h"
#include "lib/spinlock.h"
#include "lib/stack_unwind.h"
#include "mm/kasan.h"

extern "C" void abort() {
    panic("abort called");
}

namespace arch {

void BroadcastPanic() noexcept;
[[noreturn]] void PanicFinish() noexcept;
[[noreturn]] void ExitTesting(uint8_t code) noexcept;

}

namespace kern {

namespace {

constexpr size_t NO_PANIC = SIZE_MAX;
std::atomic<size_t> panic_cpu;

}

void PanicInit() noexcept {
    panic_cpu = NO_PANIC;
}

void PanicStart(std::source_location location) {
    kasan::Disable();
    kern::IrqDisable();

    size_t cpu_id = PER_CPU_GET(cpu_id);
    size_t expected = NO_PANIC;
    if (panic_cpu.compare_exchange_strong(expected, cpu_id)) {
        // We are the first CPU that started panicing, broadcast panic among other CPUs.
        arch::BroadcastPanic();
    } else {
        if (expected != cpu_id) {
            // Someone have started panic already, but on another CPU.
            arch::PanicFinish();
        }
        // Panic-in-panic occured.
        return;
    }

    printk("PANIC at %s:%d [CPU#%d]", location.file_name(), location.line(), PER_CPU_GET(cpu_id));

    sched::Task* curr = sched::Current();
    if (curr) {
        printk(" [PID %d]", curr->pid);
    }
    printk(": ");
}

[[noreturn]] void PanicFinish() {
#ifdef CONFIG_TESTING_IN_QEMU
    arch::ExitTesting(37);
#endif
    arch::PanicFinish();
}

void DoPanic(const char* msg, ...) {
    va_list args;
    va_start(args, msg);
    vprintk(LogLevel::Panic, msg, args);
    va_end(args);

    printk("\n");

    unwind::PrintStack(unwind::StackIter::FromHere());
}

}
