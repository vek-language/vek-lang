#include <iostream>
#include <string>

#include "./headers/vm.hpp"
#include "./headers/serializer.hpp"

using std::string;
using std::cerr;

static void print_usage(std::ostream& os) {
    os << "====================================================\n"
          "Usage:\n"
          "  vekvm <file.vkc>   - Run the bytecode file\n"
          "====================================================\n";
}

int main(int argc, char* argv[]) {
    if (argc < 2) { print_usage(cerr); return 1; }

    std::string path = argv[1];
    Bytecode bytecode = Serializer::load(path);
    
    VM vm;
    vm.run(bytecode);
    return 0;
}