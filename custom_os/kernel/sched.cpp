#include "kernel/per_cpu.h"
#include "fs/file_table.h"
#include "kernel/irq.h"
#include "kernel/panic.h"
#include "kernel/sched.h"
#include "kernel/signal.h"
#include "kernel/syscall.h"
#include "lib/list.h"
#include "lib/seq_bitmap.h"
#include "lib/spinlock.h"
#include "lib/stack_unwind.h"
#include "linker.h"
#include "mm/new.h"
#include "mm/obj_alloc.h"
#include "mm/page_alloc.h"
#include "mm/paging.h"
#include "lib/locking.h"
#include "uapi/resource.h"

namespace arch {

void DoIdle() noexcept;
void InitIdle(sched::Task&) noexcept;

}

constexpr size_t MAX_TASK_COUNT = 1 << 18;

size_t cpu_count = 0;
PER_CPU_DEFINE(size_t, preempt_count);
PER_CPU_DEFINE(sched::Task*, task_idle);
PER_CPU_DEFINE(sched::Task*, task_current);

namespace sched {

struct RunQueue {
    SpinLock lock;
    ListHead<Task, &Task::run_queue_list> TasksHead;
};

static RunQueue global_rq;

static SpinLock pid_bitmap_lock;
static SeqBitmap* pid_bitmap = nullptr;

// Linked list of all tasks in OS.
SpinLock all_tasks_lock;
ListHead<Task, &Task::all_tasks_list> all_tasks_head;

static mm::TypedObjectAllocator<Task> task_alloc;

kern::Result<TaskPtr> AllocTask(mm::AllocFlags af_flags) {
    int32_t pid = WithIrqSafeLocked(pid_bitmap_lock, []() {
        return pid_bitmap->Alloc();
    });

    if (pid < 0) {
        return kern::EAGAIN;
    }

    Task* task = new (task_alloc, af_flags) Task();
    if (!task) {
        return kern::ENOMEM;
    }

    task->pid = pid;
    task->state = TASK_STARTING;
    task->running = false;
    task->Ref();

    WithIrqSafeLocked(all_tasks_lock, [&]() {
        all_tasks_head.InsertFirst(*task);
    });

    return TaskPtr(task);
}

void Init() {
    pid_bitmap = new (mm::AllocFlag::NoSleep) SeqBitmap(MAX_TASK_COUNT, 1, mm::AllocFlag::NoSleep);
}

void PreemptDisable() {
    (*PER_CPU_PTR(preempt_count))++;
}

void PreemptEnable() {
    (*PER_CPU_PTR(preempt_count))--;
}

bool Preemptible() {
    return PER_CPU_GET(preempt_count) == 0;
}

void InitOnCpu() {
    Task* idle_task = new (task_alloc, mm::AllocFlag::NoSleep) Task();
    if (!idle_task) {
        panic("cannot allocate Task for idle task");
    }

    idle_task->state = TASK_RUNNABLE;
    idle_task->pid = 0;
    idle_task->vmem = nullptr;
    arch::InitIdle(*idle_task);

    PER_CPU_SET(task_current, idle_task);
    PER_CPU_SET(task_idle, idle_task);
    PER_CPU_SET(preempt_count, 1);
}


void Start() {
    PreemptEnable();

    // This is the idle task.
    for (;;) {
        Preempt();
        arch::DoIdle();
    }
}

static bool IsIdle(Task* task) {
    return task == PER_CPU_GET(task_idle);
}

void Preempt() {
    Task* curr = sched::Current();
    if (!curr) {
        return;
    }

    // We're trying to preempt idle task.
    if (IsIdle(curr)) {
        Yield();
        return;
    }

    if (curr->state != TASK_RUNNABLE) {
        return;
    }

    if (curr->ticks <= 50) {
        // Time slice not exceeded.
        return;
    }

    Yield();
    curr->ticks = 0;
    return;
}

static Task* PickNextLocked() {
    if (global_rq.TasksHead.Empty()) {
        return nullptr;
    }

    Task* task = &global_rq.TasksHead.First();
    task->Ref();
    task->run_queue_list.Remove();

    task->lock.RawLock();
    return task;
}

void WakeTask(Task* task) {
    IrqSafeScopeLocker grq_locker(global_rq.lock);
    IrqSafeScopeLocker task_locker(task->lock);

    if (task->state == TASK_WAITING || task->state == TASK_STARTING) {
        task->state = TASK_RUNNABLE;
        if (!task->running) {
            global_rq.TasksHead.InsertLast(*task);
        }
    }
}

// SchedFinishContextSwitch completes context switch. Should be called in Yield and in all process entry points. Called from assembly.
extern "C" void SchedFinishContextSwitch(Task* prev) {
    BUG_ON(kern::IsIrqEnabled());

    Task* curr = sched::Current();
    mm::Vmem* next_vm = curr->vmem.get();
    if (next_vm) {
        next_vm->SwitchTo();
    }

    if (prev->pid != 0) {
        global_rq.lock.RawLock();
        prev->lock.RawLock();

        prev->running = false;
        if (prev->state == TASK_RUNNABLE) {
            global_rq.TasksHead.InsertLast(*prev);
        }

        global_rq.lock.RawUnlock();
        prev->lock.RawUnlock();

        prev->Unref();
    }

    kern::IrqEnable();
    kern::SignalDeliver();
}

void Yield() {
    BUG_ON(!kern::IsIrqEnabled());

    kern::IrqDisable();

    Task* curr = sched::Current();

    Task* next = WithRawLocked(global_rq.lock, []() {
        return PickNextLocked();
    });

    BUG_ON_NULL(curr);
    BUG_ON(curr == next);

    if (!next) {
        // No tasks to run.
        state_t curr_state = WithRawLocked(curr->lock, [&]() {
            return curr->state;
        });

        // Scheduler doesn't know about our task, so if current is runnable, just return to it.
        if (curr_state == TASK_RUNNABLE) {
            kern::IrqEnable();
            return;
        }

        // No tasks to run at all, return to idle loop.
        next = PER_CPU_GET(task_idle);
        BUG_ON(next == curr);
    } else {
        next->running = true;
        next->lock.RawUnlock();
    }

    PER_CPU_SET(task_current, next);

    Task* prev = curr->arch_thread.SwitchTo(next->arch_thread, curr);

    SchedFinishContextSwitch(prev);
}

void TimerTick(bool /* from_user */) noexcept {
    Task* curr = sched::Current();
    if (IsIdle(curr)) {
        return;
    }
    curr->ticks++;

    if (!Preemptible()) {
        return;
    }

    Preempt();
    kern::SignalDeliver();
}

// TaskByPidLocked finds a task by its PID. Caller should hold all_tasks_lock.
static TaskPtr TaskByPidLocked(int32_t pid) {
    Task* res = nullptr;
    for (Task& task : all_tasks_head) {
        if (task.pid == pid && task.state != TASK_ZOMBIE) {
            res = &task;
            break;
        }
    }
    if (res) {
        res->Ref();
    }
    return TaskPtr(res);
}

TaskPtr TaskByPid(int32_t pid) {
    return WithIrqSafeLocked(all_tasks_lock, [&]() {
        return TaskByPidLocked(pid);
    });
}

static void ReparentChildren(Task& task) {
    IrqSafeScopeLocker locker(all_tasks_lock);

    if (!task.children_head.Empty()) {
        TaskPtr pid1 = TaskByPidLocked(1);
        BUG_ON(!pid1);

        while (!task.children_head.Empty()) {
            Task& child = task.children_head.First();
            child.children_list.Remove();
            child.parent = pid1;
            pid1->children_head.InsertLast(child);
        }
    }
}

[[noreturn]] void TaskExit(uint32_t status) {
    Task* curr = sched::Current();

    if (curr->pid == 1) {
        if (status & TASK_EXIT_KILLED) {
            panic("PID 1 was killed by signal %d", status & TASK_EXIT_CODE_MASK);
        }
#ifdef CONFIG_TESTING_IN_QEMU
        arch::ExitTesting(status);
#else
        panic("PID 1 exited with code %d", status & TASK_EXIT_CODE_MASK);
#endif
    }

    curr->exitcode = status;

    WithIrqSafeLocked(curr->lock, [&](){
        curr->state = TASK_ZOMBIE;
    });

    curr->exe_file.Reset();

    // All children are now orphaned, reparent them to PID 1.
    ReparentChildren(*curr);

    // Wake up parent waiting on child exit.
    curr->parent->wq.WakeAll();

    if (curr->vmem) {
        // We are going to destroy process vmem, switch to safe zone.
        mm::Vmem::GLOBAL.SwitchTo();
        curr->vmem.reset();
    }

    if (curr->file_table) {
        curr->file_table.reset();
    }

    // Goodbye, cruel world :(.
    Yield();

    BUG_ON_REACH();
}

// PickZombie pops first matching zombie from task children list and returns it.
static kern::Result<Task*> PickZombie(Task& parent, int32_t pid) {
    IrqSafeScopeLocker locker(all_tasks_lock);

    if (parent.children_head.Empty()) {
        return kern::ECHILD;
    }

    for (Task& task : parent.children_head) {
        if (task.state == TASK_ZOMBIE && (pid <= 0 || task.pid == pid)) {
            task.children_list.Remove();
            return &task;
        }
    }

    return nullptr;
}

void TaskForget(Task* task) {
    // Remove from all tasks list first.
    WithIrqSafeLocked(all_tasks_lock, [&]() {
        task->all_tasks_list.Remove();
    });

    // Then free PID. QUIZ: why?
    WithIrqSafeLocked(pid_bitmap_lock, [&]() {
        pid_bitmap->Free(task->pid);
    });

    task->pid = -1;

    task->parent.Reset();
    task->Unref();
}

// SchedReturnFromKthread is called if kernel thread returned.
extern "C" [[noreturn]] void SchedReturnFromKthread() noexcept {
    TaskExit(0);
}

kern::Result<void*> WaitKthread(Task* task) noexcept {
    task->wq.WaitCond([&]() {
        return task->state == TASK_ZOMBIE;
    });

    uintptr_t result = static_cast<uintptr_t>(task->arch_thread.Regs().Retval());

    TaskForget(task);

    return (void*)result;
}


kern::Result<int32_t> SysWait4(Task* curr, int32_t pid, uint32_t* statLoc, int options, struct rusage* ru) noexcept {
   if (ru) {
       return kern::EINVAL;
   }
    std::ignore = options;
    kern::Result<Task*> zombie;

    curr->wq.WaitCond([&] {
        zombie = PickZombie(*curr, pid);
        return !zombie.Ok() || *zombie;
    });

    if (!zombie.Ok()) {
        return zombie.Err();
    }

    int32_t zombie_pid = zombie->pid;
    uint32_t exitcode = zombie->exitcode;

    WithIrqSafeLocked(zombie->lock, [&]() {
        zombie->state = TASK_DEAD;
    });

    TaskForget(*zombie);

    if (!mm::CopyToUser(statLoc, exitcode)) {
        return kern::EFAULT;
    }
    return zombie_pid;
}
REGISTER_SYSCALL(wait4, SysWait4);

int64_t SysExit(Task*, int status) noexcept {
    TaskExit(status & TASK_EXIT_CODE_MASK);
    BUG_ON_REACH();
}
REGISTER_SYSCALL(exit, SysExit);

int64_t SysGetPid(Task* task) noexcept {
    return task->pid;
}
REGISTER_SYSCALL(getpid, SysGetPid);

int64_t SysGetPpid(Task* task) noexcept {
    return task->parent->pid;
}
REGISTER_SYSCALL(getppid, SysGetPpid);

int64_t SysSchedYield(Task*) noexcept {
    sched::Yield();
    return 0;
}
REGISTER_SYSCALL(sched_yield, SysSchedYield);

int64_t SysSleep(sched::Task*, uint64_t seconds) noexcept {
    using namespace time::literals;
    time::SleepUntil(time::NowMonotonic().Add(seconds * 1_s));
    return 0;
}
REGISTER_SYSCALL(sleep, SysSleep);
}
