#pragma once

#include <vector>
#include <variant>
#include "../headers/bytecode.hpp"
#include "../headers/token.hpp"

namespace detail {
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
}