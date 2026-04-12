#pragma once

#include "fs/vfs.h"
#include "mm/membuf.h"

namespace kern {

kern::Errno Execve(vfs::PathPtr path, const mm::MemBuf& argv, const mm::MemBuf& env) noexcept;

}
