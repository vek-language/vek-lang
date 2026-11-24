// The VEK VM
#include <iostream>
#include <string>

#include "vm.hpp"
#include "serializer.hpp"

// Native functions
#include "random.hpp"
#include "string.hpp"

#include "config.hpp"

using std::string;

using std::cerr;
using std::cout;
using std::cin;
using std::endl;

inline void register_all_natives(Bytecode& bc) {
    register_random_natives(bc);
    register_string_natives(bc);
}

static void print_usage(std::ostream& os) {
    os << "====================================================\n"
          "Usage:\n"
          "  vekvm version      - Print virtual machine version\n"
          "  vekvm <file.vkc>   - Run the bytecode file\n"
          "====================================================\n";
}

int main(int argc, char* argv[]) {
    if (argc < 2) { print_usage(cerr); return 1; }

    string arg = argv[1];
    if (arg == "version" || arg == "--version" || arg == "-v") {
        cout << vek::config::NAME << " Virtual Machine: v" << vek::config::VM_VERSION << endl;
        cout << vek::config::NAME << " bytecode version: " << vek::config::BYTECODE_VERSION << endl;
        return 0;
    }
    
    Bytecode bytecode = Serializer::load(arg);
    register_all_natives(bytecode);    

    VM vm;
    vm.run(bytecode);
    return 0;
}