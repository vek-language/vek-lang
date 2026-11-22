#pragma once

#include <string_view>
#include <array>
#include <cstdint>

using std::string_view;
using std::array;

namespace vek::config {
    // Shared
    inline constexpr string_view NAME = "VEK";
    inline constexpr uint16_t BYTECODE_VERSION = 3;
    inline constexpr array<char, 4> MAGIC = {'V', 'E', 'K', 'L'};
    
    // Language & Compiler
    inline constexpr string_view VERSION = "0.0.3";

    // Virtual Machine
    inline constexpr string_view VM_VERSION = "0.0.3";
}