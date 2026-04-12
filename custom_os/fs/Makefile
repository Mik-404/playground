MODULE=fs

CPP_SOURCES = \
	block_device.cpp \
	buffer.cpp \
	ext2_common.cpp \
	file_table.cpp \
	inode.cpp \
	page_cache.cpp \
	vfs.cpp \
	writeback.cpp

ifdef CONFIG_COMPILE_STUBS
CPP_SOURCES += ext2_stub.cpp pipe_stub.cpp
else
CPP_SOURCES += ext2.cpp pipe.cpp
endif

include ../build/Makefile.inc
