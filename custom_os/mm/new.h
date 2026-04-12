#pragma once

#include "mm/obj_alloc.h"


void* operator new(size_t size) noexcept;
void* operator new(size_t size, mm::AllocFlags flags) noexcept;

void* operator new[](size_t size) noexcept;
void* operator new[](size_t size, mm::AllocFlags flags) noexcept;

void* operator new(size_t size, mm::ObjectAllocator& alloc) noexcept;
void* operator new(size_t size, mm::ObjectAllocator& alloc, mm::AllocFlags flags) noexcept;

void* operator new[](size_t size, mm::ObjectAllocator& alloc) noexcept;
void* operator new[](size_t size, mm::ObjectAllocator& alloc, mm::AllocFlags flags) noexcept;

void operator delete(void* ptr) noexcept;
void operator delete[](void* ptr) noexcept;
