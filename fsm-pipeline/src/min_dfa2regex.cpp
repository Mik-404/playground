#include "min_dfa2regex.hpp"

namespace {
    string parenthesize_for_concat(const string& s) {
        if (s.empty()) return s;
        bool needs_paren = s.find('|') != string::npos;
        if (needs_paren && s.front() == '(' && s.back() == ')') {
            int balance = 0;
            bool outer_paren = true;
            for(size_t i = 0; i < s.length() - 1; ++i) {
                if (s[i] == '(') balance++;
                else if (s[i] == ')') balance--;
                if (balance == 0) {
                    outer_paren = false;
                    break;
                }
            }
            if(outer_paren) needs_paren = false;
        }

        if (needs_paren) return "(" + s + ")";
        return s;
    }

    string union_str(const string& r1, const string& r2) {
        if (r1.empty()) return r2;
        if (r2.empty()) return r1;
        if (r1 == r2) return r1;
        return r1 + "|" + r2;
    }

    string concat_str(const string& r1, const string& r2) {
        if (r1.empty() || r2.empty()) return "";
        if (r1 == epsilon_utf8) return r2;
        if (r2 == epsilon_utf8) return r1;
        return parenthesize_for_concat(r1) + parenthesize_for_concat(r2);
    }
    
    string kleene_star_str(const string& r) {
        if (r.empty()) return "";
        if (r == epsilon_utf8) return epsilon_utf8;
        if (r.length() == 1) return r + "*";
        if (r.length() > 1 && r.back() == '*') return r;
        return "(" + r + ")*";
    }
}

/**
 * @brief Converts a finite automaton to an equivalent regex.
 *
 * Uses dynamic programming (state elimination):
 *  1. Map states to indices, find start/final states.
 *  2. Init DP table with transitions (k=0).
 *  3. Iteratively refine with R[k][i][j] rule.
 *  4. Union all paths from start to finals.
 *
 * @param graph FA as a Boost Graph.
 * @return Equivalent regex string.
 */
string faToRegex(Graph graph) {
    if (boost::num_vertices(graph) == 0) {
        return "";
    }

    // --- Step 1: Preparation and mapping states to indices ---
    map<Vertex, int> vertex_to_idx;
    vector<Vertex> idx_to_vertex;
    int idx_counter = 0;
    for (const auto& v : boost::make_iterator_range(boost::vertices(graph))) {
        vertex_to_idx[v] = idx_counter;
        idx_to_vertex.push_back(v);
        idx_counter++;
    }

    const int n = boost::num_vertices(graph);
    int start_idx = -1;
    vector<int> final_indices;

    for (int i = 0; i < n; ++i) {
        const auto& props = graph[idx_to_vertex[i]];
        if (props.type == VertexType::Start || props.type == VertexType::StartTerminal) {
            start_idx = i;
        }
        if (props.type == VertexType::Terminal || props.type == VertexType::StartTerminal) {
            final_indices.push_back(i);
        }
    }

    if (start_idx == -1) {
        throw runtime_error("No start state found in FA for regex conversion.");
    }

    // --- Step 2: Initialize DP table (k = 0) ---
    vector<vector<string>> dp_prev(n, vector<string>(n, ""));

    for (const auto& e : boost::make_iterator_range(boost::edges(graph))) {
        int u = vertex_to_idx.at(boost::source(e, graph));
        int v = vertex_to_idx.at(boost::target(e, graph));
        string label = graph[e].symbol.has_value() ? string(1, graph[e].symbol.value()) : epsilon_utf8;
        dp_prev[u][v] = union_str(dp_prev[u][v], label);
    }
    for (int i = 0; i < n; ++i) {
        dp_prev[i][i] = union_str(epsilon_utf8, dp_prev[i][i]);
    }

    // --- Step 3: Main DP loop ---
    vector<vector<string>> dp_curr = dp_prev;

    for (int k = 0; k < n; ++k) {
        for (int i = 0; i < n; ++i) {
            for (int j = 0; j < n; ++j) {
                // R[k-1][i][k] . (R[k-1][k][k])* . R[k-1][k][j]
                string r_ik = dp_prev[i][k];
                string r_kk_star = kleene_star_str(dp_prev[k][k]);
                string r_kj = dp_prev[k][j];
                
                string bypass_path = concat_str(concat_str(r_ik, r_kk_star), r_kj);
                
                // R[k][i][j] = R[k-1][i][j] | bypass_path
                dp_curr[i][j] = union_str(dp_prev[i][j], bypass_path);
            }
        }
        dp_prev = dp_curr;
    }

    string final_regex = "";
    for (int final_idx : final_indices) {
        final_regex = union_str(final_regex, dp_curr[start_idx][final_idx]);
    }

    return final_regex;
}

