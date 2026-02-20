#pragma once

#include "grammar.hpp"
#include "utils.hpp"
#include <queue>


template <int Lookahead>
class GrammarAnalysis {
    const Grammar& grammar;
    std::unordered_multimap<int, int> _depends;

  public:
    using lookahead_list = std::vector<KString<Lookahead>>;
    std::vector<lookahead_list> k_first;

    GrammarAnalysis (const Grammar& grammar): grammar(grammar) {
        for (auto prod : grammar.productions) {
            for (auto token : grammar._rhs[grammar.numbered_prod[prod.second].second]) {
                if (grammar.nonterminals.contains(token)) {
                    _depends.emplace(token, prod.first);
                }
            }
        }
    }

    static lookahead_list k_concat (const lookahead_list& lhs, const lookahead_list& rhs) {
        lookahead_list result;
        result.reserve(lhs.size() * rhs.size());
        for (auto i : lhs) {
            if (i.len == Lookahead) {
                result.push_back(i);
                continue;
            }
            for (auto j : rhs) {
                KString next_candidate = i + j;
                if (result.empty() || next_candidate != result.back()) result.push_back(next_candidate);
            }
        }
        
        std::sort(result.begin(), result.end());
    
        auto last = std::unique(result.begin(), result.end());
        result.erase(last, result.end());

        return result;
    }

    static bool merge_lookahead_lists (lookahead_list& lhs, const lookahead_list& rhs) {
        bool changes = lhs != rhs;
        if (! changes) return changes;
        lookahead_list result;
        result.reserve(lhs.size() + rhs.size());
        for (size_t i = 0, j = 0; i < lhs.size() || j < rhs.size(); ) {
            if (j == rhs.size() || (i != lhs.size() && lhs[i] < rhs[j])) {
                if (result.empty() || lhs[i] != result.back()) result.push_back(lhs[i]);
                i++;
            } else {
                if (result.empty() || rhs[j] != result.back()) result.push_back(rhs[j]);
                j++;
            }
        }
        result.shrink_to_fit();
        lhs = std::move(result);
        return changes;
    }

    void build_kfirst () {
        k_first.resize(grammar._view2id.size());
        std::queue<int> worklist;
        std::unordered_set<int> worklist_double_entry_check;
        for (auto token : grammar._view2id) {
            if (!grammar.nonterminals.contains(token.second)) {
                if constexpr (Lookahead != 0) {
                    k_first[token.second].emplace_back(token.second);
                } else {
                    k_first[token.second].push_back({});
                }
            } else {
                worklist.push(token.second);
                worklist_double_entry_check.insert(token.second);
            }
        }
        while (!worklist.empty()) {
            int current_nt = worklist.front();
            worklist.pop();
            worklist_double_entry_check.erase(current_nt);
            lookahead_list result;
            auto [start, end] = grammar.productions.equal_range(current_nt);
            for (; start != end; start++) {
                auto rhs = grammar._rhs[grammar.numbered_prod[start->second].second];
                if (rhs.empty()) {
                    lookahead_list production_concat;
                    production_concat.emplace_back();
                    merge_lookahead_lists(result, production_concat);
                    continue;
                }
                lookahead_list production_concat = k_first[rhs[0]];
                for (auto token : rhs.subspan(1)) {
                    production_concat = k_concat(production_concat, k_first[token]);
                }
                merge_lookahead_lists(result, production_concat);
            }
            if (merge_lookahead_lists(k_first[current_nt], result)) {
                auto [start, end] = _depends.equal_range(current_nt);
                for (; start != end; start++) {
                    if (!worklist_double_entry_check.contains(start->second)){
                        worklist.push(start->second);
                        worklist_double_entry_check.insert(start->second);
                    }
                }
            }
        }
    }
};
