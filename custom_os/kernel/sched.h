#pragma once

#include <concepts>
#include <cstddef>
#include <functional>

#include "arch/thread.h"
#include "fs/file_table.h"
#include "fs/vfs.h"
#include "kernel/per_cpu.h"
#include "kernel/signal.h"
#include "kernel/wait.h"
#include "kernel/time.h"
#include "lib/list.h"
#include "lib/shared_ptr.h"
#include "mm/vmem.h"

namespace arch {

void DoIdle() noexcept;
void InitIdle(sched::Task&) noexcept;

}

namespace sched {

typedef enum state {
    TASK_STARTING = 0,
    TASK_RUNNABLE = 1,
    TASK_WAITING = 2,
    TASK_ZOMBIE = 3,
    TASK_DEAD = 4,
} state_t;

struct Task;

using TaskPtr = IntrusiveSharedPtr<Task, NoOpRefCountedTracker<Task>>;

struct Task {
    // arch::Thread must be the first member.
    arch::Thread arch_thread;

    RefCounted refs;

    SpinLock lock;

    int64_t pid = -1;
    state_t state = TASK_STARTING;
    int ticks = 0;
    std::unique_ptr<mm::Vmem> vmem;
    uint32_t exitcode = 0;

    std::unique_ptr<FileTable> file_table;
    vfs::FilePtr exe_file;

    kern::AtomicSignalMask pending_signals;
    kern::AtomicSignalMask blocked_signals;
    kern::SigActions sigactions;

    TaskPtr parent;

    ListNode children_list;
    ListHead<Task, &Task::children_list> children_head;

    // Wait queue for waiting on children.
    kern::WaitQueue wq;

    // Scheduler queue.
    ListNode run_queue_list;
    bool running = false;

    ListNode all_tasks_list;
    void Ref() noexcept {
        refs.Ref();
    }

    void Unref() noexcept {
        if (refs.Unref()) {
            delete this;
        }
    }
};

extern ListHead<Task, &Task::all_tasks_list> all_tasks_head;
extern SpinLock all_tasks_lock;

// Init performs global scheduler initialization.
void Init();

// InitOnCpu initializes per-cpu entities on this CPU. sched_init must be called first.
void InitOnCpu();

// Start launches scheduler on current core. InitOnCpu must be called first.
void Start();

// Yield performs context switch into next process.
void Yield();

// Preempt tries to preempt current process.
void Preempt();

// PreemptDisable disables preemption on current CPU. Reentrant.
void PreemptDisable();

// PreemptEnable enables preemption on current CPU. Reentrant.
void PreemptEnable();

// TimerTick should be called by timer interrupt.
void TimerTick(bool from_user);

// Task killed by signal.
constexpr uint64_t TASK_EXIT_KILLED = 1 << 31;
// Exit code or killing signal mask.
constexpr uint64_t TASK_EXIT_CODE_MASK = 0xff;

// TaskExit exists current task using given exit status.
[[noreturn]] void TaskExit(uint32_t status);

// WakeTask wakes the task and pushes it on run queue, if needed.
void WakeTask(Task* task);

// AllocTask allocates PID, task and register it on all_tasks_list.
kern::Result<TaskPtr> AllocTask(mm::AllocFlags af_flags);

// TaskForget is opposite to AllocTask.
void TaskForget(Task* t);

// TaskByPid returns pointer to Task which refers to given PID.
TaskPtr TaskByPid(int32_t pid);

// WaitKthread waits until kernel thread task finishes and returns it's return value.
kern::Result<void*> WaitKthread(Task* task) noexcept;

PER_CPU_DECLARE(Task*, task_current);

inline Task* Current() {
    return PER_CPU_GET(task_current);
}

}
