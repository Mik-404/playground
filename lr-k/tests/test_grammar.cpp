#include <gtest/gtest.h>
#include "test.hpp"

using ::testing::ElementsAre;
using ::testing::UnorderedElementsAre;

TEST_F(ParserFixture, CorrectParsing1)
{
    load_parser("base_grammar.txt");
    ASSERT_EQ(temp_parser->productions.size(), 3);
    int index_start = temp_parser->_view2id["S"];
    auto [start, end] = temp_parser->productions.equal_range(index_start);
    bool flag1 = false, flag2 = false;
    for (;start!=end;start++) {
        int rhs_id = temp_parser->numbered_prod[start->second].second;
        if (temp_parser->_rhs[rhs_id].empty()) {
            flag1 = true;
        } else if (temp_parser->_rhs[rhs_id].size() == 4) {
            flag2 = true;
            EXPECT_THAT (temp_parser->_rhs[rhs_id], ElementsAre(1, 0, 2, 0));
        } else {
            EXPECT_TRUE(false);
        }
    }
    EXPECT_TRUE(flag1 && flag2);
}

TEST_F(ParserFixture, InCorrectParsing1)
{
    EXPECT_THROW (load_parser("broken_grammar1.txt"), std::runtime_error);
}

TEST_F(ParserFixture, InCorrectParsing2)
{
    EXPECT_THROW (load_parser("broken_grammar2.txt"), std::runtime_error);
}

TEST_F(GrammarFixture, CorrectGrammarBuild1)
{
    load_grammar("base_grammar.txt");
    ASSERT_EQ(temp_grammar->productions.size(), 4);
    bool flag1 = false, flag2 = false, flag3 = false;
    for (auto iter : temp_grammar->productions) {
        int rhs_id = temp_grammar->numbered_prod[iter.second].second;
        if (temp_grammar->_rhs[rhs_id].empty()) {
            flag1 = true;
        } else if (temp_grammar->_rhs[rhs_id].size() == 4) {
            flag2 = true;
            EXPECT_THAT (temp_grammar->_rhs[rhs_id], ElementsAre(1, 0, 2, 0));
        } else if (temp_grammar->_rhs[rhs_id].size() == 1) {
            flag3 = true;
            EXPECT_THAT (temp_grammar->_rhs[rhs_id], ElementsAre(0));
            EXPECT_EQ (iter.first, 4);
        } else {
            EXPECT_TRUE(false);
        }
    }
    EXPECT_TRUE(flag1 && flag2 && flag3);
    EXPECT_EQ (temp_grammar->nonterminals.size(), 3);
    EXPECT_THAT (temp_grammar->nonterminals, UnorderedElementsAre(0, 3, 4));
}

TEST_F(GrammarFixture, InCorrectGrammarBuild1)
{
    EXPECT_THROW (load_grammar("broken_grammar3.txt"), std::runtime_error);
}