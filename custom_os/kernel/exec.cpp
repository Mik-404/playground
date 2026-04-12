#include <memory>

#include "defs.h"
#include "elf.h"
#include "fs/vfs.h"
#include "kernel/exec.h"
#include "kernel/printk.h"
#include "kernel/sched.h"
#include "kernel/syscall.h"
#include "lib/common.h"
#include "mm/obj_alloc.h"
#include "mm/page_alloc.h"
#include "mm/kmalloc.h"
#include "mm/new.h"
#include "mm/paging.h"
#include "mm/vmem.h"

namespace kern {

static kern::Errno Execve(sched::Task* curr, vfs::FilePtr new_exe, const mm::MemBuf&, const mm::MemBuf&) noexcept {
    std::unique_ptr<Elf64_Ehdr> hdr(new Elf64_Ehdr());
    if (!hdr) {
        return kern::ENOMEM;
    }

    auto read = new_exe->ReadAt(mm::MemBuf(hdr.get(), sizeof(Elf64_Ehdr), true), 0);
    if (!read.Ok()) {
        return read.Err();
    }

    if (*read < sizeof(Elf64_Ehdr)) {
        return kern::EACCES;
    }

    if (memcmp(hdr->e_ident, ELF_MAGIC, sizeof(ELF_MAGIC) - 1)) {
        return kern::EACCES;
    }

    std::unique_ptr<char[]> phdrs(new char[hdr->e_phentsize * hdr->e_phnum]);
    if (!phdrs) {
        return kern::ENOMEM;
    }

    read = new_exe->ReadAt(mm::MemBuf(phdrs.get(), hdr->e_phentsize * hdr->e_phnum, true), hdr->e_phoff);
    if (!read.Ok()) {
        return read.Err();
    }

    if (*read < hdr->e_phentsize * hdr->e_phnum) {
        return kern::EACCES;
    }

    auto new_vmem = mm::Vmem::New();
    if (!new_vmem.Ok()) {
        return new_vmem.Err();
    }

    curr->vmem = std::move(*new_vmem);
    curr->vmem->SwitchTo();

    for (int i = 0; i < hdr->e_phnum; i++) {
        Elf64_Phdr* phdr = (Elf64_Phdr*)(phdrs.get() + i * hdr->e_phentsize);
        if (phdr->p_type != PT_LOAD) {
            continue;
        }

        if (phdr->p_memsz == 0) {
            printk("[kernel] invalid ELF: phdr->p_memsz == 0\n");
            return kern::EINVAL;
        }

        // TODO: handle bss

        size_t pgcnt = DIV_ROUNDUP(phdr->p_memsz, PAGE_SIZE);
        if (phdr->p_offset % PAGE_SIZE != 0) {
            printk("[kernel] invalid ELF: phdr->p_offset is not page size aligned\n");
            return kern::EINVAL;
        }

        mm::AreaFlags flags = mm::AreaFlag::Fixed;
        if (phdr->p_flags & PF_R) {
            flags |= mm::AreaFlag::Read;
        }
        if (phdr->p_flags & PF_W) {
            flags |= mm::AreaFlag::Write;
        }
        if (phdr->p_flags & PF_X) {
            flags |= mm::AreaFlag::Exec;
        }

        auto res = curr->vmem->MapPages(phdr->p_vaddr, pgcnt, flags, new_exe, phdr->p_offset);
        if (!res.Ok()) {
            return res.Err();
        }
    }

    // Allocate stack.
    auto res = curr->vmem->MapPages(0x800000, 4, mm::AreaFlag::Write | mm::AreaFlag::Read | mm::AreaFlag::Exec | mm::AreaFlag::Fixed, nullptr, 0);
    if (!res.Ok()) {
        return res.Err();
    }

    arch::Registers& regs = curr->arch_thread.Regs();
    regs = arch::Registers::MakeCleanUser();
    regs.Ip() = hdr->e_entry;
    regs.Sp() = 0x800000 + 4 * PAGE_SIZE;

    curr->exe_file = std::move(new_exe);

    return kern::ENOERR;
}

Errno Execve(vfs::PathPtr path, const mm::MemBuf& argv, const mm::MemBuf& env) noexcept {
    sched::Task* curr = sched::Current();

    auto new_exe = vfs::Open(std::move(path), vfs::FileFlag::Readable);
    if (!new_exe.Ok()) {
        return new_exe.Err();
    }

    std::unique_ptr<mm::Vmem> curr_vmem = std::move(curr->vmem);
    auto res = Execve(curr, std::move(*new_exe), argv, env);
    if (res.Ok()) {
        return kern::ENOERR;
    }

    curr->vmem = std::move(curr_vmem);
    if (curr->vmem) {
        curr->vmem->SwitchTo();
    }

    return res;
}

Errno SysExecve(sched::Task*, const char* userPath) noexcept {
    auto path = vfs::Path::FromUser(userPath);
    if (!path.Ok()) {
        return path.Err();
    }
    if (auto err = Execve(std::move(*path), mm::MemBuf(), mm::MemBuf()); !err.Ok()) {
        return err;
    }

    arch::ReturnFromExecve();
}
REGISTER_SYSCALL(execve, SysExecve);

}
