#include "drivers/acpi.h"
#include "drivers/ata.h"
#include "fs/block_device.h"
#include "fs/buffer.h"
#include "fs/ext2.h"
#include "fs/vfs.h"
#include "fs/writeback.h"
#include "kernel/kernel_thread.h"
#include "kernel/elf.h"
#include "kernel/exec.h"
#include "kernel/irq.h"
#include "kernel/ksyms.h"
#include "kernel/multiboot.h"
#include "kernel/panic.h"
#include "kernel/per_cpu.h"
#include "kernel/printk.h"
#include "kernel/sched.h"
#include "kernel/time.h"
#include "lib/shared_ptr.h"
#include "linker.h"
#include "mm/kasan.h"
#include "mm/kmalloc.h"
#include "mm/new.h"
#include "mm/page_alloc.h"
#include "mm/vmem.h"

namespace arch {

void StartAllCpus() noexcept;
void InitTimers() noexcept;
void StartBootCpu() noexcept;

}

static void DumpMemory() {
    multiboot::MemoryMapIter it;

    printk("[mm] memory map:\n");
    for (;;) {
        const multiboot::MemoryMapEntry* entry = it.Next();
        if (!entry) {
            break;
        }
        printk("[mm] ");
        switch (entry->type) {
        case MULTIBOOT_MMAP_TYPE_RAM:
            printk("ram  ");
            break;
        case MULTIBOOT_MMAP_TYPE_ACPI:
            printk("acpi ");
            break;
        default:
            printk("rsrv ");
            break;
        }
        printk(" %p-%p (%lu KBytes)\n", entry->base_addr, entry->base_addr + entry->length, entry->length / KB);
    }
}

class SerialFile : public vfs::File {
public:
    SerialFile(vfs::FileFlags flags)
        : vfs::File(flags)
    {}

    kern::Result<size_t> Read(mm::MemBuf) noexcept override {
        return kern::ENOSYS;
    }

    kern::Result<size_t> Write(mm::MemBuf buf) noexcept override {
        std::unique_ptr<char[]> kbuf(new char[buf.Size() + 1]);
        if (!kbuf) {
            return kern::ENOMEM;
        }
        auto err = buf.CopyTo(kbuf.get(), buf.Size());
        if (!err.Ok()) {
            return err;
        }
        kbuf[buf.Size()] = '\0';
        printk("%s", kbuf.get());
        return buf.Size();
    }

    vfs::FilePtr Clone() noexcept override {
        return vfs::FilePtr(this);
    }
};

static kern::Errno PreparePID1() {
    sched::Task* task = sched::Current();

    auto new_ft = FileTable::Clone(nullptr);
    if (!new_ft.Ok()) {
        return new_ft.Err();
    }
    task->file_table = std::move(*new_ft);

    vfs::FilePtr stdin(new (mm::AllocFlag::NoSleep) SerialFile(vfs::FileFlag::Readable));
    if (!stdin) {
        return kern::ENOMEM;
    }

    vfs::FilePtr stdout(new (mm::AllocFlag::NoSleep) SerialFile(vfs::FileFlag::Writeable));
    if (!stdout) {
        return kern::ENOMEM;
    }

    vfs::FilePtr stderr(new (mm::AllocFlag::NoSleep) SerialFile(vfs::FileFlag::Writeable));
    if (!stderr) {
        return kern::ENOMEM;
    }

    auto fd = task->file_table->AssignFd(std::move(stdin));
    if (!fd.Ok()) {
        return fd.Err();
    }
    BUG_ON(*fd != 0);

    fd = task->file_table->AssignFd(std::move(stdout));
    if (!fd.Ok()) {
        return fd.Err();
    }
    BUG_ON(*fd != 1);

    fd = task->file_table->AssignFd(std::move(stderr));
    if (!fd.Ok()) {
        return fd.Err();
    }
    BUG_ON(*fd != 2);

    return kern::ENOERR;
}

using namespace time::literals;

__attribute__((weak)) void KernelPid1() noexcept {
    auto dev = BlockDevice::ByName("ata");
    if (!dev) {
        panic("cannot find device /dev/ata");
    }

    auto root_fs = ext2::Mount(dev);
    if (!root_fs.Ok()) {
        panic("cannot mount root ext2 fs: %e", root_fs.Err().Code());
    }

    vfs::SetRoot(*root_fs);

    printk("[init] mounted ext2 at /\n");

    auto err = PreparePID1();
    if (!err.Ok()) {
        panic("cannot prepare PID 1: %e", err.Code());
    }

    auto path = vfs::Path::FromKernel("/bin/run_tests");
    if (!path.Ok()) {
        panic("cannot allocate path for init execve: %e", err.Code());
    }

    if (auto err = kern::Execve(std::move(*path), mm::MemBuf(), mm::MemBuf()); !err.Ok()) {
        panic("cannot execve /bin/run_tests: %e", err.Code());
    }

    arch::ReturnFromExecve();
}

// InitAndRun finishes kernel initialization calling initializers which require PID 1 created or process context.
void InitAndRun(void*) noexcept {
    fs::BuffersWritebackStart();

    KernelPid1();
}

__attribute__((weak)) void EarlyTest() {}

const char* KernelCommandLine = nullptr;

extern "C" void KernelMainReturn() noexcept {
    BUG_ON_REACH();
}

extern "C" void KernelMain() noexcept {
    DumpMemory();

    mm::InitGlobalVmem();
    kasan::Init();

    mm::InitPageAlloc();
    mm::InitKmalloc();
    mm::InitVmem();

    kern::IrqEnable();
    arch::InitTimers();
    time::Init();

    ata::Init();
    vfs::Init();
    ext2::Init();

    sched::Init();

    arch::StartAllCpus();

    auto err = kern::CreatePid1(InitAndRun, (void*)nullptr);
    if (!err.Ok()) {
        panic("cannot create the first kernel thread: %e", err.Code());
    }

    arch::StartBootCpu();
}
