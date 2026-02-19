#pragma once

#include <optional>
#include <string>
#include <filesystem>
#include <fstream>
#include <gmock/gmock.h>
#include "grammar.hpp"
#include "first_k.hpp"
#include "builder.hpp"



#ifndef TEST_DATA_DIR
#define TEST_DATA_DIR "."
#endif

constexpr bool PRINT_TABLES = false;

using ::testing::UnorderedElementsAreArray;

namespace TestUtils {
    inline std::string read_file (const std::string& filename) {
        std::filesystem::path fullPath = std::filesystem::path(TEST_DATA_DIR) / filename;
        std::ifstream file(fullPath, std::ios::in | std::ios::binary);
        if (!file) throw std::runtime_error("Fail");

        const int sz = std::filesystem::file_size(fullPath);
        std::string text (sz,'\0');
        file.read(text.data(), sz);
        return text;
    }
};

class ParserFixture : public testing::Test {
protected:
    void load_parser(const std::string& filename) {
        std::string text = TestUtils::read_file(filename);

        temp_parser.emplace(std::move(text));
    }
    
    std::optional<Parser> temp_parser;
};

class GrammarFixture : public testing::Test {
protected:
    void load_grammar(const std::string& filename) {
        std::filesystem::path fullPath = std::filesystem::path(TEST_DATA_DIR) / filename;
        temp_grammar.emplace(fullPath);
    }
    
    std::optional<Grammar> temp_grammar;
};

template <typename T>
class GrammarAnalysisFixture : public testing::Test {
protected:
    using data = std::map<std::string, std::vector<std::vector<std::string>>>;
    
    static constexpr int Lookahead = T::value;

    void load_obj(const std::string& filename) {
        std::filesystem::path fullPath = std::filesystem::path(TEST_DATA_DIR) / filename;
        temp_grammar.emplace(fullPath);
        temp_anal.emplace(temp_grammar.value());
    }

    void check_first(data correct_first) {
        for (auto nt: correct_first) {
            int nt_id = temp_grammar->_view2id[nt.first];
            std::vector<KString<Lookahead>> lookahead;
            for (auto we_have_kstring_at_home : nt.second) {
                
                // std::cout << nt_id << " ------\n";

                KString<Lookahead> result;
                for (auto token : we_have_kstring_at_home) {
                    result = result + KString<Lookahead>(temp_grammar->_view2id[token]);
                    // for (int i = 0; i < Lookahead; i++) {
                    //     std::cout << result.str[i] << ' ';
                    // }
                    // std::cout << token << '\n';
                    // for (int i = 0; i < Lookahead; i++) {
                    //     std::cout << temp_grammar->_id2view[result.str[i]] << ' ';
                    // }
                    // std::cout << '\n';
                }
                lookahead.push_back(result);
            }
            // std::cout << "------\n";
            // for (auto elem : temp_anal->k_first[nt_id]) {
            //     for (int i = 0; i < Lookahead; i++) {
            //         std::cout << elem.str[i] << ' ';
            //     }
            //     std::cout << '\n';
            // }
            EXPECT_THAT(temp_anal->k_first[nt_id], UnorderedElementsAreArray(lookahead));
        }
    }
    
    void print_tables (Builder<Lookahead>& builder) {
        if (!PRINT_TABLES) return;
        for (int table = 0; table < builder.table_.size(); table++) {
            std::cout << "\nTable " << table << '\n';
            std::cout << "Kernel:\n";
            for (auto item : builder.table_[table]) {
                auto& prod = temp_grammar->numbered_prod[item.rule_id];
                std::cout << temp_grammar->_id2view[prod.first] << " -> ";
                for (auto c : temp_grammar->_rhs[prod.second]) {
                    std::cout <<  temp_grammar->_id2view[c];
                }
                std::cout << ", " << item.dot << ", ";
                for (int index = 0; index < item.lookahead.len; index++) {
                    std::cout << temp_grammar->_id2view[item.lookahead.str[index]];
                }
                std::cout << "\n";
            }
            std::cout << "Action:\n";
            for (auto lookahead : builder.action_[table]) {
                for (int index = 0; index < lookahead.first.len; index++) {
                    std::cout << temp_grammar->_id2view[lookahead.first.str[index]];
                }
                if (lookahead.first.len == 0) std::cout << "eps";
                std::cout << ' ' << (int)lookahead.second.get_action();
                if (lookahead.second.get_action() == ActionType::REDUCE) {
                    std::cout << ' ' << (int)lookahead.second.get_rule_id();
                }
                std::cout << '\n';
            }
            std::cout << "GOTO:\n";
            for (auto token : builder.goto_[table]) {
                std::cout << temp_grammar->_id2view[token.first] << " -> " << token.second << '\n';
            }
            

        }
    }

    std::optional<Grammar> temp_grammar;
    std::optional<GrammarAnalysis<Lookahead>> temp_anal; // any questions?
};
