#pragma once

#include <iostream>
#include <cmath>
#include <vector>
#include <stdexcept>
#include <string>
#include <variant>
#include <unordered_map>
#include <memory>
#include "bytecode.hpp"
#include "token.hpp"

using std::runtime_error;
using std::vector;
using std::string;
using std::get;
using std::holds_alternative;
using std::unordered_map;
using std::shared_ptr;
using std::make_shared;

class VM {
private:
    static constexpr size_t STACK_SIZE = 4096;

    vector<Value> stack;
    vector<unordered_map<string, Value>> scopes;
    
    vector<size_t> call_stack;
    vector<size_t> scope_base_stack;

    void push(Value val) {
        if (stack.size() >= STACK_SIZE) throw runtime_error("Stack overflow!");
        stack.push_back(val);
    }

    Value pop() {
        if (stack.empty()) throw runtime_error("Stack underflow!");
        Value val = stack.back();
        stack.pop_back();
        return val;
    }

    // Helpers
    string value_to_string(const Value& v) {
        if (holds_alternative<string>(v)) return get<string>(v);
        if (holds_alternative<char>(v)) return string(1, get<char>(v));
        if (holds_alternative<bool>(v)) return get<bool>(v) ? "true" : "false";
        if (holds_alternative<int32_t>(v)) return std::to_string(get<int32_t>(v));
        if (holds_alternative<int64_t>(v)) return std::to_string(get<int64_t>(v));
        if (holds_alternative<float>(v)) return std::to_string(get<float>(v));
        if (holds_alternative<double>(v)) {
            double d = get<double>(v);
            if (std::isinf(d)) return d > 0 ? "inf" : "-inf";
            if (std::isnan(d)) return "nan";
            return std::to_string(d);
        }
        if (holds_alternative<shared_ptr<ArrayObject>>(v)) {
            auto arr = get<shared_ptr<ArrayObject>>(v);
            if (!arr) return "null";
            string out = "[";
            for (size_t i = 0; i < arr->elements.size(); ++i) {
                out += value_to_string(arr->elements[i]);
                if (i < arr->elements.size() - 1) out += ", ";
            }
            return out + "]";
        }
        return "null";
    }

     bool is_truthy(const Value& v) {
        if (holds_alternative<bool>(v)) return get<bool>(v);
        if (holds_alternative<int32_t>(v)) return get<int32_t>(v) != 0;
        if (holds_alternative<int64_t>(v)) return get<int64_t>(v) != 0;
        if (holds_alternative<float>(v)) return get<float>(v) != 0.0f;
        if (holds_alternative<double>(v)) return get<double>(v) != 0.0;
        if (holds_alternative<char>(v)) return get<char>(v) != '\0';
        if (holds_alternative<string>(v)) return !get<string>(v).empty();
        if (holds_alternative<shared_ptr<ArrayObject>>(v)) {
            const auto& arr = get<shared_ptr<ArrayObject>>(v);
            return arr && !arr->elements.empty();
        }
        
        return false;
    }

    struct MathValues {
        bool is_float;
        long long i_left, i_right;
        double f_left, f_right;
    };

    MathValues extract_math_operands(Value a, Value b, const string& op_name) {
        MathValues mv;
        mv.is_float = false;
        auto is_num = [](const Value& v) { 
            return holds_alternative<int32_t>(v) || holds_alternative<int64_t>(v) ||
                   holds_alternative<float>(v) || holds_alternative<double>(v); 
        };
        if (!is_num(a) || !is_num(b)) throw runtime_error("Operands must be numbers: " + op_name); // op_name for debugging

        if (holds_alternative<float>(a) || holds_alternative<double>(a) || 
            holds_alternative<float>(b) || holds_alternative<double>(b)) {
            mv.is_float = true;
            auto to_double = [](const Value& v) {
                if (holds_alternative<int32_t>(v)) return static_cast<double>(get<int32_t>(v));
                if (holds_alternative<int64_t>(v)) return static_cast<double>(get<int64_t>(v));
                if (holds_alternative<float>(v)) return static_cast<double>(get<float>(v));
                return get<double>(v);
            };
            mv.f_left = to_double(a);
            mv.f_right = to_double(b);
        } else {
            auto to_int = [](const Value& v) {
                if (holds_alternative<int32_t>(v)) return static_cast<long long>(get<int32_t>(v));
                return get<int64_t>(v);
            };
            mv.i_left = to_int(a);
            mv.i_right = to_int(b);
        }
        return mv;
    }

public:
    void run(const Bytecode& bytecode) {
        size_t ip = 0;
        scopes.push_back({});

        while(ip < bytecode.code.size()) {
            OpCode instruction = static_cast<OpCode>(bytecode.code[ip++]);

            switch(instruction) {
                // Push
                case OpCode::OP_CONSTANT: {
                    uint8_t constant_index = bytecode.code[ip++];
                    push(bytecode.constants[constant_index]);
                    break;
                }

                // Math
                case OpCode::OP_ADD: {
                    Value b = pop(); Value a = pop();
                    // strings & chars
                    if (holds_alternative<string>(a) || holds_alternative<string>(b) || holds_alternative<char>(a) || holds_alternative<char>(b)) {
                        push(value_to_string(a) + value_to_string(b));
                    } else {
                        MathValues mv = extract_math_operands(a, b, "ADD");
                        if (mv.is_float) push(mv.f_left + mv.f_right);
                        else push(mv.i_left + mv.i_right);
                    }
                    break;
                }
                case OpCode::OP_SUBTRACT: {
                    Value b = pop(); Value a = pop();
                    MathValues mv = extract_math_operands(a, b, "SUBTRACT");
                    if (mv.is_float) push(mv.f_left - mv.f_right);
                    else push(mv.i_left - mv.i_right);
                    break;
                }
                case OpCode::OP_MULTIPLY: {
                    Value b = pop(); Value a = pop();
                    MathValues mv = extract_math_operands(a, b, "MULTIPLY");
                    if (mv.is_float) push(mv.f_left * mv.f_right);
                    else push(mv.i_left * mv.i_right);
                    break;
                }
                case OpCode::OP_DIVIDE: {
                    Value b = pop(); Value a = pop();
                    MathValues mv = extract_math_operands(a, b, "DIVIDE");
                    if (mv.is_float) {
                        // Float
                        push(mv.f_left / mv.f_right);
                    } else {
                        // Integer
                        if (mv.i_right == 0) {
                            throw runtime_error("Division by zero");
                        }
                        push(mv.i_left / mv.i_right);
                    }
                    break;
                }

                // Logic
                case OpCode::OP_GREATER_THAN: {
                    Value b = pop(); Value a = pop();
                    MathValues mv = extract_math_operands(a, b, "GREATER_THAN");
                    if (mv.is_float) push(mv.f_left > mv.f_right);
                    else push(mv.i_left > mv.i_right);
                    break;
                }

                case OpCode::OP_LESS_THAN: {
                    Value b = pop(); Value a = pop();
                    MathValues mv = extract_math_operands(a, b, "LESS_THAN");
                    if (mv.is_float) push(mv.f_left < mv.f_right);
                    else push(mv.i_left < mv.i_right);
                    break;
                }

                case OpCode::OP_GREATER_EQUAL: {
                    Value b = pop(); Value a = pop();
                    MathValues mv = extract_math_operands(a, b, "GREATER_EQUAL");
                    if (mv.is_float) push(mv.f_left >= mv.f_right);
                    else push(mv.i_left >= mv.i_right);
                    break;
                }
                case OpCode::OP_LESS_EQUAL: {
                    Value b = pop(); Value a = pop();
                    MathValues mv = extract_math_operands(a, b, "LESS_EQUAL");
                    if (mv.is_float) push(mv.f_left <= mv.f_right);
                    else push(mv.i_left <= mv.i_right);
                    break;
                }

                case OpCode::OP_EQUAL: {
                    Value b = pop(); Value a = pop();
                    
                    // strings
                    if (holds_alternative<string>(a) && holds_alternative<string>(b)) {
                        push(get<string>(a) == get<string>(b));
                    }
                    // chars
                    else if (holds_alternative<char>(a) && holds_alternative<char>(b)) {
                        push(get<char>(a) == get<char>(b));
                    }
                    // bools
                    else if (holds_alternative<bool>(a) && holds_alternative<bool>(b)) {
                        push(get<bool>(a) == get<bool>(b));
                    }
                    // numbers
                    else {
                        MathValues mv = extract_math_operands(a, b, "EQUAL");
                        if (mv.is_float) push(mv.f_left == mv.f_right);
                        else push(mv.i_left == mv.i_right);
                    }
                    break;
                }

                case OpCode::OP_NOT: {
                    Value a = pop();
                    push(!is_truthy(a));
                    break;
                }

                // Variables
                case OpCode::OP_DEFINE_VAR: {
                    uint8_t name_index = bytecode.code[ip++];
                    string var_name = get<string>(bytecode.constants[name_index]);
                    scopes.back()[var_name] = pop();
                    break;
                }
                case OpCode::OP_SAVE: {
                    uint8_t name_index = bytecode.code[ip++];
                    string var_name = get<string>(bytecode.constants[name_index]);
                    Value val = pop();
                    
                    bool found = false;
                    for (int i = scopes.size() - 1; i >= 0; i--) {
                        if (scopes[i].find(var_name) != scopes[i].end()) {
                            scopes[i][var_name] = val;
                            found = true;
                            break;
                        }
                    }
                    
                    if (!found) {
                        scopes.back()[var_name] = val;
                    }
                    break;
                }
                case OpCode::OP_LOAD: {
                    uint8_t name_index = bytecode.code[ip++];
                    string var_name = get<string>(bytecode.constants[name_index]);
                    int i = (int)scopes.size() - 1;
                    for (; i >= 0; i--) {
                        auto it = scopes[i].find(var_name);
                        if (it != scopes[i].end()) {
                            push(it->second);
                            break;
                        }
                    }
                    if (i < 0) {
                        throw runtime_error("Undefined variable '" + var_name + "'");
                    }
                    break;
                }
                case OpCode::OP_CAST: {
                    uint8_t target_type_id = bytecode.code[ip++];
                    TokenType target_type = static_cast<TokenType>(target_type_id);
                    Value val = pop();
                    
                    // To string
                    if (target_type == TokenType::STR) {
                        push(value_to_string(val));
                        break;
                    }

                    // To int
                    if (target_type == TokenType::INT32 || target_type == TokenType::INT64) {
                        long long result = 0;
                        if (holds_alternative<int32_t>(val)) result = get<int32_t>(val);
                        else if (holds_alternative<int64_t>(val)) result = get<int64_t>(val);
                        else if (holds_alternative<float>(val)) result = static_cast<long long>(get<float>(val));
                        else if (holds_alternative<double>(val)) result = static_cast<long long>(get<double>(val));
                        else if (holds_alternative<bool>(val)) result = get<bool>(val) ? 1 : 0;
                        else if (holds_alternative<char>(val)) result = static_cast<long long>(get<char>(val));
                        
                        push(result);
                    }
                    // To float
                    else if (target_type == TokenType::FLOAT32 || target_type == TokenType::FLOAT64) {
                        double result = 0.0;
                        if (holds_alternative<int32_t>(val)) result = static_cast<double>(get<int32_t>(val));
                        else if (holds_alternative<int64_t>(val)) result = static_cast<double>(get<int64_t>(val));
                        else if (holds_alternative<float>(val)) result = static_cast<double>(get<float>(val));
                        else if (holds_alternative<double>(val)) result = get<double>(val);
                        else if (holds_alternative<bool>(val)) result = get<bool>(val) ? 1.0 : 0.0;
                        else if (holds_alternative<char>(val)) result = static_cast<double>(get<char>(val));
                        
                        push(result);
                    }
                    // To bool
                    else if (target_type == TokenType::BOOL) {
                        push(is_truthy(val));
                    }
                    // To char
                    else if (target_type == TokenType::CHAR) {
                        char result = '\0';
                        if (holds_alternative<int32_t>(val)) result = static_cast<char>(get<int32_t>(val));
                        else if (holds_alternative<int64_t>(val)) result = static_cast<char>(get<int64_t>(val));
                        else if (holds_alternative<char>(val)) result = get<char>(val);
                        
                        push(result);
                    }
                    
                    else {
                        throw runtime_error("Unsupported cast target");
                    }
                    break;
                }
                case OpCode::OP_POP: {
                    pop();
                    break;
                }
                case OpCode::OP_ENTER_SCOPE: {
                    scopes.push_back({});
                    break;
                }
                case OpCode::OP_EXIT_SCOPE: {
                    scopes.pop_back();
                    break;
                }

                case OpCode::OP_DUPLICATE: {
                    if (stack.empty()) throw runtime_error("Stack underflow!");
                    push(stack.back());
                    break;
                }

                // Arrays
                case OpCode::OP_NEW_ARRAY: {
                    Value sz_val = pop();
                    int64_t size = holds_alternative<int32_t>(sz_val) ? get<int32_t>(sz_val) : get<int64_t>(sz_val);
                    auto arr = make_shared<ArrayObject>();
                    arr->elements.resize(size);
                    push(arr);
                    break;
                }
                case OpCode::OP_GET_INDEX: {
                    Value idx_val = pop();
                    int64_t index = holds_alternative<int32_t>(idx_val) ? get<int32_t>(idx_val) : get<int64_t>(idx_val);
                    Value arr_val = pop();
                    if (!holds_alternative<shared_ptr<ArrayObject>>(arr_val)) throw runtime_error("Not an array");
                    auto arr = get<shared_ptr<ArrayObject>>(arr_val);
                    if (index < 0 || index >= (int64_t)arr->elements.size()) throw runtime_error("Array index out of bounds");
                    push(arr->elements[index]);
                    break;
                }
                case OpCode::OP_SET_INDEX: {
                    Value val = pop();
                    Value idx_val = pop();
                    int64_t index = holds_alternative<int32_t>(idx_val) ? get<int32_t>(idx_val) : get<int64_t>(idx_val);
                    Value arr_val = pop();
                    if (!holds_alternative<shared_ptr<ArrayObject>>(arr_val)) throw runtime_error("Not an array");
                    auto arr = get<shared_ptr<ArrayObject>>(arr_val);
                    if (index < 0 || index >= (int64_t)arr->elements.size()) throw runtime_error("Array index out of bounds");
                    arr->elements[index] = val;
                    break;
                }
                case OpCode::OP_ARRAY_PUSH: {
                    Value val = pop();
                    Value arr_val = pop();
                    if (!holds_alternative<shared_ptr<ArrayObject>>(arr_val)) throw runtime_error("Not an array");
                    auto arr = get<shared_ptr<ArrayObject>>(arr_val);
                    arr->elements.push_back(val);
                    push(arr_val);
                    break;
                }
                case OpCode::OP_ARRAY_POP: {
                    Value arr_val = pop();
                    if (!holds_alternative<shared_ptr<ArrayObject>>(arr_val)) throw runtime_error("Not an array");
                    auto arr = get<shared_ptr<ArrayObject>>(arr_val);
                    if (arr->elements.empty()) throw runtime_error("Cannot pop from empty array");
                    Value popped = arr->elements.back();
                    arr->elements.pop_back();
                    push(popped);
                    break;
                }

                // Functions
                case OpCode::OP_CALL: {
                    uint8_t name_index = bytecode.code[ip++];
                    uint8_t arg_count = bytecode.code[ip++];
                    string func_name = get<string>(bytecode.constants[name_index]);
                    call_stack.push_back(ip);
                    scope_base_stack.push_back(scopes.size());
                    scopes.push_back({});
                    if (bytecode.function_addresses.find(func_name) == bytecode.function_addresses.end()) throw runtime_error("Undefined function");
                    ip = bytecode.function_addresses.at(func_name);
                    break;
                }
                case OpCode::OP_RETURN: {
                    if (!call_stack.empty()) {
                        ip = call_stack.back();
                        call_stack.pop_back();
                        scopes.pop_back();

                        size_t base = scope_base_stack.back();
                        scope_base_stack.pop_back();
                        while (scopes.size() > base) {
                            scopes.pop_back();
                        }
                    } else return;
                    break;
                }

                // Flow control
                case OpCode::OP_JUMP: {
                    uint8_t high = bytecode.code[ip++];
                    uint8_t low = bytecode.code[ip++];
                    ip = (high << 8) | low;
                    break;
                }
                case OpCode::OP_JUMP_IF_FALSE: {
                    uint8_t high = bytecode.code[ip++];
                    uint8_t low  = bytecode.code[ip++];
                    uint16_t target = (high << 8) | low;
                    if (!is_truthy(pop())) ip = target;
                    break;
                }

                case OpCode::OP_JUMP_IF_TRUE: {
                    uint8_t high = bytecode.code[ip++];
                    uint8_t low  = bytecode.code[ip++];
                    uint16_t target = (high << 8) | low;
                    if (is_truthy(pop())) ip = target;
                    break;
                }

                // System functions
                case OpCode::OP_PRINT: {
                    Value val = pop();
                    std::cout << value_to_string(val) << "\n";
                    break;
                }
                
                // End of code
                case OpCode::OP_HALT: return;
                default: throw runtime_error("Unknown opcode");
            }
        }
    }
};