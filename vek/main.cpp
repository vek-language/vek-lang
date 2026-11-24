// The VEK compiler
#include <iostream>
#include <filesystem>
#include <fstream>
#include <sstream>

#include <string>
#include <vector>
#include <unordered_map>

#include "lexer.hpp"
#include "parser.hpp"
#include "compiler.hpp"
#include "vm.hpp"
#include "serializer.hpp"

// Native functions
#include "random.hpp"
#include "string.hpp"

#include "config.hpp"
#include "paths.hpp"

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
    string command;
    string input;

    // Optional
    string template_type;
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
        if (opts.command == "create") {
            cerr << "[Error] 'create' requires a <name> argument\n";
        } else {
            cerr << "[Error] '" << opts.command << "' requires a <file.ext> argument\n";
        }
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
        } else if (opts.command == "create" && opts.template_type.empty()) {
            opts.template_type = arg;
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
    try {
        Options opts;
        int parsed = parse_args(argc, argv, opts);
        if (parsed == 0) return 0;
        if (parsed < 0)  return 1;

        if (opts.command == "create") {
            if (opts.input.empty()) {
                throw runtime_error("Missing project name. Usage: vek create <name> [project|package]");
            }

            string type = opts.template_type.empty() ? "project" : opts.template_type;
            if (type == "pkg") type = "package";
            if (type == "proj") type = "project";

            if (type != "project" && type != "package") {
                throw runtime_error("Unknown template '" + type + "'. Available: project, package");
            }

            fs::path exec_dir = get_vek_dir();
            fs::path template_dir = exec_dir / "templates" / type;
            if (!fs::exists(template_dir)) {
                throw runtime_error("Template folder not found: " + template_dir.string());
            }

            fs::path target_dir = fs::current_path() / opts.input;
            if (fs::exists(target_dir)) {
                throw runtime_error("Directory already exists: " + opts.input);
            }

            fs::copy(template_dir, target_dir, fs::copy_options::recursive);

            fs::path toml_path = target_dir / "vek.toml";
            if (fs::exists(toml_path)) {
                ifstream in(toml_path);
                string content((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
                in.close();

                string pkg_id = opts.input;
                if (type == "package") {
                    for (char& c : pkg_id) {
                        if (c == '-') c = '_'; // prevent TokenType::MINUS on `load`
                    }
                }

                size_t pos = 0;
                while ((pos = content.find("{{NAME}}", pos)) != string::npos) {
                    content.replace(pos, 8, pkg_id);
                    pos += pkg_id.size();
                }

                ofstream out(toml_path);
                out << content;
            }

            cout << "Created " << type << " '" << opts.input << "'!\n";
            return 0;
        }

        if (opts.command == "info") {
            if (!opts.out.empty()) {
                throw runtime_error("-o is not valid for 'info'");
            }

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
            return 0;
        }

        // Build/Run
        string source = read_file(opts.input);

        Lexer lexer(source);
        auto tokens = lexer.lex();
        Parser parser(tokens);
        auto ast = parser.parse_program();

        Compiler compiler;
        Bytecode bytecode;

        register_all_natives(bytecode);

        auto exec_dir = get_vek_dir();
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
        cerr << "[Error] " << e.what() << "\n";
        return 1;
    } catch (...) {
        cerr << "[Error] An unexpected error occurred.\n";
        return 1;
    }

    return 0;
}