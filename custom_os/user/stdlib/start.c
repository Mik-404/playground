#include <unistd.h>

extern int main();

void _stdlib_start() {
    _exit(main());
}
