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

// Native functions
#include "./natives/random.hpp"
#include "./natives/string.hpp"

#include "./configs/config.hpp"

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

inline void register_all_natives(Bytecode& bc) {
    register_random_natives(bc);
    register_string_natives(bc);
}

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
          "  vek version                                - Print compiler version\n"
          "  vek create <name>     [<project|package>]  - Create a VEK project\n"
          "  vek info   <file.vkc>                      - Print metadata of a compiled file\n"
          "  vek build  <file.vek> [-o <dir|file>]      - Compile <file>.vek\n"
          "  vek run    <file.vek>                      - Compile and execute\n"
          "====================================================\n";
}

struct Options {
    string  command;
    string input;

    fs::path out; // -o, empty if not given
};

/* 
Returns:
   1  = proceed to normal command handling
   0  = already handled (version/help printed), exit 0
  -1  = error, exit 1 
*/
static int parse_args(int argc, char* argv[], Options& opts) {
    if (argc < 2) {
        print_usage(cerr);
        return -1;
    }

    opts.command = argv[1];

    if (opts.command == "version" || opts.command == "--version" || opts.command == "-v") {
        cout << vek::config::NAME << " language: v" << vek::config::VERSION << "\n";
        cout << vek::config::NAME << " bytecode version: " << vek::config::BYTECODE_VERSION << "\n";
        return 0;
    }
    if (opts.command == "help" || opts.command == "--help" || opts.command == "-h") {
        print_usage(cout);
        return 0;
    }

    if (opts.command != "build" && opts.command != "run" && opts.command != "info" && opts.command != "create") {
        cerr << "Unknown command: " << opts.command << "\n";
        print_usage(cerr);
        return -1;
    }

    if (argc < 3) {
        cerr << "[Error] '" << opts.command << "' requires a <file.ext> argument\n";
        print_usage(cerr);
        return -1;
    }

    opts.input = argv[2];

    for (int i = 3; i < argc; ++i) {
        string arg = argv[i];
        if (arg == "-o" || arg == "--out") {
            if (opts.command != "build") {
                cerr << "[Error] " << arg << " is only valid for 'build'\n";
                return -1;
            }
            if (i + 1 >= argc) {
                cerr << "[Error] " << arg << " requires a path\n";
                return -1;
            }
            opts.out = argv[++i];
        } else {
            cerr << "[Error] unknown argument: " << arg << "\n";
            return -1;
        }
    }

    return 1;
}

static fs::path resolve_output(const Options& opts) {
    fs::path in = opts.input;

    if (opts.out.empty()) {
        return in.replace_extension(".vkc");
    }

    if (opts.out.extension() != ".vkc") {
        fs::create_directories(opts.out);
        return opts.out / in.filename().replace_extension(".vkc");
    }

    if (opts.out.has_parent_path()) {
        fs::create_directories(opts.out.parent_path());
    }
    return opts.out;
}

int main(int argc, char* argv[]) {
    Options opts;
    int parsed = parse_args(argc, argv, opts);
    if (parsed == 0) return 0;
    if (parsed < 0)  return 1;

    if (opts.command == "create") {
        return 0;
    }
    
    if (opts.command == "info") {
        if (!opts.out.empty()) {
            cerr << "[Error] -o is not valid for 'info'\n";
            return 1;
        }
        try {
            Bytecode bc = Serializer::load(opts.input);

            if (bc.metadata.name.empty() && bc.metadata.version.empty()
                && bc.metadata.author.empty() && bc.metadata.description.empty()) {
                cout << "(no metadata)\n";
            } else {
                if (!bc.metadata.name.empty())
                    cout << bc.metadata.name;
                if (!bc.metadata.version.empty())
                    cout << " v" << bc.metadata.version;
                cout << "\n";
                if (!bc.metadata.author.empty())
                    cout << "by " << bc.metadata.author << "\n";
                if (!bc.metadata.description.empty())
                    cout << bc.metadata.description << "\n";
            }
        } catch (const std::exception& e) {
            cerr << "\n[Error] " << e.what() << "\n";
            return 1;
        }
        return 0;
    }

    try {
        string source = read_file(opts.input);

        Lexer lexer(source);
        auto tokens = lexer.lex();
        Parser parser(tokens);
        auto ast = parser.parse_program();

        Compiler compiler;
        Bytecode bytecode;

        register_all_natives(bytecode);

        auto exec_dir = fs::weakly_canonical(fs::path(argv[0])).parent_path();
        compiler.compile(ast, bytecode, opts.input, exec_dir);

        if (opts.command == "build") {
            fs::path out = resolve_output(opts);
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