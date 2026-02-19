#pragma once

#include "first_k.hpp"
#include "utils.hpp"
#include <iostream>
#include <algorithm>


template <int Lookahead>
struct Builder {
    using Kernel = std::deque<Item<Lookahead>>;
    using Table = std::deque<Item<Lookahead>>;
    using table_checker = std::unordered_map<const Kernel*, uint32_t, 
                        KernelHash<Lookahead>, KernelEqual<Lookahead>>;

  private:
    Grammar& grammar;
    GrammarAnalysis<Lookahead>& k_first_gen;
    table_checker _check_table_exists;

  public:
    using item_checker = std::unordered_map<const Item<Lookahead>*, uint32_t,
                ItemHash<Lookahead>, ItemEqual<Lookahead>>;
    using lookahead_list = GrammarAnalysis<Lookahead>::lookahead_list;
    std::deque<Kernel> table_;
    std::deque<std::unordered_map<int, uint32_t>> goto_;
    std::deque<std::unordered_map<KString<Lookahead>, Action, KStringHasher<Lookahead>>> action_;

    Table closure (const Kernel& kernel) const {
        item_checker _check_item_exists;
        std::deque<uint32_t> worklist;
        Table result (kernel);
        for (uint32_t iter = 0; iter < result.size(); iter++) {
            _check_item_exists[&result[iter]] = iter;
            worklist.push_back(iter);
        }
        while (!worklist.empty()) {
            uint32_t current_item = worklist.front();
            worklist.pop_front();
            int rhs = grammar.numbered_prod[result[current_item].rule_id].second;
            // выход за границу
            if (grammar._rhs[rhs].size() == result[current_item].dot) continue;
            
            int next_token = grammar._rhs[rhs][result[current_item].dot];
            if (grammar.nonterminals.contains(next_token)) {
                lookahead_list possible_lookaheads {KString<Lookahead>()};
                for (uint32_t iter = result[current_item].dot + 1; iter < grammar._rhs[rhs].size(); iter++) {
                    possible_lookaheads = k_first_gen.k_concat(possible_lookaheads, k_first_gen.k_first[grammar._rhs[rhs][iter]]);
                }
                possible_lookaheads = k_first_gen.k_concat(possible_lookaheads, {result[current_item].lookahead});

                auto [start, end] = grammar.productions.equal_range(next_token);
                for (;start != end; start++) {
                    for (auto& lookahead : possible_lookaheads) {
                        Item<Lookahead> possible_item (start->second, lookahead, 0);
                        if (!_check_item_exists.contains(&possible_item)) {
                            result.push_back(possible_item);
                            _check_item_exists[&result.back()] = result.size() - 1;
                            worklist.push_back(result.size() - 1);
                        }
                    }
                }
            }
        }
        return result;
    }

    Builder (Grammar& grammar, GrammarAnalysis<Lookahead>& k_first_gen)
            : grammar(grammar), k_first_gen(k_first_gen) {}


    void build () {
        table_.push_back({Item<Lookahead>(grammar.productions
                            .find(grammar.start)->second, KString<Lookahead>(), 0)});
        _check_table_exists[&table_.back()] = 0;
        goto_.push_back({});
        action_.push_back({});
        std::deque<uint32_t> worklist;
        worklist.push_back(0);
        while (!worklist.empty()) {
            // Process kernel: compute closure, goto, and action table
            uint32_t current_kernel = worklist.front();
            worklist.pop_front();
            Table closure_table = closure(table_[current_kernel]);

            std::deque<Kernel> goto_func;
            std::unordered_map<int, uint32_t> has_token;
            for (auto& item : closure_table) {
                auto item_rhs = grammar._rhs[grammar.numbered_prod[item.rule_id].second];
                if (item.dot == item_rhs.size()) {
                    auto& local_action = action_[current_kernel];
                    if (local_action.contains(item.lookahead)) {
                        if (local_action[item.lookahead].get_action() == ActionType::REDUCE)
                            throw std::runtime_error("incorrect grammar: reduce / reduce conflict");
                        else throw std::runtime_error("incorrect grammar: shift / reduce conflict");
                    }
                    local_action.emplace(item.lookahead, Action(ActionType::REDUCE, item.rule_id));
                } else {
                    uint32_t goto_kernel_index = goto_func.size();
                    if (has_token.contains(item_rhs[item.dot])) {
                        goto_kernel_index = has_token[item_rhs[item.dot]];
                    } else {
                        has_token[item_rhs[item.dot]] = goto_func.size();
                        goto_func.push_back({});
                    }
                    goto_func[goto_kernel_index].push_back(item);
                    goto_func[goto_kernel_index].back().dot++;

                    if (grammar.nonterminals.contains(item_rhs[item.dot])) continue;

                    lookahead_list possible_lookaheads {KString<Lookahead>()};
                    for (uint32_t iter = item.dot; iter < item_rhs.size(); iter++) {
                        possible_lookaheads = k_first_gen.k_concat(possible_lookaheads, k_first_gen.k_first[item_rhs[iter]]);
                    }
                    possible_lookaheads = k_first_gen.k_concat(possible_lookaheads, {item.lookahead});
                    auto& local_action = action_[current_kernel];
                    for (auto& lookahead : possible_lookaheads) {
                        if (local_action.contains(lookahead) && 
                                local_action[lookahead].get_action() == ActionType::REDUCE) 
                            throw std::runtime_error("incorrect grammar: shift / reduce conflict");
                        local_action.emplace(lookahead, Action(ActionType::SHIFT));
                    }
                }
            }

            for (auto& token : has_token) {
                // sort kernels to ensure consistent representation
                auto& candidate_kernel = goto_func[token.second];
                std::sort(candidate_kernel.begin(), candidate_kernel.end());

                if (_check_table_exists.contains(&candidate_kernel)) {
                    goto_[current_kernel][token.first] = _check_table_exists[&candidate_kernel];
                } else {
                    table_.push_back(std::move(candidate_kernel));
                    _check_table_exists[&table_.back()] = table_.size() - 1;
                    goto_.push_back({});
                    action_.push_back({});
                    worklist.push_back(table_.size() - 1);
                    goto_[current_kernel][token.first] = table_.size() - 1;
                }
            }
        }
        uint32_t start_prod_id = grammar.productions.find(grammar.start)->second;
        // find and set accept
        action_[goto_[0][grammar._rhs[grammar.numbered_prod[start_prod_id].second][0]]]
                [KString<Lookahead>()] = Action (ActionType::ACCEPT);
    }

};