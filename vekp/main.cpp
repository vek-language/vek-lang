#include <exception>
#include <iostream>
#include <stdexcept>
#include <string>

#include "install.hpp"
#include "http.hpp"

using std::cerr;
using std::cout;
using std::runtime_error;
using std::string;

void report_error(const string& msg) {
    throw runtime_error("[Error] " + msg + "\n");
}

static void print_usage(std::ostream& os) {
    os << "====================================================\n"
          "Usage:\n"
          "  vekp version                       - Print package manager version\n"
          "  vekp install <author/name@tag>     - Install package\n"
          "====================================================\n";
}

int main(int argc, char **argv) {
    try {
        if (argc < 2) { 
            print_usage(cout);
            return 1;
        }

        string command = argv[1];
        
        if (command == "version" || command == "--version" || command == "-v") {
            // hard-coding the version since we don't have anything shared with compiler/vm.
            cout << "VEK Package Manager: v0.0.1" << "\n";
            return 0;
        }
        if (command == "help" || command == "--help" || command == "-h") {
            print_usage(cout);
            return 0;
        }

        if (command == "install") {
            if (argc < 3) {
                report_error("Missing package name.");
            }

            string package_name = argv[2];
            install_package(package_name);
            return 0;
        }

        report_error("Command not found.");
    }
    catch (const std::exception& e) {
        cerr << e.what() << '\n';
        return 1;
    }
    catch (...) {
        cerr << "[Error] An unexpected error occurred.\n";
        return 1;
    }

    return 0;
}
