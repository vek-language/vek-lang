#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include <variant>
#include <unordered_map>
#include "./token.hpp"

using std::variant;
using std::string;
using std::vector;
using std::unordered_map;
using std::shared_ptr;

struct ArrayObject;
struct Cell;

using CellPtr = std::shared_ptr<Cell>;
using Value = std::variant<
    std::monostate, 
    int32_t, int64_t, float, double, bool, char, string, 
    shared_ptr<ArrayObject>, 
    CellPtr
>;

struct ArrayObject {
    vector<Value> elements;
};

struct Cell { 
    Value value; 
};

enum class OpCode {
    // Stack and Math
    OP_CONSTANT = 0x01,
    
    OP_ADD = 0x02,
    OP_SUBTRACT = 0x03,
    OP_MULTIPLY = 0x04,
    OP_DIVIDE = 0x05,

    // Logic
    OP_GREATER_THAN = 0x06,
    OP_LESS_THAN = 0x07,

    OP_GREATER_EQUAL = 0x08,
    OP_LESS_EQUAL = 0x09,
    
    OP_EQUAL = 0x0A,
    OP_NOT = 0x0B,

    // Vars
    OP_SAVE = 0x10,

    OP_LOAD = 0x11,
    OP_DEFINE_VAR = 0x12,

    OP_LOAD_REF = 0x13,
    OP_DEFINE_REF_VAR = 0x14,

    OP_POP = 0x15,
    OP_ENTER_SCOPE = 0x16,
    OP_EXIT_SCOPE = 0x17,
    OP_CAST = 0x18,
    OP_DUPLICATE = 0x19,

    // Control Flow
    OP_RETURN = 0x20,
    OP_CALL = 0x21,

    OP_JUMP = 0x22,
    OP_JUMP_IF_FALSE = 0x23,
    OP_JUMP_IF_TRUE = 0x24,

    // System Functions
    OP_PRINT = 0x30,

    // Arrays
    OP_NEW_ARRAY = 0x40,

    OP_GET_INDEX = 0x41,
    OP_SET_INDEX = 0x42, 

    OP_ARRAY_PUSH = 0x43,
    OP_ARRAY_POP = 0x44,

    // End of code
    OP_HALT = 0xFF,
};

struct Bytecode
{
    vector<uint8_t> code;
    vector<Value> constants;
    unordered_map<string, size_t> function_addresses;

    void write(uint8_t byte) {
        code.push_back(byte);
    }

    int add_const(Value val) {
        constants.push_back(val);
        return constants.size() - 1;
    }

    template <typename... Args> // Function overloading would look a little bit weird there
    void emit(OpCode op, Args... args) {
        write(static_cast<uint8_t>(op));
        (write(static_cast<uint8_t>(args)), ...); // Fold expression (c++17)
    }
};
