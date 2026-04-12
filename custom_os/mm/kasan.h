#pragma once

#include <cstddef>
#include <cstdint>

#include "arch/ptr.h"
#include "mm/page_alloc.h"
#include "lib/memory.h"

#define NO_KASAN __attribute__((no_sanitize("kernel-address")))

namespace kasan {

#ifdef CONFIG_KASAN

void Enable();
void Disable();
bool IsEnabled();

void Poison(uintptr_t addr, size_t sz);
void Unpoison(uintptr_t addr, size_t sz);

void RecordAlloc(uintptr_t addr, size_t sz);
void RecordFree(uintptr_t addr, size_t sz);
void PreFree(uintptr_t addr, size_t sz);

template <typename T>
void PoisonPtr(T*& ptr) {
    ptr = (T*)arch::InvalidPointer;
}

void Init();
void InitFinish();
void InitAreas(mm::PageAllocArea* areas, size_t areas_cnt);

#else // no KASAN

inline void Enable() {}
inline void Disable() {}
inline bool IsEnabled() {
    return false;
}

inline void Poison(uintptr_t, size_t) {}
inline void Unpoison(uintptr_t, size_t) {}

inline void RecordAlloc(uintptr_t, size_t) {}
inline void RecordFree(uintptr_t, size_t) {}
inline void PreFree(uintptr_t, size_t) {}

template <typename T>
inline void PoisonPtr(T*&) {}

inline void Init() {}
inline void InitFinish() {}
inline void InitAreas(mm::PageAllocArea*, size_t) {}

#endif // KASAN

}
