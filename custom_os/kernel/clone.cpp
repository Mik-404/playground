#include "kernel/clone.h"
#include "kernel/sched.h"
#include "kernel/syscall.h"
#include "kernel/panic.h"
#include "mm/obj_alloc.h"

namespace kern {

kern::Result<sched::TaskPtr> DoClone(void* ip, void* sp, int flags) noexcept {
    mm::AllocFlags af_flags;
    if (flags & CLONE_PID1) {
        af_flags |= mm::AllocFlag::NoSleep;
    }

    auto child = sched::AllocTask(af_flags);
    if (!child.Ok()) {
        return child.Err();
    }

    if (flags & CLONE_PID1) {
        BUG_ON(child->pid != 1);
    }

    sched::Task* curr = sched::Current();

    if (!(flags & CLONE_KERNEL_THREAD)) {
        // Both processes shares the executable file.
        child->exe_file = curr->exe_file;
        child->parent = sched::TaskPtr(curr);
    } else {
        child->parent = *child;
    }

    if (flags & CLONE_KERNEL_THREAD) {
        // Kernel threads don't have its own address space, they use Vmem::Global.
        // To prevent accidental destroy of Vmem::Global, set it to nullptr.
        child->vmem = nullptr;
    } else {
        auto new_vmem = curr->vmem->Clone();
        if (!new_vmem.Ok()) {
            sched::TaskForget(child.Val().Get());
            return new_vmem.Err();
        }
        child->vmem = std::move(*new_vmem);

        auto new_ft = FileTable::Clone(curr->file_table.get());
        if (!new_ft.Ok()) {
            sched::TaskForget(child.Val().Get());
            return new_ft.Err();
        }
        child->file_table = std::move(*new_ft);

        child->sigactions = curr->sigactions;
    }

    if (auto err = arch::Thread::CloneCurrent(&child->arch_thread, flags, ip, sp); !err.Ok()) {
        sched::TaskForget(child.Val().Get());
        return err;
    }

    if (!(flags & CLONE_KERNEL_THREAD)) {
        // Set return value in child.
        child->arch_thread.Regs().Retval() = 0;

        // Publish child in parent children list.
        WithIrqSafeLocked(sched::all_tasks_lock, [&]() {
            child->parent->children_head.InsertFirst(**child);
        });
    }

    sched::WakeTask(child.Val().Get());

    return child;
}

Result<int64_t> SysFork(sched::Task* task) noexcept {
    arch::Registers& parentRegs = task->arch_thread.Regs();
    auto child = kern::DoClone((void*)parentRegs.Ip(), (void*)parentRegs.Sp(), 0);
    if (!child.Ok()) {
        return child.Err();
    }
    return child->pid;
}
REGISTER_SYSCALL(fork, SysFork)

}
