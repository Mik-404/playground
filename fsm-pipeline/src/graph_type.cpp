#include "graph_types.hpp"


const std::string epsilon_utf8 = "ϵ"; // 𝜀 in UTF-8
const std::string replace_eps = "$";
const string output_path_for_fa = "out_graph.dot";
const string output_path_for_regex = "regex.txt";


namespace env {
    std::optional<string> output_path = std::nullopt;
    InputInterface input;
    bool debug;
    int first_stage;
    int last_stage;
    bool is_regex_input;
};
