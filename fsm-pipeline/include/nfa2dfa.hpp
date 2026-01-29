#pragma once
#include "graph_types.hpp"

using namespace env;


/**
 * @brief Converts an NFA into an equivalent DFA using the subset construction algorithm.
 * 
 * @param nfa The input NFA graph. Assumes no epsilon-transitions.
 * @return Graph A new graph representing the DFA.
 */
Graph NfaToDfa(Graph nfa);
