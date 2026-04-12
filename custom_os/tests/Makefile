MODULE=tests

ifdef CONFIG_TEST_LAPIC_TIMER
	CPP_SOURCES = test_lapic_timer.cpp
else ifdef CONFIG_TEST_MULTIBOOT
	CPP_SOURCES = test_multiboot.cpp
	ASM_SOURCES = test_multiboot.S
else ifdef CONFIG_TEST_PAGE_ALLOC_BASIC
	CPP_SOURCES = test_page_alloc_common.cpp test_page_alloc_basic.cpp
else ifdef CONFIG_TEST_PAGE_ALLOC_FRAGMENTATION
	CPP_SOURCES = test_page_alloc_common.cpp test_page_alloc_fragmentation.cpp
else ifdef CONFIG_TEST_SLAB_ALLOC
	CPP_SOURCES = test_slab_alloc.cpp
endif

include ../build/Makefile.inc
