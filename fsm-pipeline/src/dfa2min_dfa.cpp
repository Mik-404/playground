#include "dfa2min_dfa.hpp"

/**
 * @brief Finds the target vertex for a given transition from a source vertex on a specific symbol.
 * @param u The source vertex.
 * @param symbol The transition symbol.
 * @param g The graph to search within.
 * @return An std::optional containing the target vertex if the transition exists, otherwise std::nullopt.
 */
std::optional<Vertex> find_transition_target(Vertex u, char symbol, const Graph& g) {
    for (const auto& edge : boost::make_iterator_range(boost::out_edges(u, g))) {
        if (g[edge].symbol.has_value() && g[edge].symbol.value() == symbol) {
            return boost::target(edge, g);
        }
    }
    return std::nullopt;
}


/**
 * @brief Completes a DFA by adding a trap (sink) state.
 * @param dfa Graph to be completed (modified in place).
 * @param alphabet Set of symbols in the automaton alphabet.
 */
void CompleteDfa(Graph& dfa, const set<char>& alphabet) {
    Vertex trap_state = boost::add_vertex(dfa);
    dfa[trap_state].type = VertexType::Default;
    dfa[trap_state].name = -1;

    for (char c : alphabet) {
        boost::add_edge(trap_state, trap_state, {1.0, c}, dfa);
    }

    vector<Vertex> vertices_copy;
    for (const auto& v : boost::make_iterator_range(boost::vertices(dfa))) {
        if (v != trap_state) {
            vertices_copy.push_back(v);
        }
    }
    
    for (const auto& v : vertices_copy) {
        for (char c : alphabet) {
            if (!find_transition_target(v, c, dfa).has_value()) {
                boost::add_edge(v, trap_state, {1.0, c}, dfa);
            }
        }
    }
}


/**
 * @brief Minimizes a DFA using the partition refinement algorithm.
 * 
 * @param dfa The input DFA graph.
 * @return Graph A new graph representing the minimized DFA.
 */
Graph DfaToMinDfa(Graph dfa) {
    if (boost::num_vertices(dfa) == 0) {
        return Graph{};
    }

    // --- Step 1: Get alphabet ---
    set<char> alphabet;
    for (const auto& e : boost::make_iterator_range(boost::edges(dfa))) {
        if (dfa[e].symbol.has_value()) {
            alphabet.insert(dfa[e].symbol.value());
        }
    }

    CompleteDfa(dfa, alphabet);

    // --- Step 2: Initial partition ---
    std::list<set<Vertex>> partitions;
    set<Vertex> accepting_states;
    set<Vertex> non_accepting_states;
    std::optional<Vertex> start_vertex;

    for (const auto& v : boost::make_iterator_range(boost::vertices(dfa))) {
        if (dfa[v].type == VertexType::Terminal || dfa[v].type == VertexType::StartTerminal) {
            accepting_states.insert(v);
        } else {
            non_accepting_states.insert(v);
        }
        if (dfa[v].type == VertexType::Start || dfa[v].type == VertexType::StartTerminal) {
            start_vertex = v;
        }
    }
    
    if (!start_vertex) {
         throw runtime_error("Start state not found in DFA for minimization.");
    }

    if (!accepting_states.empty()) partitions.push_back(accepting_states);
    if (!non_accepting_states.empty()) partitions.push_back(non_accepting_states);

    // --- Step 3: Worklist ---
    std::list<set<Vertex>> worklist;
    if (accepting_states.size() <= non_accepting_states.size() && !accepting_states.empty()) {
        worklist.push_back(accepting_states);
    } else if (!non_accepting_states.empty()) {
        worklist.push_back(non_accepting_states);
    }

    // --- Step 4: Refinement loop ---
    while (!worklist.empty()) {
        set<Vertex> A = worklist.front();
        worklist.pop_front();

        for (char c : alphabet) {
            // S = {u | transition(u, c) is in A}
            set<Vertex> S;
            for (const auto& u : boost::make_iterator_range(boost::vertices(dfa))) {
                if(auto target_v_opt = find_transition_target(u, c, dfa)) {
                    if (A.count(*target_v_opt)) {
                        S.insert(u);
                    }
                }
            }

            if (S.empty()) continue;

            for (auto it = partitions.begin(); it != partitions.end(); ) {
                set<Vertex>& P = *it;
                
                set<Vertex> intersection;
                set_intersection(P.begin(), P.end(), S.begin(), S.end(),
                                      std::inserter(intersection, intersection.begin()));

                if (intersection.empty() || intersection.size() == P.size()) {
                    ++it;
                    continue;
                }

                set<Vertex> difference;
                set_difference(P.begin(), P.end(), intersection.begin(), intersection.end(),
                                    std::inserter(difference, difference.begin()));
                
                // Replace P with the two new sets
                *it = intersection;
                it = partitions.insert(std::next(it), difference); 

                // Update worklist
                bool p_in_worklist = false;
                for(auto& w_item : worklist) {
                    if (w_item == P) {
                        w_item = intersection;
                        worklist.push_back(difference);
                        p_in_worklist = true;
                        break;
                    }
                }

                if (!p_in_worklist) {
                    if (intersection.size() <= difference.size()) {
                        worklist.push_back(intersection);
                    } else {
                        worklist.push_back(difference);
                    }
                }
            }
        }
    }
    
    // --- Step 5: Construct the minimized DFA ---
    Graph min_dfa;
    map<int, Vertex> partition_to_new_vertex;
    map<Vertex, int> old_vertex_to_partition_id;
    int partition_id_counter = 0;

    for (const auto& p : partitions) {
        Vertex new_v = boost::add_vertex(min_dfa);
        partition_to_new_vertex[partition_id_counter] = new_v;
        
        // === FIX START ===
        // Determine the properties of the new state independently
        bool is_start = p.count(*start_vertex);
        // All states in a partition are either all accepting or all non-accepting.
        // So we just need to check one of them (the representative).
        Vertex rep_v = *p.begin(); 
        bool is_terminal = (dfa[rep_v].type == VertexType::Terminal || dfa[rep_v].type == VertexType::StartTerminal);

        // Combine the properties to set the final correct type
        if (is_start && is_terminal) {
            min_dfa[new_v].type = VertexType::StartTerminal;
        } else if (is_start) {
            min_dfa[new_v].type = VertexType::Start;
        } else if (is_terminal) {
            min_dfa[new_v].type = VertexType::Terminal;
        } else {
            min_dfa[new_v].type = VertexType::Default;
        }
        // === FIX END ===

        min_dfa[new_v].name = partition_id_counter;

        for (const auto& old_v : p) {
            old_vertex_to_partition_id[old_v] = partition_id_counter;
        }
        partition_id_counter++;
    }

    // --- Step 6: Create transitions ---
    for (const auto& p : partitions) {
        Vertex rep_v = *p.begin(); 
        int source_partition_id = old_vertex_to_partition_id.at(rep_v);
        Vertex source_new_v = partition_to_new_vertex.at(source_partition_id);

        for (char c : alphabet) {
            if (auto target_v_opt = find_transition_target(rep_v, c, dfa)) {
                int target_partition_id = old_vertex_to_partition_id.at(*target_v_opt);
                Vertex target_new_v = partition_to_new_vertex.at(target_partition_id);
                
                EdgeProperties props = {1.0, c};
                boost::add_edge(source_new_v, target_new_v, props, min_dfa);
            }
        }
    }
    
    if (debug) {
        std::cout << "Minimization complete. Total states: " << boost::num_vertices(min_dfa) << std::endl;
    }
    return min_dfa;
}