#include "arch/x86/arch/signal.h"
#include "arch/x86/arch/thread.h"
#include "arch/x86/gdt.h"
#include "arch/x86/lapic.h"
#include "arch/x86/msr.h"
#include "arch/x86/x86.h"
#include "drivers/acpi.h"
#include "kernel/clone.h"
#include "kernel/irq.h"
#include "kernel/ksyms.h"
#include "kernel/panic.h"
#include "kernel/sched.h"
#include "kernel/syscall.h"
#include "lib/common.h"
#include "mm/page_alloc.h"
#include "mm/paging.h"

namespace arch {

kern::Errno SignalDeliver(kern::Signal /* sig */, void* /* handler */) noexcept {
    return kern::ENOSYS;
}

kern::Errno SysSigreturn(sched::Task* /* curr */) noexcept {
    return kern::ENOSYS;
}
REGISTER_SYSCALL(sigreturn, SysSigreturn);

}
