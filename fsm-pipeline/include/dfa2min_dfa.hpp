#pragma once
#include "graph_types.hpp"

using namespace env;

/**
 * @brief Minimizes a DFA using the partition refinement algorithm.
 * 
 * @param dfa The input DFA graph.
 * @return Graph A new graph representing the minimized DFA.
 */
Graph DfaToMinDfa(Graph dfa);
