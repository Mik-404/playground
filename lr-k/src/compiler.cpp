#include <argparse/argparse.hpp>
#include <filesystem>
#include "builder.hpp"
#include "connector.hpp"

#ifndef K
#define K 1
#endif

int main (int argc, char* argv[]) {
    argparse::ArgumentParser program("LR-k generator");
    program.add_argument("-o", "--output").default_value(std::string("table"))
            .help("specify the output filename");
    program.add_argument("-i", "--input").required()
            .help("specify the input file with grammar");

    try {
        program.parse_args(argc, argv);
    } catch (const std::exception& err) {
        std::cerr << err.what() << std::endl;
        std::cerr << program;
        std::exit(1);
    }
    constexpr int Lookahead = K;
    std::string output_filename = program.get<std::string> ("--output");
    std::filesystem::path input_filename = program.get<std::string> ("--input");

    Grammar grammar (input_filename);
    GrammarAnalysis<K> first_k_gen (grammar);
    first_k_gen.build_kfirst();
    Builder<K> table_generator (grammar, first_k_gen);
    table_generator.build();
    Exporter exporter (grammar);
    exporter.export_to_file(table_generator, output_filename);
}