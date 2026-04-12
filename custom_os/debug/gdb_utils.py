import gdb

uintptr_t = gdb.lookup_type("uintptr_t")
Task = gdb.lookup_type("sched::Task")
stack_frame_t = gdb.lookup_type("arch::StackIter::StackFrame")
uint64_t = gdb.lookup_type("uint64_t")


_task_list_offset = None
for f in Task.fields():
    if f.name == "all_tasks_list":
        _task_list_offset = f.bitpos // 8
assert _task_list_offset is not None, "field all_tasks_list not found"


def iter_tasks():
    all_tasks_list = gdb.parse_and_eval("sched::all_tasks_head")
    assert str(all_tasks_list.type) == "ListHead<sched::Task, &sched::Task::all_tasks_list>"
    ptr = all_tasks_list["head_"]["next"]
    while ptr != all_tasks_list["head_"].address:
        task = (ptr.cast(uintptr_t) - _task_list_offset).cast(Task.pointer()).dereference()
        yield task
        ptr = ptr["next"]


def get_func_by_addr(rip):
    block = gdb.block_for_pc(rip)
    if block is None:
        return "???"
    return block.function


class TaskByPidFunc(gdb.Function):
    """Finds Task* by given PID"""

    def __init__(self):
        super().__init__("task_by_pid")

    def invoke(self, pid):
        assert pid.type.code == gdb.TYPE_CODE_INT, "pid is not int"

        for task in iter_tasks():
            if task["pid"] == pid:
                return task.address

        return gdb.Value(0).cast(Task.pointer())


class Container(gdb.Function):
    """Returns pointer to type by ptr of field."""
    def __init__(self):
        super().__init__("container")

    def invoke(self, ptr, typ, field):
        offset = None
        typ = gdb.lookup_type(typ.string())
        for f in typ.fields():
            if f.name == field.string():
                offset = f.bitpos // 8
                break
        assert offset is not None, f"no field {field} in type {typ}"
        return (ptr.cast(uintptr_t) - offset).cast(typ.pointer())


class PerCpu(gdb.Function):
    def __init__(self):
        super().__init__("per_cpu")

    def invoke(self, sym):
        gs = gdb.parse_and_eval("$gs_base").cast(uintptr_t)
        return (gs + sym.address.cast(uintptr_t)).cast(sym.type.pointer())


PerCpu()
Container()
TaskByPidFunc()


class TaskKernelStackBacktrace(gdb.Command):
        def __init__(self):
            super().__init__("task_kbt", gdb.COMMAND_USER)

        def complete(self, text, word):
            return gdb.COMPLETE_SYMBOL

        def invoke(self, args, from_tty):
            task = gdb.parse_and_eval(args)
            assert task.type.code == gdb.TYPE_CODE_PTR and task.type.target() == Task, f"expecting Task* in argument, got {task.type}"

            on_stack_ctx = task["arch_thread"]["saved_kernel_rsp_"].cast(gdb.lookup_type("arch::OnStackContext").pointer())
            ret = on_stack_ctx["ret_addr"]
            frame = on_stack_ctx["rbp"].cast(stack_frame_t.pointer())
            print(f"[0] {get_func_by_addr(ret)}")
            pos = 1
            while frame != 0:
                print(f'[{pos}] {get_func_by_addr(frame["ret"])}')
                pos += 1
                frame = frame["rbp"].cast(stack_frame_t.pointer())

TaskKernelStackBacktrace()
