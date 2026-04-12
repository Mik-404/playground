#pragma once

#include "uapi/time.h"

struct rusage {
    struct timeval ru_utime;
    struct timeval ru_stime;
};
