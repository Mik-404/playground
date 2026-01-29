#pragma once
#include "graph_types.hpp"

using namespace env;


/**
 * @brief Converts an ε-NFA into an equivalent NFA without ε-transitions.
 * 
 * @param eps_nfa The input graph representing the ε-NFA.
 * @return Graph A new graph representing the NFA.
 */
Graph epsNfaToNfa(Graph eps_nfa);
