#pragma once
#include "graph_types.hpp"

using namespace env;


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
string faToRegex(Graph graph);
