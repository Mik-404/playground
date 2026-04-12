#include <string.h>

int strncmp(const char* a, const char* b, size_t sz) {
    while (*a != '\0' && *b != '\0' && sz > 0) {
        if (*a != *b) {
            break;
        }
        a++;
        b++;
        sz--;
    }
    if (sz == 0) {
        return 0;
    }
    return *a - *b;
}

size_t strlen(const char* str) {
	size_t len = 0;
	while (str[len]) {
		len++;
    }
	return len;
}
