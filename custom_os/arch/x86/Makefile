MODULE=arch-x86

CPP_SOURCES = \
	early.cpp \
	exc_entry.cpp \
	fault.cpp \
	init.cpp \
	lapic.cpp \
	rtc.cpp \
	serial.cpp \
	stack_unwind.cpp \
	thread.cpp \
	vga.cpp

ifdef CONFIG_COMPILE_STUBS
CPP_SOURCES += signal_stub.cpp
else
CPP_SOURCES += signal.cpp
endif

ASM_SOURCES = \
	early.S \
	exc_entry.S \
	multiboot.S \
	syscall.S \
	thread.S \
	user.S

include ../../build/Makefile.inc
