#include "kernel/irq.h"
#include "kernel/panic.h"
#include "kernel/sched.h"

namespace kern {

IrqHandler irqHandlers[256] = { nullptr };

void IrqGenericEntry(unsigned int irq) noexcept {
    BUG_ON(irq > 255);
    IrqHandler handler = irqHandlers[irq];
    if (!handler) {
        panic("unregistered IRQ#%u raised", irq);
    }
    handler(irq);
}

void IrqAssign(unsigned int irq, IrqHandler handler) noexcept {
    irqHandlers[irq] = handler;
}

}
