#pragma once

#include <cstddef>

#include "mm/page_alloc.h"

namespace mm {

void InitKmalloc();
void* Kmalloc(size_t sz, mm::AllocFlags flags);
void Kfree(void* obj);

}
