#pragma once

#include <string_view>

#include <cstdint>
#include <cstddef>

namespace kern {

// SumbolNameByAddr returns symbol name assosiated with given address.
std::string_view SymbolNameByAddr(uintptr_t addr) noexcept;

}
