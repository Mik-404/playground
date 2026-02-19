#include <argparse/argparse.hpp>
#include "connector.hpp"

#ifndef K
#define K 1
#endif

int main (int argc, char* argv[]) {
    argparse::ArgumentParser program("LR-k parser");
    program.add_argument("-o", "--output").default_value(std::string("tree"))
            .help("output file for the parse tree");
    program.add_argument("-i", "--input_table").required()
            .help("path to the grammar definition file.");
    program.add_argument("-t", "--text").required()
            .help("path to the input text file");

    try {
        program.parse_args(argc, argv);
    } catch (const std::exception& err) {
        std::cerr << err.what() << std::endl;
        std::cerr << program;
        std::exit(1);
    }
    constexpr int Lookahead = K;
    std::string output_filename = program.get<std::string> ("--output");
    std::filesystem::path table_filename = program.get<std::string> ("--input_table");
    std::filesystem::path input_text_filename = program.get<std::string> ("--text");

    Importer importer;
    RuntimeParser<Lookahead> parser;
    importer.import_from_file<Lookahead>(parser, table_filename);
    std::string input_text = _read_file(input_text_filename);
    std::vector<uint32_t> derivation = parser.parse(input_text); // the rest is up to you

    std::ofstream out(output_filename);
    if (!out.is_open()) throw std::runtime_error("Fail");

    for (const auto& val : derivation) {
        out << val << " ";
    }
    out << "\n";

}