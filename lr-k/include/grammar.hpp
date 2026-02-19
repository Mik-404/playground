#pragma once

#include <cstdint>
#include <filesystem>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>


static constexpr std::string_view WHITESPACE = " \t\n\r";
static constexpr std::string EPS = "eps";
static constexpr std::string SEPARATOR = "->";

class Grammar;

class Parser {

    std::string source_text;
    std::vector<int> _rhs_storage;

    friend Grammar;

  public:
    std::unordered_map<std::string_view, int> _view2id;
    std::vector<std::span<int>> _rhs;
    std::vector<std::string_view> _id2view;
    std::vector<std::pair<int, int>> numbered_prod;
    std::unordered_multimap<int, uint32_t> productions;

    explicit Parser (std::string&& text);
};


class Grammar : public Parser {
  private:
    std::string _start_token;
    
  public:
    std::unordered_set<int> nonterminals;
    int start;
    explicit Grammar (const std::filesystem::path& path);
    void Augment ();
    size_t get_storage_size() const noexcept;
};