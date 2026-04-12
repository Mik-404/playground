AS=x86_64-none-elf-gcc
AR=x86_64-none-elf-ar
CC=x86_64-none-elf-gcc
LD=x86_64-none-elf-ld
OBJCOPY=x86_64-none-elf-objcopy
PYTHON=python3
ASFLAGS=-I./ -g

BASE_CCFLAGS=-I./ -I../stdlib -O2 -g -m64 -std=c11 -ffreestanding -nostdlib -fno-common -Wall -Werror -Wextra -Wno-unused-function -static-libgcc
CCFLAGS=$(BASE_CCFLAGS) -mno-mmx -mno-sse -mno-sse2

LDFLAGS=-nostdlib --no-dynamic-linker --warn-constructors --warn-common --no-warn-rwx-segment --no-eh-frame-hdr --fatal-warnings -T ../script.ld

TARGET_ARCH=x86

C_SOURCES := main.c

ifdef TEST_FPU
C_SOURCES += tests_fpu.c
endif

ifdef TEST_MMAP
C_SOURCES += tests_mmap.c
endif

ifdef TEST_PIPE
C_SOURCES += tests_pipe.c
endif

ifdef TEST_SIGNAL
C_SOURCES += tests_signal.c
endif

ifdef TEST_FILE_WRITE
C_SOURCES += tests_file_write.c
endif

ifdef TEST_FORK
C_SOURCES += tests_fork_wait.c
endif

ifdef TEST_FILE
C_SOURCES += tests_file.c
endif

ifdef TEST_WAIT
C_SOURCES += tests_wait4.c
endif

C_OBJS := $(C_SOURCES:.c=.c.o)

run_tests: $(C_OBJS) gentestdata
	$(LD) $(LDFLAGS) $(C_OBJS) ../stdlib/libstd.a -o run_tests

%.c.o: %.c
	$(CC) $(CCFLAGS) -c $< -o $@

ifdef TEST_FPU
tests_fpu.c.o: tests_fpu.c
	$(CC) $(BASE_CCFLAGS) -c tests_fpu.c -o tests_fpu.c.o
endif

gentestdata:
	mkdir -p gentestdata
	$(PYTHON) gen_testdata.py
.PHONY: gentestdata

clean:
	rm -f *.o
	rm -f *.a
.PHONY: clean
