CC=x86_64-none-elf-gcc
LD=x86_64-none-elf-ld
OBJCOPY=x86_64-none-elf-objcopy
CCFLAGS=-Istdlib/ -O2 -mno-mmx -mno-sse -mno-sse2 -g -m64 -std=c11 -ffreestanding -nostdlib -fno-common -Wall -Werror -Wextra -Wno-unused-function -static-libgcc
LDFLAGS=-nostdlib --no-dynamic-linker --warn-constructors --warn-common --no-warn-rwx-segment --no-eh-frame-hdr --fatal-warnings -T script.ld

TESTS_SOURCES := $(wildcard tests/*.c)
TESTS_HEADERS := $(wildcard tests/*.c)

IMAGE_DIR=disk_image

all: $(IMAGE_DIR)/bin/run_tests

image_dir:
	mkdir -p $(IMAGE_DIR)
	mkdir -p $(IMAGE_DIR)/bin
	mkdir -p $(IMAGE_DIR)/etc

stdlib:
	make -C stdlib/

$(IMAGE_DIR)/bin/run_tests: image_dir stdlib
	$(MAKE) -C tests/
	cp -r tests/testdata $(IMAGE_DIR)/etc
	cp -r tests/gentestdata $(IMAGE_DIR)/etc
	@$(OBJCOPY) --only-keep-debug tests/run_tests tests/run_tests.sym
	@$(OBJCOPY) --strip-debug tests/run_tests
	mv tests/run_tests $(IMAGE_DIR)/bin

clean:
	rm -rf *.o
	rm -rf tests/*.o
	rm -rf $(IMAGE_DIR)
	$(MAKE) -C stdlib/ clean
	$(MAKE) -C tests/ clean

.PHONY: image_dir clean stdlib
