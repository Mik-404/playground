#pragma once

#include "arch/irq.h"

typedef void (*IrqHandler)(unsigned int irq);

namespace kern {

// irq_generic_entry processes given IRQ.
void IrqGenericEntry(unsigned int irq) noexcept;

// irq_assign registers handler for given IRQ.
void IrqAssign(unsigned int irq, IrqHandler handler) noexcept;

using arch::IrqEnable;
using arch::IrqDisable;
using arch::IsIrqEnabled;

template <typename Fn>
void WithoutIrqs(Fn f) noexcept {
    bool irq = IsIrqEnabled();
    if (irq) {
        IrqDisable();
    }
    f();
    if (irq) {
        IrqEnable();
    }

}

}
