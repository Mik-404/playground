MODULE=mm

CPP_SOURCES := \
	kmalloc.cpp \
	membuf.cpp \
	new.cpp \
	page_alloc_common.cpp \
	vmem_common.cpp

ifdef CONFIG_COMPILE_STUBS
CPP_SOURCES += \
	vmem_stub.cpp \
	obj_alloc_stub.cpp \
	page_alloc_stub.cpp

else

CPP_SOURCES += \
	vmem.cpp \
	obj_alloc.cpp \
	page_alloc.cpp
endif

ifdef CONFIG_KASAN
CPP_SOURCES += kasan.cpp
endif

include ../build/Makefile.inc

ifdef CONFIG_KASAN
kasan.cpp.o: kasan.cpp
	@$(call cxx_colored,$(CXXFLAGS) -fno-sanitize=kernel-address -c $< -o $@)
endif
