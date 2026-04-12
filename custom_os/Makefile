include ./build/funcs.inc

SHELL := /bin/bash

BUILDDIR=$(ROOT)/build

CCACHE=
AS=x86_64-none-elf-gcc
LD=x86_64-none-elf-ld
OBJCOPY=x86_64-none-elf-objcopy
CXX=$(CCACHE) x86_64-none-elf-g++
CPP=x86_64-none-elf-cpp
SED=sed
GRUB_MKRESCUE=grub-mkrescue
PYTHON=python3

include .gen/Config

ROOT=$(shell pwd)
ASFLAGS += -g -I$(ROOT)
CXXFLAGS += \
	-I$(ROOT) \
	-I$(ROOT)/arch/$(CONFIG_TARGET_ARCH) \
	-I$(ROOT)/cxx \
	-std=c++23 -ffreestanding -fno-exceptions -fno-rtti -fno-use-cxa-atexit \
	-fno-omit-frame-pointer \
	-I/usr/local/boost_1_84_0 -DBOOST_DISABLE_ASSERTS=1 -DBOOST_NO_EXCEPTIONS=1 -DBOOST_EXCEPTION_DISABLE=1 -DBOOST_NO_RTTI=1 -DBOOST_NO_TYPEID=1 \
	-m64 -mno-mmx -mno-sse -mno-sse2 \
	-maddress-mode=long -mcmodel=kernel -mno-red-zone \
	-O$(CONFIG_OPTIMIZE_LEVEL) \
	-g \
	-Wall -Werror -Wextra \
	-Wno-pointer-arith -Wno-unused-function -Wno-changes-meaning -Wno-dangling-pointer \
	-Wno-error=deprecated-declarations
LDFLAGS=-nostdlib --no-dynamic-linker --warn-constructors --warn-common --no-warn-rwx-segment --no-eh-frame-hdr --fatal-warnings -T <($(CPP) -P -E linker.ld)

ifdef CONFIG_KASAN
CXXFLAGS += -fsanitize=kernel-address
endif

export

all: kernel.iso disk.img
.PHONY: all

CONFIGS := $(shell find . -name config.yaml)
.gen/Config: config_values.yaml $(CONFIGS)
	mkdir -p .gen/
	$(PYTHON) build/gen_config.py config_values.yaml $(CONFIGS) > .gen/Config

config: .gen/Config
.PHONY: config

disk.img: config
	$(MAKE) -C user/ TARGET_ARCH=$(CONFIG_TARGET_ARCH)
	genext2fs -d ./user/disk_image -b 10000 -B 4096 disk.img
	chmod 777 disk.img

kernel.iso: kernel.elf
	mkdir -p isodir/boot/grub
	cp grub.cfg isodir/boot/grub
	cp kernel.elf isodir/boot
	$(GRUB_MKRESCUE) -o kernel.iso isodir
	rm -rf isodir

demangle: build/demangle.cpp
	mkdir -p .gen/
	g++ -O2 build/demangle.cpp -o .gen/demangle

sub_arch: config
	@$(MAKE) -C arch/$(CONFIG_TARGET_ARCH)
.PHONY: sub_arch

sub_drivers: config
	@$(MAKE) -C drivers/
.PHONY: sub_drivers

sub_fs: config
	@$(MAKE) -C fs/
.PHONY: sub_fs

sub_kernel: config
	@$(MAKE) -C kernel/
.PHONY: sub_kernel

sub_lib: config
	@$(MAKE) -C lib/
.PHONY: sub_lib

sub_mm: config
	@$(MAKE) -C mm/
.PHONY: sub_mm

sub_tests: config
	@$(MAKE) -C tests/
.PHONY: sub_tests

kernel.elf: config sub_arch sub_drivers sub_fs sub_kernel sub_lib sub_mm sub_tests demangle
	mkdir -p .gen/

	@$(call ld_colored,$(LDFLAGS) `find $(ROOT) -name '*.o' -not -path '$(ROOT)/user/*'` -o kernel.bin)

	@$(PYTHON) build/extract_symbols.py kernel.bin .gen/demangle > .gen/ksyms.S
	@$(call as_colored,$(ASFLAGS) -c .gen/ksyms.S -o .gen/ksyms.S.o)

	@$(call ld_colored,$(LDFLAGS) `find $(ROOT) -name '*.o' -not -path '$(ROOT)/user/*'` -o kernel.bin)

	@$(PYTHON) build/extract_symbols.py kernel.bin .gen/demangle > .gen/ksyms.S
	@$(call as_colored,$(ASFLAGS) -c .gen/ksyms.S -o .gen/ksyms.S.o)

	@$(call ld_colored,$(LDFLAGS) `find $(ROOT) -name '*.o' -not -path '$(ROOT)/user/*'` -o kernel.elf)
	@rm -f kernel.bin

	@$(PYTHON) build/build_syscalls.py kernel.elf

	@$(OBJCOPY) --only-keep-debug kernel.elf kernel.sym
	@$(OBJCOPY) --strip-debug kernel.elf

qemu:
	qemu-system-x86_64 -smp cores=4,threads=1,sockets=1 -m 512M -cdrom kernel.iso -hda disk.img -no-reboot -serial stdio -monitor null -nographic -s

qemu-gdb:
	qemu-system-x86_64 -smp cores=4,threads=1,sockets=1 -m 512M -cdrom kernel.iso -hda disk.img -no-reboot -serial stdio -monitor null -nographic -s -S

clean:
	@$(MAKE) -C arch/ clean
	@$(MAKE) -C drivers/ clean
	@$(MAKE) -C fs/ clean
	@$(MAKE) -C kernel/ clean
	@$(MAKE) -C lib/ clean
	@$(MAKE) -C mm/ clean
	@$(MAKE) -C user/ clean
	@$(MAKE) -C tests/ clean
	@rm -f kernel.elf
	@rm -f kernel.sym
	@rm -f kernel.iso
	@rm -f disk.img
	@rm -rf .gen
.PHONY: clean
