#pragma once

#include <any>
#include <fstream>
#include <functional>
#include <iostream>
#include <queue>
#include <string>

#include <boost/graph/adjacency_list.hpp>
#include <boost/graph/graphviz.hpp>
#include <boost/program_options.hpp>
#include <boost/json.hpp>

namespace po = boost::program_options;
namespace json = boost::json;

/**
 * @enum VertexType
 * @brief Defines the type of a vertex in the finite automaton graph.
 */
enum class VertexType {
    Start,
    Terminal,
    Default,
    StartTerminal,
    Ghost
};


/**
 * @enum Stage
 * @brief Represents the different processing stages in the FA/Regex conversion pipeline.
 */
enum Stage {
    DEFAULT_STAGE = -1,
    PARSE_REGEX = 1,
    EPS_NFA_TO_NFA = 2,
    NFA_TO_DFA = 3,
    DFA_TO_MIN_DFA = 4,
    FA_TO_REGEX = 5
};


/**
 * @struct VertexProperties
 * @brief Stores properties associated with each vertex in the graph.
 */
struct VertexProperties {
    int name;
    VertexType type;
};


/**
 * @struct EdgeProperties
 * @brief Stores properties associated with each edge in the graph.
 */
struct EdgeProperties {
    double label_dist;
    std::optional<char> symbol;
};


using Graph = boost::adjacency_list<boost::vecS, boost::vecS, boost::directedS, 
                                    VertexProperties, 
                                    EdgeProperties>;

using Vertex = boost::graph_traits<Graph>::vertex_descriptor;
using Edge = boost::graph_traits<Graph>::edge_descriptor;
using PipelineStep = std::function<std::any(std::any&&)>;

template<typename T>
using vector = std::vector<T>;

template<typename T>
using set = std::set<T>;

using string = std::string;
using runtime_error = std::runtime_error;

template<typename T, typename U>
using map = std::map<T, U>;


/**
 * @struct InputInterface
 * @brief A wrapper to handle input from either a standard input stream (std::cin) or a file stream (std::ifstream) seamlessly.
 */
struct InputInterface {

    std::variant<std::ifstream, std::istream*> input_holder;

    InputInterface(): input_holder(&std::cin) {}
    InputInterface(std::istream& input): input_holder(&input) {}
    InputInterface(std::ifstream&& input): input_holder(std::move(input)) {}
    InputInterface& operator= (InputInterface&& other) {
        if (std::holds_alternative<std::istream*>(other.input_holder)) {
            input_holder = std::get<std::istream*> (other.input_holder);
        } else {
            input_holder = std::move (std::get<std::ifstream> (other.input_holder));
        }
        return *this;
    }

    InputInterface& operator>>(auto& some_obj) {
        if (std::holds_alternative<std::istream*>(input_holder)) {
            *std::get<std::istream*> (input_holder) >> some_obj;
        } else {
            std::get<std::ifstream> (input_holder) >> some_obj;
        }
        return *this;
    }

    /**
     * @brief Reads the entire content of the associated input stream into a string.
     * @return A string containing the full content of the input stream.
     */
    string readFileToString() {
        if (std::holds_alternative<std::istream*>(input_holder)) {
            return string((std::istreambuf_iterator<char>(*std::get<std::istream*> (input_holder))),
                            std::istreambuf_iterator<char>());
        }
        return string((std::istreambuf_iterator<char>(std::get<std::ifstream> (input_holder))),
                            std::istreambuf_iterator<char>());
    }
};


extern const string epsilon_utf8;
extern const string replace_eps;
extern const string output_path_for_fa;
extern const string output_path_for_regex;

namespace env {
    extern std::optional<string> output_path;
    extern InputInterface input;
    extern bool debug;
    extern int first_stage;
    extern int last_stage;
    extern bool is_regex_input;
}

using namespace env;
