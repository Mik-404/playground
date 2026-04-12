include $(BUILDDIR)/funcs.inc

SHELL := /bin/bash

ASM_OBJS := $(ASM_SOURCES:.S=.S.o)
CPP_OBJS := $(CPP_SOURCES:.cpp=.cpp.o)

build: $(C_OBJS) $(CPP_OBJS) $(ASM_OBJS)
.PHONY: build

%.S.o: %.S
	@$(call as_colored,$(ASFLAGS) -c $< -o $@)

%.cpp.o: %.cpp
	@$(call cxx_colored,$(CXXFLAGS) -c $< -o $@)

clean:
	@rm -f *.o
.PHONY: clean
