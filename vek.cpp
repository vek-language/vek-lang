// The VEK compiler
#include <iostream>
#include <filesystem>
#include <fstream>
#include <sstream>

#include <string>
#include <vector>
#include <unordered_map>

#include "./headers/lexer.hpp"
#include "./headers/parser.hpp"
#include "./headers/compiler.hpp"
#include "./headers/vm.hpp"
#include "./headers/serializer.hpp"

namespace fs = std::filesystem;
using std::runtime_error;

using std::cerr;
using std::cout;
using std::cin;
using std::endl;

using std::string;
using std::vector;

using std::ifstream;
using std::stringstream;

string read_file(const string& path) {
    ifstream file(path);
    if (!file.is_open()) {
        throw runtime_error("Can't open file: " + path);
    }

    stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

static void print_usage(std::ostream& os) {
    os << "====================================================\n"
          "Usage:\n"
          "  vek version            - Print compiler version\n"
          "  vek build <file.vek>   - Compile <file>.vek to <file>.vkc\n"
          "  vek run   <file.vek>   - Compile and execute\n"
          "====================================================\n";
}

int main(int argc, char* argv[]) {
    if (argc < 2) { print_usage(cerr); return 1; }

    const string command = argv[1];

    if (command == "version") {
        cout << "VEK language 0.0.1" << endl;
        return 0;
    }
    if (command == "help" || command == "--help" || command == "-h") {
        print_usage(cout);
        return 0;
    }

    // Validate the command
    if (command != "build" && command != "run") {
        cerr << "Unknown command: " << command << "\n";
        print_usage(cerr);
        return 1;
    }

    if (argc < 3) {
        cerr << "error: '" << command << "' requires a <file.vek> argument\n";
        print_usage(cerr);
        return 1;
    }

    const string path = argv[2];

    try {
        std::string source = read_file(path);

        Lexer lexer(source);
        auto tokens = lexer.lex();
        Parser parser(tokens);
        auto ast = parser.parse_program();

        Compiler compiler;
        Bytecode bytecode = compiler.compile(ast);

        if (command == "build") {
            fs::path out = fs::path(path).replace_extension(".vkc");
            Serializer::save(bytecode, out.string());
            cout << "Successfully compiled to " << out.string() << "\n";
        } else {
            VM vm;
            vm.run(bytecode);
        }

    } catch (const std::exception& e) {
        cerr << "\n[Error] " << e.what() << "\n";
        return 1;
    }

    return 0;
}