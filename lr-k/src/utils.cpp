#include "utils.hpp"
#include <ranges>
#include <fstream>



ActionType Action::get_action () const noexcept {
    if (action == 0) {
        return ActionType::SHIFT;
    } else if (action == 1) {
        return ActionType::ACCEPT;
    }
    return ActionType::REDUCE;
}

uint32_t Action::get_rule_id () const noexcept {
    return action - 2;
}

Action::Action (ActionType type, uint32_t rule_id) {
    if (type != ActionType::REDUCE) action = static_cast<uint32_t> (type);
    else action = rule_id + 2;
}

Action::Action () = default;

bool Action::operator== (const Action& other) const noexcept {
    return this->action == other.action;
}

Tokenizer::Tokenizer() = default;

std::vector<int> Tokenizer::tokenize (const std::string& text) const {
    std::unordered_map<std::string_view, int> _view2id;
    for (uint32_t index = 0; index < _id2view.size(); index++) {
        _view2id[_id2view[index]] = index;
    }
    std::string_view view = text;
    std::vector<int> result;
    auto tokens_range = std::views::split(view, ' ');
    for (auto token_iter : tokens_range) {
        std::string_view token (token_iter.begin(), token_iter.end());
        if (!_view2id.contains(token)) throw std::runtime_error("unknown token");
        result.push_back(_view2id[token]);
       
    }
    return result;
}

std::string _read_file (const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::in | std::ios::binary);
    if (!file) throw std::runtime_error("Fail");

    const int sz = std::filesystem::file_size(path);
    std::string text (sz,'\0');
    file.read(text.data(), sz);
    return text;
}