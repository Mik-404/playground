#include <iostream>
#include "grammar.hpp"
#include "utils.hpp"
#include <ranges>



Parser::Parser (std::string&& text): source_text(std::move(text)) {
    std::string_view view = source_text;
    std::vector<int> temp_rhs;
    auto lines_range = std::views::split(view, '\n');
    for (auto line_iter : lines_range) {
        std::string_view line (line_iter.begin(), line_iter.end());
        int lhs = -1;
        size_t rhs_start_storage_pos = _rhs_storage.size();
        int rhs_len = 0;
        auto tokens = line
                        | std::views::split(' ')
                        | std::views::filter([](const auto& range) { return !range.empty(); });
        for (auto token_iter = tokens.begin(); token_iter != tokens.end(); ++token_iter) {
            std::string_view token ((*token_iter).begin(), (*token_iter).end());

            if (!_view2id.contains(token) && token != EPS) {
                _view2id[token] = _id2view.size();
                _id2view.push_back(token);
            }
            
            if (lhs == -1) {
                lhs = _view2id[token];
                if (++token_iter == tokens.end()) {
                    throw std::runtime_error("Incorrect production");
                };
                token = std::string_view((*token_iter).begin(), (*token_iter).end());

                if (token != SEPARATOR) {
                    throw std::runtime_error("Incorrect production");
                }
            } else if (token != EPS){
                _rhs_storage.push_back(_view2id[token]);
                rhs_len++;
            }
        }
        productions.emplace(lhs, numbered_prod.size());
        numbered_prod.emplace_back(lhs, temp_rhs.size());
        temp_rhs.push_back(rhs_len);
    }
    _rhs_storage.push_back(-1);
    auto left_bound = _rhs_storage.begin(); 
    auto right_bound = _rhs_storage.begin();
    for (size_t iter = 0; iter < temp_rhs.size(); ++iter) {
        right_bound += temp_rhs[iter];
        _rhs.emplace_back(left_bound, right_bound);
        left_bound = right_bound;
    }
}


Grammar::Grammar (const std::filesystem::path& path): Parser(_read_file(path)) {
    if (_view2id.find("S") == _view2id.end()) {
        throw std::runtime_error("no start nonterminal");
    }
    // Augment
    start = _id2view.size();

    // Make unique token
    _start_token = "S_0";
    while (_view2id.contains(_start_token)) {
        _start_token.push_back('0');
    }

    _view2id[_start_token] = _id2view.size();
    _id2view.push_back(_start_token);

    _rhs_storage.back() = _view2id["S"];
    _rhs.emplace_back(std::prev(_rhs_storage.end()), _rhs_storage.end());
    productions.emplace(start, numbered_prod.size());
    numbered_prod.emplace_back(start, _rhs.size()-1);
    // find nonterms
    auto iter = productions.begin();
    for (; iter != productions.end();) {
        nonterminals.insert(iter->first);
        iter = productions.equal_range(iter->first).second;
    }
}

size_t Grammar::get_storage_size () const noexcept {
    return this->_rhs_storage.size();
}