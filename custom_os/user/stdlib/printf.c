#include <stddef.h>
#include <stdint.h>
#include <stdarg.h>

#include <unistd.h>
#include <string.h>


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

typedef enum {
    NEXT_CHAR = 0,
    MODIFIER = 1,
    LONG_NUM = 2,
} state_t;

void _do_vprintf(const char* fmt, va_list args) {
    const char* cursor = fmt;
    char* str;
    char buf[PRINTK_BUF_SIZE];

    size_t size = 0;
    state_t state = NEXT_CHAR;
    const char* chunk_start = NULL;

    while (*cursor) {
        switch (state) {
        case NEXT_CHAR:
            if (*cursor == '%') {
                if (cursor - chunk_start > 0) {
                    write(1, chunk_start, cursor - chunk_start);
                    chunk_start = 0;
                }
                state = MODIFIER;
                break;
            }
            if (chunk_start == NULL) {
                chunk_start = cursor;
            }
            break;
        case MODIFIER:
            if (*cursor == 'l') {
                state = LONG_NUM;
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
            case 's':
                str = va_arg(args, char*);
                write(1, str, strlen(str));
                size = 0;
                break;
            default:
                size = 0;
                break;
            }
            if (size) {
                write(1, buf, size);
            }
            state = NEXT_CHAR;
            break;

        case LONG_NUM:
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
                write(1, buf, size);
            }
            state = NEXT_CHAR;
        }
        ++cursor;
    }

    if (chunk_start != NULL && cursor - chunk_start > 0) {
        write(1, chunk_start, cursor - chunk_start);
    }
}

void vprintf(const char* fmt, va_list args) {
    _do_vprintf(fmt, args);
}

void printf(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);
}
