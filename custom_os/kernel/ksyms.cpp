#include <algorithm>
#include <string_view>

#include "kernel/ksyms.h"
#include "kernel/printk.h"

namespace kern {

struct KernelSymbolDesc {
    uintptr_t end_addr;
    size_t size;
    size_t ksymstr_offset;
    size_t ksymstr_size;
};

extern "C" KernelSymbolDesc _ksymdefs_start;
extern "C" KernelSymbolDesc _ksymdefs_end;
extern "C" char _ksymstrs_start;


std::string_view SymbolNameByAddr(uintptr_t addr) noexcept {
    auto sym = std::lower_bound(&_ksymdefs_start, &_ksymdefs_end, addr, [](const KernelSymbolDesc& sym, uintptr_t addr) {
        return sym.end_addr <= addr;
    });
    if (sym == &_ksymdefs_end || addr < sym->end_addr - sym->size) {
        return "<unknown symbol>";
    }
    return std::string_view(&_ksymstrs_start + sym->ksymstr_offset, sym->ksymstr_size);
}

}
