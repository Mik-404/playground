#include "test.hpp"
#include <gtest/gtest.h>
#include <ranges>


using K0Fixture = GrammarAnalysisFixture<std::integral_constant<int, 0>>;
using K1Fixture = GrammarAnalysisFixture<std::integral_constant<int, 1>>;
using K2Fixture = GrammarAnalysisFixture<std::integral_constant<int, 2>>;
using K5Fixture = GrammarAnalysisFixture<std::integral_constant<int, 5>>;
using K6Fixture = GrammarAnalysisFixture<std::integral_constant<int, 6>>;
using ::testing::UnorderedElementsAre;


TEST_F (K1Fixture, ClosureTest1) {
    load_obj("base_grammar.txt");
    temp_anal->build_kfirst();
    Builder<1> builder (temp_grammar.value(), temp_anal.value());

    auto correct_item1 = Item<1>(temp_grammar->productions.find(temp_grammar->start)->second, KString<1>(), 0);
    auto correct_item2 = Item<1>(0, KString<1>(), 0);
    auto correct_item3 = Item<1>(1, KString<1>(), 0);
    auto correct_item4 = Item<1>(0, KString<1>(), 4);

    auto closure = builder.closure({correct_item1});
    EXPECT_EQ(closure.size(), 3);
    EXPECT_THAT(closure, UnorderedElementsAre(correct_item1, correct_item2, correct_item3));
    closure = builder.closure({});
    EXPECT_EQ(closure.size(), 0);

    closure = builder.closure({correct_item4});
    EXPECT_EQ(closure.size(), 1);

    KString<Lookahead> lookahead1;
    lookahead1.len = 1;
    lookahead1.str[0] = temp_grammar->_view2id["b"];
    
    auto correct_item5 = Item<Lookahead>(0, KString<Lookahead>(), 1);
    auto correct_item6 = Item<Lookahead>(0, lookahead1, 0);
    auto correct_item7 = Item<Lookahead>(1, lookahead1, 0);

    closure = builder.closure({correct_item5});
    EXPECT_EQ(closure.size(), 3);
    EXPECT_THAT(closure, UnorderedElementsAre(correct_item5, 
                    correct_item6, correct_item7));
}

TEST_F (K2Fixture, ClosureTest2) {
    load_obj("lr-2_grammar.txt");
    temp_anal->build_kfirst();
    constexpr int Lookahead = 2;
    Builder<Lookahead> builder (temp_grammar.value(), temp_anal.value());
    KString<Lookahead> lookahead1;
    lookahead1.len = 2;
    lookahead1.str[0] = temp_grammar->_view2id["x"], lookahead1.str[1] = temp_grammar->_view2id["x"];
    auto correct_item1 = Item<Lookahead>(temp_grammar->productions.find(temp_grammar->start)->second, KString<Lookahead>(), 0);
    auto correct_item2 = Item<Lookahead>(0, KString<Lookahead>(), 0);
    auto correct_item3 = Item<Lookahead>(1, KString<Lookahead>(), 0);
    auto correct_item4 = Item<Lookahead>(2, lookahead1, 0);
    auto correct_item5 = Item<Lookahead>(3, lookahead1, 0);

    std::deque<Item<Lookahead>> kernel = {correct_item1};
    auto closure = builder.closure(kernel);
    EXPECT_EQ(closure.size(), 5);
    EXPECT_THAT(closure, UnorderedElementsAre(correct_item1, correct_item2,
                    correct_item3, correct_item4, correct_item5));
}

TEST_F (K6Fixture, ClosureTest3) {
    load_obj("lr-6_grammar.txt");
    temp_anal->build_kfirst();
    constexpr int Lookahead = 6;

    std::vector <int> correct_id = {6,0,1,2,4,3,5,2,4,3,5,2,4,3,5,2,4,3,5};
    std::vector<std::vector<std::string>> correct_lookaheads = {
        {}, {}, {},
        {"c", "c", "c", "c", "c", "d"},
        {"c", "c", "c", "c", "c", "d"},
        {"c", "c", "c", "c", "c", "e"},
        {"c", "c", "c", "c", "c", "e"},
        {"+", "n", "c", "c", "c", "c"},
        {"+", "n", "c", "c", "c", "c"},
        {"*", "n", "c", "c", "c", "c"},
        {"*", "n", "c", "c", "c", "c"},
        {"+", "n", "+", "n", "c", "c"},
        {"+", "n", "+", "n", "c", "c"},
        {"*", "n", "*", "n", "c", "c"},
        {"*", "n", "*", "n", "c", "c"},
        {"+", "n", "+", "n", "+", "n"},
        {"+", "n", "+", "n", "+", "n"},
        {"*", "n", "*", "n", "*", "n"},
        {"*", "n", "*", "n", "*", "n"}
    };
    std::vector <Item<Lookahead>> correct_closure;
    for (int i = 0 ; i < correct_id.size(); i++) {
        KString<Lookahead> lookahead;
        lookahead.len = correct_lookaheads[i].size();
        for (int index = 0; index < correct_lookaheads[i].size(); index++) {
            lookahead.str[index] = temp_grammar->_view2id[correct_lookaheads[i][index]];
        }
        correct_closure.emplace_back(correct_id[i], lookahead, 0);
    }

    Builder<Lookahead> builder (temp_grammar.value(), temp_anal.value());

    std::deque<Item<Lookahead>> kernel = {correct_closure[0]};
    auto closure = builder.closure(kernel);
    EXPECT_EQ(closure.size(), 19);
    EXPECT_THAT(closure, UnorderedElementsAreArray(correct_closure));
}

TEST_F (K1Fixture, ClosureTest4) {
    load_obj("special_grammar.txt");
    temp_anal->build_kfirst();
    Builder<1> builder (temp_grammar.value(), temp_anal.value());

    auto correct_item1 = Item<1>(temp_grammar->productions.find(temp_grammar->start)->second, KString<1>(), 0);
    auto correct_item2 = Item<1>(0, KString<1>(), 0);
    auto correct_item3 = Item<1>(1, KString<1>(), 0);

    auto closure = builder.closure({correct_item1});
    EXPECT_EQ(closure.size(), 3);
    EXPECT_THAT(closure, UnorderedElementsAre(correct_item1, correct_item2, correct_item3));
}


TEST_F (K1Fixture, BuildTest1) {
    load_obj("base_grammar.txt");
    temp_anal->build_kfirst();
    constexpr int Lookahead = 1;

    Builder<Lookahead> builder (temp_grammar.value(), temp_anal.value());
    builder.build();
    print_tables(builder);
}

TEST_F (K5Fixture, BuildTest2) {
    load_obj("lr-6_grammar.txt");
    temp_anal->build_kfirst();
    constexpr int Lookahead = 5;

    Builder<Lookahead> builder (temp_grammar.value(), temp_anal.value());
    EXPECT_ANY_THROW(builder.build());
}

TEST_F (K6Fixture, BuildTest3) {
    load_obj("lr-6_grammar.txt");
    temp_anal->build_kfirst();
    constexpr int Lookahead = 6;

    Builder<Lookahead> builder (temp_grammar.value(), temp_anal.value());
    builder.build();
    print_tables(builder);
}

TEST_F (K1Fixture, BuildTest4) {
    load_obj("lr-0_grammar.txt");
    temp_anal->build_kfirst();
    constexpr int Lookahead = 1;

    Builder<Lookahead> builder (temp_grammar.value(), temp_anal.value());
    builder.build();
    print_tables(builder);
}

TEST_F (K0Fixture, BuildTest5) {
    load_obj("lr-0_grammar.txt");
    temp_anal->build_kfirst();
    constexpr int Lookahead = 0;

    Builder<Lookahead> builder (temp_grammar.value(), temp_anal.value());
    builder.build();
    print_tables(builder); 
}

TEST_F (K0Fixture, BuildTest6) {
    load_obj("base_grammar.txt");
    temp_anal->build_kfirst();
    constexpr int Lookahead = 0;

    Builder<Lookahead> builder (temp_grammar.value(), temp_anal.value());
    EXPECT_ANY_THROW(builder.build());
}