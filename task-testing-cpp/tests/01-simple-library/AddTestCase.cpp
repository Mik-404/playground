//
// Created by akhtyamovpavel on 5/1/20.
//

#include "AddTestCase.h"
#include "Functions.h"

TEST(IWANNABREAKTHISADD, test1) {
    EXPECT_EQ(Add(12, 23), 35);
    EXPECT_EQ(Add(12, -12), 0);
}
