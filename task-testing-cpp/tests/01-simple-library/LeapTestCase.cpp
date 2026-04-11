//
// Created by akhtyamovpavel on 5/1/20.
//

#include "LeapTestCase.h"

#include <Functions.h>


TEST(OHHTHISDAYS, test2) {
    EXPECT_ANY_THROW(GetMonthDays(2, 15));
    EXPECT_ANY_THROW(GetMonthDays(2, -1));

    EXPECT_EQ(GetMonthDays(1930, 2), 30);
    EXPECT_ANY_THROW(GetMonthDays(-1, 2));

    EXPECT_EQ(GetMonthDays(5, 2), 28);
    EXPECT_EQ(GetMonthDays(4, 2), 29);

    EXPECT_EQ(GetMonthDays(2, 4), 30);
    EXPECT_EQ(GetMonthDays(2, 5), 31);
}

TEST(BYTHELIPS, test3) {
    EXPECT_EQ(IsLeap(200), false);
    EXPECT_EQ(IsLeap(400), true);
}
