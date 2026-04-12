#pragma once

#ifndef __cplusplus

#include <stdatomic.h>
#include <cstdint>

typedef _Atomic uint64_t atomic_uint64_t;
typedef _Atomic uint32_t atomic_uint32_t;

#else

#include <atomic>

using atomic_uint64_t = std::atomic<uint64_t>;
using atomic_uint32_t = std::atomic<uint32_t>;

using std::atomic_fetch_add;
using std::atomic_fetch_sub;
using std::atomic_fetch_and;
using std::atomic_fetch_or;

#endif
