#pragma once

#include "defs.h"

#define PER_CPU(varname) %gs:__per_cpu_##varname
