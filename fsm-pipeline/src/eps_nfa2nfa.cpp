#include "eps_nfa2nfa.hpp"


/**
 * @brief Computes the epsilon-closure for a given set of states.
 * 
 * @param states The initial set of states.
 * @param g The ε-NFA graph.
 * @return set<Vertex> The set of all states reachable from the initial set
 *         through zero or more ε-transitions.
 */
set<Vertex> EpsilonClosure(const set<Vertex>& states, const Graph& g) {
    set<Vertex> closure = states;
    std::queue<Vertex> q;

    for (const auto& v : states) {
        q.push(v);
    }

    while (!q.empty()) {
        Vertex u = q.front();
        q.pop();

        auto [edge_it, edge_end] = boost::out_edges(u, g);
        for (; edge_it != edge_end; ++edge_it) {
            if (!g[*edge_it].symbol.has_value()) {
                Vertex v = boost::target(*edge_it, g);
                if (closure.find(v) == closure.end()) {
                    closure.insert(v);
                    q.push(v);
                }
            }
        }
    }
    return closure;
}


/**
 * @brief Converts an ε-NFA into an equivalent NFA without ε-transitions.
 * 
 * @param eps_nfa The input graph representing the ε-NFA.
 * @return Graph A new graph representing the NFA.
 */
Graph epsNfaToNfa(Graph eps_nfa) {
    Graph nfa;
    map<Vertex, Vertex> old_to_new_map;
    
    auto [old_verts_it, old_verts_end] = boost::vertices(eps_nfa);

    // === Step 1: Copying vertices and collecting information ===
    set<char> alphabet;
    vector<Vertex> original_terminals;
    Vertex original_start_state;
    bool start_state_found = false;

    for (auto it = old_verts_it; it != old_verts_end; ++it) {
        Vertex old_v = *it;
        VertexProperties old_props = eps_nfa[old_v];

        VertexProperties new_props = {old_props.name, VertexType::Default};
        Vertex new_v = boost::add_vertex(new_props, nfa);
        old_to_new_map[old_v] = new_v;

        if (old_props.type == VertexType::Start || old_props.type == VertexType::StartTerminal) {
            original_start_state = old_v;
            start_state_found = true;
        }
        if (old_props.type == VertexType::Terminal || old_props.type == VertexType::StartTerminal) {
            original_terminals.push_back(old_v);
        }
    }
    
    auto [edge_it, edge_end] = boost::edges(eps_nfa);
    for (; edge_it != edge_end; ++edge_it) {
        if (eps_nfa[*edge_it].symbol.has_value()) {
            alphabet.insert(eps_nfa[*edge_it].symbol.value());
        }
    }
    
    // === Step 2: Creating new transitions ===
    for (auto it = old_verts_it; it != old_verts_end; ++it) {
        Vertex q_old = *it;
        
        set<Vertex> closure_q = EpsilonClosure({q_old}, eps_nfa);

        for (char symbol : alphabet) {
            set<Vertex> reachable_after_symbol;
            for (const auto& p : closure_q) {
                auto [p_edges_it, p_edges_end] = boost::out_edges(p, eps_nfa);
                for (; p_edges_it != p_edges_end; ++p_edges_it) {
                    const auto& edge_label = eps_nfa[*p_edges_it].symbol;
                    if (edge_label.has_value() && edge_label.value() == symbol) {
                        reachable_after_symbol.insert(boost::target(*p_edges_it, eps_nfa));
                    }
                }
            }
            
            if (!reachable_after_symbol.empty()) {
                set<Vertex> final_targets = EpsilonClosure(reachable_after_symbol, eps_nfa);

                Vertex q_new = old_to_new_map[q_old];
                for (const auto& r_old : final_targets) {
                    Vertex r_new = old_to_new_map[r_old];
                    EdgeProperties props = {1.0, symbol};
                    boost::add_edge(q_new, r_new, props, nfa);
                }
            }
        }
    }

    // === Step 3: Determining new vertex types (Start, Terminal, StartTerminal, Default) ===
    
    for (auto it = old_verts_it; it != old_verts_end; ++it) {
        Vertex q_old = *it;
        Vertex q_new = old_to_new_map[q_old];
        
        bool is_start = start_state_found && (q_old == original_start_state);

        bool is_terminal = false;
        set<Vertex> closure_q = EpsilonClosure({q_old}, eps_nfa);
        for (const auto& old_terminal : original_terminals) {
            if (closure_q.count(old_terminal)) {
                is_terminal = true;
                break;
            }
        }
        
        if (is_start && is_terminal) {
            nfa[q_new].type = VertexType::StartTerminal;
        } else if (is_start) {
            nfa[q_new].type = VertexType::Start;
        } else if (is_terminal) {
            nfa[q_new].type = VertexType::Terminal;
        } else {
            nfa[q_new].type = VertexType::Default;
        }
    }
    
    if (debug) {
        std::cout << "Conversion to NFA complete." << std::endl;
    }
    return nfa;
}