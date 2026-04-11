#include <gtest/gtest.h>
#include <Foo.h>

TEST(A, B) {
    Foo();
    ASSERT_EQ(4, 2 + 2);
}
