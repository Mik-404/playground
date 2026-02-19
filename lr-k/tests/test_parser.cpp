#include "test.hpp"
#include "connector.hpp"
#include "runtime_parser.hpp"
#include <gtest/gtest.h>
#include <sstream>

using K0Fixture = GrammarAnalysisFixture<std::integral_constant<int, 0>>;
using K1Fixture = GrammarAnalysisFixture<std::integral_constant<int, 1>>;
using K6Fixture = GrammarAnalysisFixture<std::integral_constant<int, 6>>;
using ::testing::ElementsAreArray;
using ::testing::ElementsAre;

TEST_F (K1Fixture, ParsingTest1) {
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
    importer.load<Lookahead> (parser, pipe_channel);
    auto derivation = parser.parse("a a b a b a a b b b a b");
    EXPECT_THAT (derivation, ElementsAre(1, 1, 1, 1, 0, 1, 0, 0, 0, 1, 1, 0, 0));
    EXPECT_ANY_THROW (parser.parse("a b b a b a"));
    EXPECT_ANY_THROW (parser.parse("a b a b a"));
}