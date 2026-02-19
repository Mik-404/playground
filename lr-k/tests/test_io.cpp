#include "test.hpp"
#include "connector.hpp"
#include "runtime_parser.hpp"
#include <gtest/gtest.h>
#include <sstream>

using K0Fixture = GrammarAnalysisFixture<std::integral_constant<int, 0>>;
using K1Fixture = GrammarAnalysisFixture<std::integral_constant<int, 1>>;
using K6Fixture = GrammarAnalysisFixture<std::integral_constant<int, 6>>;
using ::testing::ElementsAreArray;

template <int Lookahead>
void check_load (RuntimeParser<Lookahead>& parser, Builder<Lookahead>& builder, Grammar& grammar) {
    EXPECT_EQ(parser.tokenizer._id2view, grammar._id2view);
    EXPECT_EQ(parser.tokenizer.prod.size(), grammar.numbered_prod.size());
    for (uint32_t index = 0; index < parser.tokenizer.prod.size(); index++) {
        EXPECT_EQ(parser.tokenizer.prod[index].first, 
                            grammar.numbered_prod[index].first);
        EXPECT_THAT(parser.tokenizer.prod[index].second,
                    ::ElementsAreArray(grammar._rhs[grammar.numbered_prod[index].second]));
    }
    EXPECT_EQ(parser.action_, builder.action_);    
    EXPECT_EQ(parser.goto_, builder.goto_);    
}

TEST_F (K6Fixture, IOTest1) {
    load_obj("lr-6_grammar.txt");
    temp_anal->build_kfirst();
    constexpr int Lookahead = 6;

    Builder<Lookahead> builder (temp_grammar.value(), temp_anal.value());
    builder.build();
    Exporter exporter(temp_grammar.value());
    std::stringstream pipe_channel;
    exporter.serialize<Lookahead>(builder, pipe_channel);
    Importer importer;
    RuntimeParser<Lookahead> parser;
    importer.load<Lookahead> (parser, pipe_channel);
    check_load (parser, builder, temp_grammar.value());
}

TEST_F (K1Fixture, IOTest2) {
    load_obj("base_grammar.txt");
    temp_anal->build_kfirst();
    constexpr int Lookahead = 1;

    Builder<Lookahead> builder (temp_grammar.value(), temp_anal.value());
    builder.build();
    
    Exporter exporter(temp_grammar.value());
    std::stringstream pipe_channel;
    exporter.serialize<Lookahead>(builder, pipe_channel);
    Importer importer;
    RuntimeParser<Lookahead> parser;
    importer.load<Lookahead>(parser, pipe_channel);
    check_load (parser, builder, temp_grammar.value());
}

TEST_F (K1Fixture, IOTest3) {
    load_obj("lr-0_grammar.txt");
    temp_anal->build_kfirst();
    constexpr int Lookahead = 1;

    Builder<Lookahead> builder (temp_grammar.value(), temp_anal.value());
    builder.build();

    Exporter exporter(temp_grammar.value());
    std::stringstream pipe_channel;
    exporter.serialize<Lookahead>(builder, pipe_channel);

    Importer importer;
    RuntimeParser<0> parser;
    
    EXPECT_ANY_THROW (importer.load<0>(parser, pipe_channel));
}

TEST_F (K0Fixture, IOTest4) {
    load_obj("lr-0_grammar.txt");
    temp_anal->build_kfirst();
    constexpr int Lookahead = 0;

    Builder<Lookahead> builder (temp_grammar.value(), temp_anal.value());
    builder.build();
    
    Exporter exporter(temp_grammar.value());
    std::stringstream pipe_channel;
    exporter.serialize<Lookahead>(builder, pipe_channel);
    Importer importer;
    RuntimeParser<Lookahead> parser;
    importer.load<Lookahead>(parser, pipe_channel);
    check_load (parser, builder, temp_grammar.value());
}

TEST_F (K1Fixture, IOTest5) {
    load_obj("base_grammar.txt");
    temp_anal->build_kfirst();
    constexpr int Lookahead = 1;

    Builder<Lookahead> builder (temp_grammar.value(), temp_anal.value());
    builder.build();
    
    Exporter exporter(temp_grammar.value());
    std::stringstream pipe_channel;
    exporter.serialize<Lookahead>(builder, pipe_channel);
    Importer importer;
    RuntimeParser<Lookahead> parser;
    importer.load<Lookahead>(parser, pipe_channel);
    check_load (parser, builder, temp_grammar.value());
}