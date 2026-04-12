AS=x86_64-none-elf-gcc
AR=x86_64-none-elf-ar
CC=x86_64-none-elf-gcc
LD=x86_64-none-elf-ld
OBJCOPY=x86_64-none-elf-objcopy
ASFLAGS=-I./ -g
CCFLAGS=-I./ -O2 -mcmodel=large -mno-mmx -mno-sse -mno-sse2 -g -m64 -std=c11 -ffreestanding -nostdlib -fno-common -Wall -Werror -Wextra -Wno-unused-function -static-libgcc
LDFLAGS=-nostdlib --no-dynamic-linker --warn-constructors --warn-common --no-warn-rwx-segment --no-eh-frame-hdr --fatal-warnings -T user_linker.ld

TARGET_ARCH=x86

C_SOURCES   := $(wildcard *.c) $(wildcard sys/*.c)
ASM_SOURCES := $(wildcard arch/$(TARGET_ARCH)/*.S)
C_OBJS      := $(C_SOURCES:.c=.c.o)
ASM_OBJS    := $(ASM_SOURCES:.S=.S.o)

libstd.a: $(C_OBJS) $(ASM_OBJS)
	$(AR) r libstd.a $(C_OBJS) $(ASM_OBJS)

%.c.o: %.c
	$(CC) $(CCFLAGS) -c $< -o $@

sys/%.c.o: sys/%.c
	$(CC) $(CCFLAGS) -c $< -o $@

arch/$(TARGET_ARCH)/%.S.o: arch/$(TARGET_ARCH)/%.S
	$(AS) $(ASFLAGS) -c $< -o $@

clean:
	rm -f *.o
	rm -f sys/*.o
	rm -f *.a

.PHONY: clean
