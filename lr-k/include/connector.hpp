#pragma once

#include "builder.hpp"
#include "runtime_parser.hpp"
#include <fstream>
#include <stdint.h>

struct Exporter {
    Grammar& grammar;
    Exporter (Grammar& grammar): grammar(grammar) {}

    template <int Lookahead>
    void serialize(Builder<Lookahead>& builder, std::ostream& out) const {
        int lookahead = Lookahead;
        out.write(reinterpret_cast<const char*>(&lookahead), sizeof(lookahead));


        // save token
        size_t token_count = grammar._id2view.size();
        out.write(reinterpret_cast<const char*>(&token_count), sizeof(token_count));

        size_t total_length = 0;
        for (auto& token : grammar._id2view) {
            total_length += token.size();
        }
        out.write(reinterpret_cast<const char*>(&total_length), sizeof(total_length));

        for (auto& token : grammar._id2view) {
            uint32_t token_len = token.size();
            out.write(reinterpret_cast<const char*>(&token_len), sizeof(token_len));
            out.write(token.data(), token_len);
        }

        // save prod rules
        size_t prod_list_len = grammar.numbered_prod.size();
        size_t prod_storage_list_len = grammar.get_storage_size();
        out.write(reinterpret_cast<const char*>(&prod_list_len), sizeof(prod_list_len));
        out.write(reinterpret_cast<const char*>(&prod_storage_list_len), sizeof(prod_storage_list_len));

        for (auto& prod : grammar.numbered_prod) {
            out.write(reinterpret_cast<const char*>(&prod.first), sizeof(prod.first));
            uint32_t rhs_len = grammar._rhs[prod.second].size();
            out.write(reinterpret_cast<const char*>(&rhs_len), sizeof(rhs_len));
            out.write(reinterpret_cast<const char*>(grammar._rhs[prod.second].data()), 
                                    rhs_len * sizeof(grammar._rhs[prod.second].front()));
        }

        // save goto table
        size_t _goto_size = builder.goto_.size();
        out.write(reinterpret_cast<const char*>(&_goto_size), sizeof(_goto_size));
        for (auto& local_goto_table : builder.goto_) {
            uint32_t local_goto_table_size = local_goto_table.size();
            out.write(reinterpret_cast<const char*>(&local_goto_table_size), 
                                                sizeof(local_goto_table_size));
            for (auto& trans : local_goto_table) {
                out.write(reinterpret_cast<const char*>(&trans.first), sizeof(trans.first));
                out.write(reinterpret_cast<const char*>(&trans.second), sizeof(trans.second));
            }
        }


        // save action table
        size_t _action_size = builder.action_.size();
        out.write(reinterpret_cast<const char*>(&_action_size), sizeof(_action_size));
        for (auto& local_action_table : builder.action_) {
            uint32_t local_action_table_size = local_action_table.size();
            out.write(reinterpret_cast<const char*>(&local_action_table_size), 
                                                sizeof(local_action_table_size));
            for (auto& action : local_action_table) {
                out.write(reinterpret_cast<const char*>(&action.first), sizeof(action.first));
                out.write(reinterpret_cast<const char*>(&action.second), sizeof(action.second));
            }
        }
    }

    template <int Lookahead>
    void export_to_file(Builder<Lookahead>& builder, const std::string& filename) const {
        std::ofstream out(filename, std::ios::binary);
        if (!out.is_open()) throw std::runtime_error("Fail");
        serialize(builder, out);
    }
};


struct Importer {
    template <int Lookahead>
    RuntimeParser<Lookahead> load(RuntimeParser<Lookahead>& result, std::istream& in) const {
        int lookahead;
        in.read(reinterpret_cast<char*>(&lookahead), sizeof(lookahead));

        if (lookahead != Lookahead) throw std::runtime_error("Wrong lookahead mismatch! Generator used K = " +
                        std::to_string(lookahead));
        // load tokens
        size_t token_count;
        size_t total_length;
        in.read(reinterpret_cast<char*>(&token_count), sizeof(token_count));
        in.read(reinterpret_cast<char*>(&total_length), sizeof(total_length));
        result.tokenizer._id2view.reserve(token_count);
        result.tokenizer.tokens.resize(total_length);
        char* ptr_to_string = result.tokenizer.tokens.data();
        for (int index = 0; index < token_count; index++) {
            uint32_t token_len;
            in.read(reinterpret_cast<char*>(&token_len), sizeof(token_len));
            in.read(ptr_to_string, token_len);
            result.tokenizer._id2view.emplace_back(ptr_to_string, token_len);
            ptr_to_string += token_len;
        }
        // load prod rules
        size_t prod_list_len;
        in.read(reinterpret_cast<char*>(&prod_list_len), sizeof(prod_list_len));
        in.read(reinterpret_cast<char*>(&total_length), sizeof(total_length));
        result.tokenizer._rhs_storage.resize(total_length);
        result.tokenizer.prod.reserve(prod_list_len);
        int* ptr_to_int = result.tokenizer._rhs_storage.data();

        for (int index = 0; index < prod_list_len; index++) {
            int lhs_id;
            in.read(reinterpret_cast<char*>(&lhs_id), sizeof(lhs_id));
            uint32_t rhs_len;
            in.read(reinterpret_cast<char*>(&rhs_len), sizeof(rhs_len));
            in.read(reinterpret_cast<char*>(ptr_to_int), 
                                    rhs_len * sizeof(result.tokenizer._rhs_storage.front()));
            std::span<int> rhs (ptr_to_int, rhs_len);
            result.tokenizer.prod.emplace_back(lhs_id, rhs);
            ptr_to_int += rhs_len;    
        }
        
        // load goto table
        size_t _goto_size;
        in.read(reinterpret_cast<char*>(&_goto_size), sizeof(_goto_size));
        result.goto_.resize(_goto_size);
        for (auto& local_goto_table : result.goto_) {
            uint32_t local_goto_table_size;
            in.read(reinterpret_cast<char*>(&local_goto_table_size), 
                                                sizeof(local_goto_table_size));
            for (int index = 0; index < local_goto_table_size; index++) {
                int token_id;
                uint32_t table_id;
                in.read(reinterpret_cast<char*>(&token_id), sizeof(token_id));
                in.read(reinterpret_cast<char*>(&table_id), sizeof(table_id));
                local_goto_table[token_id] = table_id;
            }
        }

        // load action table
        size_t _action_size;
        in.read(reinterpret_cast<char*>(&_action_size), sizeof(_action_size));
        result.action_.resize(_action_size);
        for (auto& local_action_table : result.action_) {
            uint32_t local_action_table_size;
            in.read(reinterpret_cast<char*>(&local_action_table_size), 
                                                sizeof(local_action_table_size));
            for (int index = 0; index < local_action_table_size; index++) {
                KString<Lookahead> lookahead_str;
                Action action;
                in.read(reinterpret_cast<char*>(&lookahead_str), sizeof(lookahead_str));
                in.read(reinterpret_cast<char*>(&action), sizeof(action));
                local_action_table[lookahead_str] = action;
            }
        }
        return result;
    }

    template <int Lookahead>
    RuntimeParser<Lookahead> import_from_file(RuntimeParser<Lookahead>& parser
            , const std::string& filename) const {
        std::ifstream in(filename, std::ios::binary);
        if (!in.is_open()) throw std::runtime_error("Fail");
        load<Lookahead>(parser, in);
        return parser;
    }
};