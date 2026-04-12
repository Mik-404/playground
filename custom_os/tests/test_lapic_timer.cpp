#include "arch/x86/serial.h"
#include "tests/common.h"
#include "kernel/wait.h"
#include "kernel/time.h"

using namespace time::literals;

void KernelPid1() noexcept {
    for (;;) {
        serial::Com2.Write('@');
        // For testing purposes, you can uncomment line below:
        // printk("@");
        auto now = time::NowMonotonic();
        kern::WaitQueue wq;
        wq.WaitCondDeadline([&]() { return false; }, now.Add(5_ms));
    }
}
