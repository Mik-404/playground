// Original implementation by @pixelindigo: https://github.com/carzil/KeltOS/blob/master/kernel/printk.c

#include <cstdint>

#include "kernel/time.h"
#include "kernel/printk.h"
#include "lib/spinlock.h"
#include "lib/memory.h"


enum { PRINTK_BUF_SIZE = 20 };
static const char digits[] = {'0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'a', 'b', 'c', 'd', 'e', 'f'};


static size_t bprintu64(char* buf, uint64_t a, int base) {
    size_t i;
    size_t bytes;

    if (a == 0) {
        buf[0] = '0';
        return 1;
    }
    for (bytes = 0; a > 0; bytes++) {
        buf[bytes] = digits[a % base];
        a /= base;
    }
    for (i = 0; i < bytes / 2; i++) {
        char tmp = buf[bytes - i - 1];
        buf[bytes - i - 1] = buf[i];
        buf[i] = tmp;
    }
    return bytes;
}

static size_t bprints64(char* buf, int64_t a, int base) {
    if (a < 0) {
        *(buf++) = '-';
        return 1 + bprintu64(buf, -a, base);
    }
    return bprintu64(buf, a, base);
}

static size_t bprintstr(char* buf, const char* str) {
    const char* cur = str;
    size_t length = 0;
    while (*cur) {
        buf[length++] = *(cur++);
    }
    return length;
}

static size_t bprintptr(char* buf, void* ptr) {
    size_t bytes = bprintstr(buf, "0x");
    return bytes + bprintu64(buf + bytes, (uint64_t) ptr, 16);
}

constexpr size_t MAX_PRINTERS = 5;
static size_t printers_size = 0;
static PrintkPrinter printers[MAX_PRINTERS] = {};

void PrintkRegisterPrinter(PrintkPrinter printer) noexcept {
    if (printers_size >= MAX_PRINTERS) {
        panic("trying to register too many printk printers");
    }
    printers[printers_size++] = printer;
}

bool LevelEnabled(LogLevel level, LogLevel min_level) noexcept {
    using T = std::underlying_type_t<LogLevel>;
    return T(min_level) <= T(level);
}

static void Print(LogLevel level, const char* buf, size_t sz) {
    for (size_t i = 0; i < printers_size; i++) {
        if (LevelEnabled(level, printers[i].min_level)) {
            printers[i].print(buf, sz);
        }
    }
}

static size_t bprinterrno(char* buf, int err) {
    switch (err) {
        case kern::ENOSYS.Code():
            return bprintstr(buf, "ENOSYS");
        case kern::ENOMEM.Code():
            return bprintstr(buf, "ENOMEM");
        case kern::EINVAL.Code():
            return bprintstr(buf, "EINVAL");
        case kern::EBADF.Code():
            return bprintstr(buf, "EBADF");
        case kern::EMFILE.Code():
            return bprintstr(buf, "EMFILE");
        case kern::ENOENT.Code():
            return bprintstr(buf, "ENOENT");
        case kern::EISDIR.Code():
            return bprintstr(buf, "EISDIR");
        case kern::EIO.Code():
            return bprintstr(buf, "EIO");
        case kern::EACCES.Code():
            return bprintstr(buf, "EACCES");
        case kern::ECHILD.Code():
            return bprintstr(buf, "ECHILD");
        case kern::EFAULT.Code():
            return bprintstr(buf, "EFAULT");
        case kern::ENOTDIR.Code():
            return bprintstr(buf, "ENOTDIR");
        case kern::ENAMETOOLONG.Code():
            return bprintstr(buf, "ENAMETOOLONG");
        case kern::ENOSPC.Code():
            return bprintstr(buf, "ENOSPC");
        default:
            return bprints64(buf, err, 10);
    }
}

enum class State {
    NextChar = 0,
    Modifier = 1,
    LongNum = 2,
};

static SpinLock PrintkLock;

void vprintk(LogLevel level, const char* fmt, va_list args) noexcept {
    IrqSafeScopeLocker locker(PrintkLock);

    const char* cursor = fmt;
    char* str;
    char buf[PRINTK_BUF_SIZE];

    size_t size = 0;
    State state = State::NextChar;

    while (*cursor) {
        switch (state) {
        case State::NextChar:
            if (*cursor != '%') {
                Print(level, cursor, 1);
            } else {
                state = State::Modifier;
            }
            break;
        case State::Modifier:
            if (*cursor == 'l') {
                state = State::LongNum;
                break;
            }
            switch (*cursor) {
            case 'c':
                buf[0] = (char)va_arg(args, int);
                size = 1;
                break;
            case 'u':
                size = bprintu64(buf, va_arg(args, unsigned), 10);
                break;
            case 'x':
                size = bprintu64(buf, va_arg(args, unsigned), 16);
                break;
            case 'd':
                size = bprints64(buf, va_arg(args, int), 10);
                break;
            case 'p':
                size = bprintptr(buf, va_arg(args, void*));
                break;
            case 'e':
                size = bprinterrno(buf, va_arg(args, int));
                break;
            case 's':
                str = va_arg(args, char*);
                Print(level, str, strlen(str));
                size = 0;
                break;
            default:
                size = 0;
                break;
            }
            if (size) {
                Print(level, buf, size);
            }
            state = State::NextChar;
            break;

        case State::LongNum:
            switch (*cursor) {
            case 'u':
                size = bprintu64(buf, va_arg(args, unsigned long), 10);
                break;
            case 'x':
                size = bprintu64(buf, va_arg(args, unsigned long), 16);
                break;
            case 'd':
                size = bprints64(buf, va_arg(args, long), 10);
                break;
            default:
                size = 0;
                break;
            }
            if (size) {
                Print(level, buf, size);
            }
            state = State::NextChar;
        }
        ++cursor;
    }
}
