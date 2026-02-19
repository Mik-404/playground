#pragma once

#include <array>
#include <deque>
#include <filesystem>
#include <functional>
#include <stdint.h>
#include <string>
#include <span>


template <int Lookahead>
struct KString {
    std::array<int, Lookahead> str;
    uint8_t len = 0;

    KString () = default;

    explicit KString (int nt) noexcept {
        static_assert(Lookahead != 0, "Unexpected Behaviour");
        str[0] = nt, len = 1;
    }

    KString operator+ (const KString& rhs) const noexcept {
        KString result (*this);
        for (int i = 0; (i < rhs.len) && ((i + len) < Lookahead); ++i) {
            result.str[result.len] = rhs.str[i];
            result.len++;
        }
        return result;
    }

    std::strong_ordering operator<=> (const KString& rhs) const {
        for (size_t i = 0; i < std::min(len, rhs.len); i++) {
            if (auto cmp = str[i] <=> rhs.str[i]; cmp != 0) {
                return cmp;
            }
        }
        if (auto cmp = len <=> rhs.len; cmp != 0) {
            return cmp;
        }
        return std::strong_ordering::equal;
    };

    bool operator== (const KString& rhs) const {
        if (rhs.len != len) return false;
        return operator<=>(rhs) == std::strong_ordering::equal;
    }

    bool contains (int elem) const noexcept {
        for (int i = 0; i < len; i++) {
            if (str[i] == elem) return true;
        }
        return false;
    }

    uint64_t hash () const noexcept {
        size_t seed = len;
        for(size_t x = 0, i = 0; i < len; i++, x=str[i]) {
            x = ((x >> 16) ^ x) * 0x45d9f3b;
            x = ((x >> 16) ^ x) * 0x45d9f3b;
            x = (x >> 16) ^ x;
            seed ^= x + 0x9e3779b9 + (seed << 6) + (seed >> 2);
        }
        return seed;
    }
};

std::string _read_file (const std::filesystem::path& path);

template <int Lookahead>
struct KStringHasher {
    size_t operator() (const KString<Lookahead>& obj) const {
        return obj.hash();
    }
};


inline void hash_combine(std::size_t& seed, std::size_t value) {
    seed ^= value + 0x9e3779b9 + (seed << 6) + (seed >> 2);
}

template <int Lookahead>
struct Item {
    int rule_id = 0;
    KString<Lookahead> lookahead;
    uint32_t dot = 0;

    Item (int rule_id, const KString<Lookahead>& lookahead, uint8_t dot)
            : rule_id(rule_id), lookahead(lookahead), dot(dot) {}

    auto operator<=>(const Item<Lookahead>&) const = default;
};

enum class ActionType : uint8_t {
    REDUCE = 2,
    SHIFT = 0,
    ACCEPT = 1
};

struct Action {
    uint32_t action;

    ActionType get_action () const noexcept;

    uint32_t get_rule_id () const noexcept;

    Action (ActionType type, uint32_t rule_id = 0);

    Action ();

    bool operator== (const Action& other) const noexcept;
};

template<int Lookahead>
struct ItemHash {
    size_t operator()(const Item<Lookahead>& item, size_t seed = 0) const {
        size_t h1 = std::hash<int>{}(item.rule_id);
        size_t h2 = std::hash<int>{}(item.dot);

        size_t h3 = item.lookahead.hash(); 

        hash_combine(seed, h1);
        hash_combine(seed, h2);
        hash_combine(seed, h3);
        return seed;
    }

    size_t operator()(const Item<Lookahead>* k) const {
        return operator()(*k);
    }
};

template<int Lookahead>
struct ItemEqual {

    bool operator()(const Item<Lookahead>* lhs, const Item<Lookahead>* rhs) const {
        return *lhs == *rhs;
    }

    bool operator()(const Item<Lookahead>* lhs, const Item<Lookahead>& rhs) const {
        return *lhs == rhs;
    }
    
    bool operator()(const Item<Lookahead>& lhs, const Item<Lookahead>* rhs) const {
        return lhs == *rhs;
    }
};

template<int Lookahead>
struct KernelHash {
    using Kernel = std::deque<Item<Lookahead>>;
    

    size_t operator()(const Kernel& state) const {
        size_t seed = 0;
        for (const auto& item : state) {
            seed = ItemHash<Lookahead>()(item, seed);
        }
        return seed;
    }

    size_t operator()(const Kernel* k) const {
        return operator()(*k);
    }
};

template<int Lookahead>
struct KernelEqual {
    using Kernel = std::deque<Item<Lookahead>>;

    bool operator()(const Kernel* lhs, const Kernel* rhs) const {
        return *lhs == *rhs;
    }

    bool operator()(const Kernel* lhs, const Kernel& rhs) const {
        return *lhs == rhs;
    }
    
    bool operator()(const Kernel& lhs, const Kernel* rhs) const {
        return lhs == *rhs;
    }
};


struct Tokenizer {
    std::string tokens;
    std::vector<int> _rhs_storage;
    std::vector<std::pair<int, std::span<int>>> prod;
    std::vector<std::string_view> _id2view;

    Tokenizer ();

    std::vector<int> tokenize (const std::string& text) const;
};