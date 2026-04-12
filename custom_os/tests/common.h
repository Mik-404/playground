#include "kernel/panic.h"
#include "kernel/printk.h"

#define FAIL(msg, ...) { printk("[test] fail: " msg "\n" __VA_OPT__(,) __VA_ARGS__ ); panic(""); }
#define FAIL_ON_NULL(ptr, msg, ...) { if (!(ptr)) { FAIL(msg __VA_OPT__(,) __VA_ARGS__) } }
#define FAIL_ON(expr, msg, ...) if (expr) { FAIL(msg __VA_OPT__(,) __VA_ARGS__) }
#define FAIL_IF_UNALIGNED(ptr, align, msg, ...) if ((uintptr_t)(ptr) % align != 0) { FAIL(msg __VA_OPT__(,) __VA_ARGS__) }
#define FAIL_ON_INVALID_PTR(ptr) { \
    if (ptr < (void*)KERNEL_DIRECT_PHYS_MAPPING_START || ptr > (void*)KERNEL_DIRECT_PHYS_MAPPING_START + KERNEL_DIRECT_PHYS_MAPPING_SIZE) { \
        FAIL("allocator returned invalid pointer %p\n", ptr); \
    } \
}
#define TEST_OK() { x86::Outw(0x501, 61); panic("test ended"); }
