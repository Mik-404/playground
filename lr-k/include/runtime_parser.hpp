#pragma once

#include "utils.hpp"
#include "connector.hpp"

template<int Lookahead>
class RuntimeParser {
    
  public:
    
    Tokenizer tokenizer;
    std::deque<std::unordered_map<int, uint32_t>> goto_;
    std::deque<std::unordered_map<KString<Lookahead>, Action, KStringHasher<Lookahead>>> action_;

    RuntimeParser () = default;

    std::vector<uint32_t> parse(const std::string& text) const {
        std::vector<int> input_text_tokenized = tokenizer.tokenize(text);
        std::vector<uint32_t> result;
        uint32_t table = 0;
        KString<Lookahead> current_lookahead;
        std::vector <int64_t> memory; memory.push_back(table);
        for (uint32_t index = 0; index <= input_text_tokenized.size(); ) {
            current_lookahead.len = 0;
            for (int shift = 0; shift < Lookahead 
                        && index + shift < input_text_tokenized.size(); shift++) {
                current_lookahead.str[shift] = input_text_tokenized[index + shift];
                current_lookahead.len++;
            }
            auto action = action_[table].find(current_lookahead);
            if (action == action_[table].end()) 
                throw std::runtime_error("Syntax error at token " + std::to_string(index));
            if (action->second.get_action() == ActionType::SHIFT) {
                if (index == input_text_tokenized.size()) 
                    throw std::runtime_error("Syntax error at token " + std::to_string(index));
                memory.push_back(input_text_tokenized[index]);
                table = goto_[table].at(memory.back());
                memory.push_back(table);
                index++;
            } else if (action->second.get_action() == ActionType::REDUCE) {
                uint32_t prod_num = action->second.get_rule_id();
                result.push_back(prod_num);
                uint32_t prod_len = tokenizer.prod[prod_num].second.size();
                memory.resize(memory.size() - prod_len * 2);
                table = memory.back();
                memory.push_back(tokenizer.prod[prod_num].first);
                table = goto_[table].at(memory.back());
                memory.push_back(table);
            } else {
                if (index != input_text_tokenized.size()) 
                    throw std::runtime_error("Syntax error at token " + std::to_string(index));
                index++;
            }
        }
        return result;
    }
};