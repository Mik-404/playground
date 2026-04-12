.intel_syntax noprefix

.extern _early_entry_point

.section .early.text
.global _fake_multiboot_start
.type _fake_multiboot_start, @function
_fake_multiboot_start:
.code32
#ifdef CONFIG_TEST_MULTIBOOT_FAKE_MAGIC
    mov eax, 0xdeadbeef
    mov ebx, 0xdeadbeef
#endif
    jmp _early_entry_point
