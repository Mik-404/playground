MODULE=kernel

CPP_SOURCES = \
	clone.cpp \
	exec.cpp \
	irq.cpp \
	kernel_thread.cpp \
	ksyms.cpp \
	main.cpp \
	multiboot_common.cpp \
	panic.cpp \
	per_cpu.cpp \
	printk.cpp \
	sched.cpp \
	signal.cpp \
	syscall.cpp \
	time.cpp \
	wait.cpp

ifdef CONFIG_COMPILE_STUBS
	CPP_SOURCES += multiboot_stub.cpp
else
	CPP_SOURCES += multiboot.cpp
endif

include ../build/Makefile.inc
