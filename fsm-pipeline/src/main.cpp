#include "graph_types.hpp"
#include "dfa2min_dfa.hpp"
#include "eps_nfa2nfa.hpp"
#include "min_dfa2regex.hpp"
#include "nfa2dfa.hpp"
#include "pipeline.hpp"
#include "regex2eps_nfa.hpp"

constexpr const char* typeToColor(VertexType c) noexcept {
    switch (c) {
        case VertexType::Default:  return "black";
        case VertexType::Terminal:    return "red";
        case VertexType::Start:  return "green";
        default:            return "purple";
    }
}


/**
 * @brief Parses command-line arguments to configure the program's execution.
 * @param argc Argument count.
 * @param argv Argument vector.
 * @return 0 on success, 1 if help message was displayed.
 * @throws std::exception on parsing errors.
 */
int parseCommandLine (int argc, char* argv []) {
    try {
        po::options_description desc("Allowed options");

        desc.add_options()
            ("help,h", "Produce help message")
            
            ("debug,d", po::bool_switch()->default_value(false), "Enable debug info")
            
            ("output-file,o", po::value<string>(), "Output file path")

            ("input-file,i", po::value<string>(), "Input file path. istream will be read by default.")

            ("first-stage,f", po::value<int>()->default_value(-1), "Number of the first processing stage of the FA or Regex; the numbers correspond to those given in the assignment.")

            ("last-stage,l", po::value<int>()->default_value(-1), "Number of the last processing stage of the FA or Regex; the numbers correspond to those given in the assignment.")

            ("input-type", po::value<string>(), "FA or Regex will be submitted as input. Possible inputs are `fa` and `regex`")
        ;

        po::variables_map vm;
        po::store(po::parse_command_line(argc, argv, desc), vm);

        po::notify(vm);


        if (vm.contains("help")) {
            std::cout << desc << "\n";
            return 1;
        }

        if (!vm.contains("input-type")) {
            throw po::required_option("--input-type");
        }

        if (vm.contains("input-file")) {
            std::ifstream input_stream = std::ifstream (vm["input-file"].as<string>());
            if (! input_stream.is_open()) {
                throw runtime_error("An error occurred while opening the file " + vm["input-file"].as<string>());
            }
            input = InputInterface (std::move(input_stream));
        }

        debug = vm["debug"].as<bool>();
        first_stage = vm["first-stage"].as<int>();
        last_stage = vm["last-stage"].as<int>();

        if (vm.contains("output-file")) {
            output_path = vm["output-file"].as<string>(); 
        }

        if (vm["input-type"].as<string>() == "fa") is_regex_input = false;
        else if (vm["input-type"].as<string>() == "regex") is_regex_input = true;
        else throw runtime_error("Invalid input type: only `fa` or `regex` are allowed");

    } catch (const po::error &ex) {
        throw;
    } catch (const std::exception &ex) {
        throw;
    }
    return 0;
}


/**
 * @brief Parses a JSON string representing a finite automaton and constructs a graph.
 * @param json_string The JSON string describing the FA's states, alphabet, start state, final states, and transitions.
 * @return A Graph representing the parsed finite automaton.
 * @throws runtime_error on parsing errors or invalid JSON structure.
 */
Graph parseFa (const string& regex) {
    json::value jv = json::parse(regex);

    if (!jv.is_object()) {
        throw runtime_error("JSON root is not an object");
    }
    const json::object& root = jv.as_object();

    Graph g;
    map<string, Vertex> state_map;

    // 2. Create vertex
    const json::array& states = root.at("S").as_array();
    for (const auto& state_val : states) {
        if (!state_val.is_string()) continue;
        
        string state_name(state_val.as_string());
        
        Vertex v = boost::add_vertex(g);
        
        try {
            g[v].name = std::stoi(state_name.substr(1));
        } catch (const std::exception&) {
            g[v].name = static_cast<int>(v);
        }
        g[v].type = VertexType::Default;

        state_map[state_name] = v;
    }

    // 3. Set vertex types
    
    string start_state_name(root.at("start").as_string());
    if (state_map.count(start_state_name)) {
        Vertex start_v = state_map.at(start_state_name);
        g[start_v].type = VertexType::Start;
    } else {
        throw runtime_error("Start state '" + start_state_name + "' not found in S");
    }

    const json::array& final_states = root.at("F").as_array();
    for (const auto& final_state_val : final_states) {
        string final_state_name(final_state_val.as_string());
        if (state_map.count(final_state_name)) {
            Vertex final_v = state_map.at(final_state_name);
            if (g[final_v].type == VertexType::Start) {
                g[final_v].type = VertexType::StartTerminal;
            } else {
                g[final_v].type = VertexType::Terminal;
            }
        } else {
            std::cerr << "Warning: Final state '" << final_state_name << "' not found in S" << std::endl;
        }
    }

    // 4. Create edges
    const json::array& delta = root.at("delta").as_array();
    for (const auto& transition_val : delta) {
        const json::object& trans = transition_val.as_object();
        
        string from_name(trans.at("from").as_string());
        string to_name(trans.at("to").as_string());
        string sym_str(trans.at("sym").as_string());

        if (state_map.count(from_name) && state_map.count(to_name) && !sym_str.empty()) {
            Vertex u = state_map.at(from_name);
            Vertex v = state_map.at(to_name);
            char symbol = sym_str[0];

            EdgeProperties props;
            props.symbol = symbol;

            boost::add_edge(u, v, props, g);
        } else {
            throw runtime_error("Invalid transition found in delta");
        }
    }

    return g;
}


/**
 * @brief Renders a graph representation of a finite automaton to a .dot file for visualization with Graphviz.
 * @param graph The FA graph to render.
 */
void renderGraph (Graph graph) {
    boost::dynamic_properties dp;
    Vertex ghost_vert = boost::add_vertex (VertexProperties(-1, VertexType::Ghost), graph);
    EdgeProperties props = {1.0, '`'};
    for (const auto& v : boost::make_iterator_range(boost::vertices(graph))) {
        if (graph[v].type == VertexType::Start || graph[v].type == VertexType::StartTerminal) {
            boost::add_edge(ghost_vert, v, props, graph);
            break;
        }
    }

    auto vertex_bundle_pm = boost::get(boost::vertex_bundle, graph);

    dp.property("node_id", boost::get(boost::vertex_index, graph));
    dp.property("label", boost::make_transform_value_property_map(
        [](const VertexProperties& p) { return p.name; }, vertex_bundle_pm));

    dp.property("color", boost::make_transform_value_property_map(
        [](const VertexProperties& p) { return typeToColor(p.type); }, vertex_bundle_pm));

    dp.property("peripheries", boost::make_transform_value_property_map(
        [](const VertexProperties& p) { return p.type == VertexType::Terminal || 
                                                p.type == VertexType::StartTerminal ? 2 : 1; }, vertex_bundle_pm));
    dp.property("style", boost::make_transform_value_property_map(
        [](const VertexProperties& p) { return p.type == VertexType::Ghost ? "invis" : ""; }, vertex_bundle_pm));

    dp.property("penwidth", boost::make_constant_property<Vertex>(2));

    dp.property("shape", boost::make_constant_property<Vertex>(string("circle")));


    auto edge_bundle_pm = boost::get(boost::edge_bundle, graph);
    dp.property("label", boost::make_transform_value_property_map(
        [](const EdgeProperties& p) -> string {
            if (p.symbol.has_value()) {
                if (p.symbol.value() == '`') {
                    return "";
                } 
                return string(1, p.symbol.value());
            } else {
                return "ε";
            }
        }, edge_bundle_pm));
    dp.property("labeldistance", boost::make_transform_value_property_map(
        [](const EdgeProperties& p) { return p.label_dist; }, edge_bundle_pm));

    std::ofstream dot_file(output_path.value());
    boost::write_graphviz_dp(dot_file, graph, dp);

    dot_file.close();
}


void printRegex (const string& output) {
    std::ofstream out (output_path.value());
    out << output << '\n';
}


/**
 * @brief Constructs the processing pipeline based on command-line arguments.
 *        It determines the start and end stages and adds the corresponding conversion functions to the pipeline.
 * @return A configured Pipeline object ready for execution.
 * @throws runtime_error for invalid stage configurations.
 */
Pipeline getPipeline() {
    if (is_regex_input) {
        if (first_stage != Stage::PARSE_REGEX && first_stage != Stage::DEFAULT_STAGE) {
            throw runtime_error("If you provide a regex as input, the program must start from the first stage");
        }
        first_stage = Stage::PARSE_REGEX;
        if (last_stage == Stage::DEFAULT_STAGE) {
            last_stage = Stage::DFA_TO_MIN_DFA; // Defaults to 4 stage.
        }
    } else {
        if (first_stage == Stage::PARSE_REGEX) {
            throw runtime_error("Execution starting from the first stage requires a regex input.");
        }
        if (first_stage == Stage::DEFAULT_STAGE) {
            first_stage = Stage::EPS_NFA_TO_NFA; // By default, start from the second stage.
        }
        if (last_stage == Stage::DEFAULT_STAGE) {
            last_stage = Stage::FA_TO_REGEX; // Defaults to the end.
        }
    }

    if (last_stage < first_stage || last_stage > Stage::FA_TO_REGEX || first_stage < Stage::PARSE_REGEX) {
        throw runtime_error("Invalid stage range provided.");
    }

    auto start_stage = static_cast<Stage>(first_stage);
    auto end_stage = static_cast<Stage>(last_stage);
    if (end_stage == Stage::FA_TO_REGEX && !output_path.has_value()) {
        output_path = output_path_for_regex;
    } else if (!output_path.has_value()) {
        output_path = output_path_for_fa;
    }

    // === 2. Construct pipeline ===

    Pipeline p;
    if (start_stage > Stage::PARSE_REGEX) {
        p.add_step(parseFa);
    }

    for (int stage = start_stage; stage <= end_stage; stage++) {
        switch (static_cast<Stage>(stage)) {
            case Stage::PARSE_REGEX:
                p.add_step(parseRegex);
                break;
            case Stage::EPS_NFA_TO_NFA:
                p.add_step(epsNfaToNfa);
                break;
            case Stage::NFA_TO_DFA:
                p.add_step(NfaToDfa);
                break;
            case Stage::DFA_TO_MIN_DFA:
                p.add_step(DfaToMinDfa);
                break;
            case Stage::FA_TO_REGEX:
                p.add_step(faToRegex);
                break;
            default:
                break;
        }
    }

    // === 3. Add final step ===

    if (end_stage >= Stage::PARSE_REGEX && end_stage <= Stage::DFA_TO_MIN_DFA) {
        p.add_step(renderGraph);
    } else {
        p.add_step(printRegex);
    }

    return p;
}


int main(int argc, char* argv[]) {
    if (parseCommandLine(argc, argv)) return 0;
    Pipeline p = getPipeline();

    std::cout << debug << '\n';
    string inp_str = input.readFileToString();
    p.execute(inp_str);

    return 0;
}