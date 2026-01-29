#pragma once
#include "graph_types.hpp"

using namespace env;

/**
 * @brief Parses a regex string and converts it into an epsilon-NFA graph.
 * @param regex The input regular expression.
 * @return A Graph representing the corresponding epsilon-NFA.
 */
Graph parseRegex (const string& regex);
