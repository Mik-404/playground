#include <gtest/gtest.h>
#include "test.hpp"

using ::testing::ElementsAre;

using K3Fixture = GrammarAnalysisFixture<std::integral_constant<int, 3>>;
using K6Fixture = GrammarAnalysisFixture<std::integral_constant<int, 6>>;

TEST_F (K3Fixture, FirstKTest1) {
    load_obj("lr-3_grammar.txt");
    temp_anal->build_kfirst();
    data correct_first = {
        {"C", {{"c", "d", "c"}, {"d"}}},
        {"B", {{"b", "b"}, {}}},
        {"A", {{"a"}, {}}},
        {"S", {{"a", "b", "b"}, {"a", "c", "d"}, {"a", "d"}, {"b", "b", "c"}, {"b", "b", "d"}, {"c", "d", "c"}, {"d"}}}
    };
    check_first(correct_first);
}

TEST_F (K6Fixture, FirstKTest2) {
    load_obj("lr-6_hard_grammar.txt");
    temp_anal->build_kfirst();
    data correct_first = {
        {"L", {{"1", "2", "1", "2", "1", "2"}}},
        {"R", {{"y", "z"},{"x", "y", "z"},{"x", "x", "y", "z"},
            {"x", "x", "x", "y", "z"}, {"x", "x", "x", "x", "y", "z"},
            {"x", "x", "x", "x", "x", "y"}, {"x", "x", "x", "x", "x", "x"}}},
        {"S", {{"1", "2", "1", "2", "1", "2"}, {"y", "z"},{"x", "y", "z"},
            {"x", "x", "y", "z"},{"x", "x", "x", "y", "z"}, {"x", "x", "x", "x", "y", "z"},
            {"x", "x", "x", "x", "x", "y"}, {"x", "x", "x", "x", "x", "x"}}}
    };
    check_first(correct_first);
}