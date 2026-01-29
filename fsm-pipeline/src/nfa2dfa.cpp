#include "nfa2dfa.hpp"

/**
 * @brief Converts an NFA into an equivalent DFA using the subset construction algorithm.
 * 
 * @param nfa The input NFA graph. Assumes no epsilon-transitions.
 * @return Graph A new graph representing the DFA.
 */
Graph NfaToDfa(Graph nfa) {
    Graph dfa;
    if (boost::num_vertices(nfa) == 0) {
        return dfa;
    }

    // --- Step 1: init ---

    set<char> alphabet;
    for (const auto& e : boost::make_iterator_range(boost::edges(nfa))) {
        if (nfa[e].symbol.has_value()) {
            alphabet.insert(nfa[e].symbol.value());
        }
    }

    std::optional<Vertex> nfa_start_vertex_opt;
    for (const auto& v : boost::make_iterator_range(boost::vertices(nfa))) {
        if (nfa[v].type == VertexType::Start || nfa[v].type == VertexType::StartTerminal) {
            nfa_start_vertex_opt = v;
            break;
        }
    }

    if (!nfa_start_vertex_opt.has_value()) {
        std::cerr << "Error: Start vertex not found in NFA." << std::endl;
        return dfa;
    }
    Vertex nfa_start_vertex = *nfa_start_vertex_opt;

    map<set<Vertex>, Vertex> dfa_states_map;
    std::queue<set<Vertex>> worklist;


    // --- Step 2: Define the start state of the DFA ---

    set<Vertex> initial_nfa_set = {nfa_start_vertex};
    Vertex dfa_start_vertex = boost::add_vertex(dfa);

    bool is_start_also_terminal = (nfa[nfa_start_vertex].type == VertexType::Terminal || nfa[nfa_start_vertex].type == VertexType::StartTerminal);
    dfa[dfa_start_vertex].name = nfa[nfa_start_vertex].name; 
    dfa[dfa_start_vertex].type = is_start_also_terminal ? VertexType::StartTerminal : VertexType::Start;
    
    dfa_states_map[initial_nfa_set] = dfa_start_vertex;
    worklist.push(initial_nfa_set);
    int dfa_state_counter = 1;

    // --- Step 3: Основной цикл алгоритма построения подмножеств ---

    while (!worklist.empty()) {
        set<Vertex> current_nfa_set = worklist.front();
        worklist.pop();

        Vertex current_dfa_vertex = dfa_states_map.at(current_nfa_set);

        for (char symbol : alphabet) {
            set<Vertex> target_nfa_set;
            
            for (Vertex nfa_v : current_nfa_set) {
                for (const auto& edge : boost::make_iterator_range(boost::out_edges(nfa_v, nfa))) {
                    if (nfa[edge].symbol.has_value() && nfa[edge].symbol.value() == symbol) {
                        target_nfa_set.insert(boost::target(edge, nfa));
                    }
                }
            }

            if (target_nfa_set.empty()) {
                continue;
            }

            Vertex target_dfa_vertex;

            if (dfa_states_map.find(target_nfa_set) == dfa_states_map.end()) {
                target_dfa_vertex = boost::add_vertex(dfa);

                bool is_new_state_terminal = false;
                vector<int> names;
                for (Vertex v : target_nfa_set) {
                    if (nfa[v].type == VertexType::Terminal || nfa[v].type == VertexType::StartTerminal) {
                        is_new_state_terminal = true;
                    }
                    names.push_back(nfa[v].name);
                }

                std::sort(names.begin(), names.end());
                std::stringstream new_name_ss;
                for(size_t i = 0; i < names.size(); ++i) {
                    new_name_ss << names[i] << (i == names.size() - 1 ? "" : ",");
                }
                
                dfa[target_dfa_vertex].name = dfa_state_counter++;
                dfa[target_dfa_vertex].type = is_new_state_terminal ? VertexType::Terminal : VertexType::Default;
                
                dfa_states_map[target_nfa_set] = target_dfa_vertex;
                worklist.push(target_nfa_set);
            } else {
                target_dfa_vertex = dfa_states_map.at(target_nfa_set);
            }

            EdgeProperties dfa_edge_props = {1.0, symbol};
            boost::add_edge(current_dfa_vertex, target_dfa_vertex, dfa_edge_props, dfa);
        }
    }

    if (debug) {
        std::cout << "Conversion to DFA complete. Total states: " << boost::num_vertices(dfa) << std::endl;
    }
    return dfa;
}

