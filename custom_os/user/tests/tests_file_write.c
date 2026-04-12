#include "common.h"

#include <unistd.h>
#include <sys/wait.h>
#include <string.h>
#include <fcntl.h>


TEST(write_file_basic) {
    int fd = ASSERT_NO_ERR(open("/etc/test1", O_RDWR | O_CREAT, 0777));

    char buf[5000];
    for (int i = 0; i < 5000; i++) {
        buf[i] = 'A' + i % 26;
    }

    ssize_t res = ASSERT_NO_ERR(write(fd, buf, 5000));
    ASSERT_MSG(res == 5000, "wrote less than 5000 bytes, res: %ld", res);

    int fd2 = ASSERT_NO_ERR(open("/etc/test1", O_RDONLY));
    res = ASSERT_NO_ERR(read(fd2, buf, 5000));
    ASSERT(res == 5000);
    for (int i = 0; i < 5000; i++) {
        ASSERT(buf[i] == 'A' + i % 26);
    }
    close(fd);
    close(fd2);
}
