#include "lib/stack_unwind.h"
#include "kernel/ksyms.h"
#include "kernel/printk.h"

namespace unwind {

static void Skip(StackIter& iter, size_t skip) noexcept {
    while (skip > 0 && iter.HasNext()) {
        skip--;
        iter.Next();
    }
}

void UnwindedStack::Print() const noexcept {
    for (size_t i = 0; i < size; i++) {
        std::string_view name = kern::SymbolNameByAddr(stack[i] - 1);
        printk("[%d] %p %s\n", i, stack[i], name.data());
    }
}

UnwindedStack UnwindedStack::FromStackIter(StackIter iter, size_t skip) noexcept {
    UnwindedStack st;
    Skip(iter, skip);
    while (st.size < MAX_FRAMES && iter.HasNext()) {
        st.stack[st.size++] = iter.Get();
        iter.Next();
    }
    return st;
}

void PrintStack(arch::StackIter iter, size_t skip) noexcept {
    Skip(iter, skip);
    size_t pos = 0;
    iter.Visit([&](uintptr_t addr) noexcept {
        std::string_view name = kern::SymbolNameByAddr(addr - 1);
        printk("[%d] %p %s\n", pos++, addr, name.data());
    });
}

}
