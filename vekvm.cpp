#include <iostream>
#include <string>

#include "./headers/vm.hpp"
#include "./headers/serializer.hpp"

using std::string;

using std::cerr;
using std::cout;
using std::cin;
using std::endl;

static void print_usage(std::ostream& os) {
    os << "====================================================\n"
          "Usage:\n"
          "  vekvm <file.vkc>   - Run the bytecode file\n"
          "====================================================\n";
}

int main(int argc, char* argv[]) {
    if (argc < 2) { print_usage(cerr); return 1; }

    string arg = argv[1];
    if (arg == "version" || arg == "--version" || arg == "-v") {
        cout << "VEK Virtual Machine 0.0.1" << endl;
        return 0;
    }
    
    Bytecode bytecode = Serializer::load(arg);
    
    VM vm;
    vm.run(bytecode);
    return 0;
}