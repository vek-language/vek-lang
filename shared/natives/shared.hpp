#pragma once

#include <cstdint>
#include <memory>
#include <stdexcept>
#include <string>
#include <variant>
#include <vector>

#include "bytecode.hpp"
#include "token.hpp"

namespace detail {
    // convert argument position to VM stack index
    inline size_t arg_idx(const std::vector<Value>& args, size_t pos) {
        if (pos >= args.size()) {
            throw std::runtime_error("Native function: missing argument at index " + std::to_string(pos));
        }
        return args.size() - 1 - pos;
    }

    template <typename T>
    T get_int_arg(const std::vector<Value>& args, size_t idx, const char* func_name) {
        if (const auto* v32 = std::get_if<int32_t>(&args[idx])) return static_cast<T>(*v32);
        if (const auto* v64 = std::get_if<int64_t>(&args[idx])) return static_cast<T>(*v64);
        throw std::runtime_error(std::string(func_name) + ": expected integer argument");
    }

    template <typename T>
    T get_float_arg(const std::vector<Value>& args, size_t idx, const char* func_name) {
        if (const auto* f32 = std::get_if<float>(&args[idx])) return static_cast<T>(*f32);
        if (const auto* f64 = std::get_if<double>(&args[idx])) return static_cast<T>(*f64);
        if (const auto* v32 = std::get_if<int32_t>(&args[idx])) return static_cast<T>(*v32);
        if (const auto* v64 = std::get_if<int64_t>(&args[idx])) return static_cast<T>(*v64);
        throw std::runtime_error(std::string(func_name) + ": expected float argument");
    }

    inline const std::string& get_str_arg(const std::vector<Value>& args, size_t idx, const char* func_name) {
        if (const auto* s = std::get_if<std::string>(&args[idx])) {
            return *s;
        }
        throw std::runtime_error(std::string(func_name) + ": expected str argument");
    }

    inline bool get_bool_arg(const std::vector<Value>& args, size_t idx, const char* func_name) {
        if (const auto* b = std::get_if<bool>(&args[idx])) {
            return *b;
        }
        throw std::runtime_error(std::string(func_name) + ": expected bool argument");
    }

    inline char get_char_arg(const std::vector<Value>& args, size_t idx, const char* func_name) {
        if (const auto* c = std::get_if<char>(&args[idx])) {
            return *c;
        }
        throw std::runtime_error(std::string(func_name) + ": expected char argument");
    }

    inline std::shared_ptr<ArrayObject> get_array_arg(const std::vector<Value>& args, size_t idx, const char* func_name) {
        if (const auto* arr = std::get_if<std::shared_ptr<ArrayObject>>(&args[idx])) {
            return *arr;
        }
        throw std::runtime_error(std::string(func_name) + ": expected array argument");
    }
}
