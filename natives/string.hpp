#pragma once

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

#include "../headers/bytecode.hpp"
#include "../headers/token.hpp"
#include "./shared.hpp"

inline void register_string_natives(Bytecode& bc) {
    // str_len(s)
    bc.register_native("str_len", [](std::vector<Value> args) -> Value {
        const auto& s = detail::get_str_arg(args, 0, "str_len");
        return static_cast<int64_t>(s.size());
    }, 1, 1, Type::INT64);

    // str_trim(s)
    bc.register_native("str_trim", [](std::vector<Value> args) -> Value {
        std::string s = detail::get_str_arg(args, 0, "str_trim");
        auto not_space = [](unsigned char ch) { return !std::isspace(ch); };
        s.erase(s.begin(), std::find_if(s.begin(), s.end(), not_space));
        s.erase(std::find_if(s.rbegin(), s.rend(), not_space).base(), s.end());
        return s;
    }, 1, 1, Type::STR);

    // str_is_int(s)
    bc.register_native("str_is_int", [](std::vector<Value> args) -> Value {
        const auto& s = detail::get_str_arg(args, 0, "str_is_int");
        if (s.empty()) return false;
        try {
            size_t pos = 0;
            std::stoll(s, &pos);
            return pos == s.size();
        } catch (...) {
            return false;
        }
    }, 1, 1, Type::BOOL);

    // str_to_int(s)
    bc.register_native("str_to_int", [](std::vector<Value> args) -> Value {
        const auto& s = detail::get_str_arg(args, 0, "str_to_int");
        try {
            size_t pos = 0;
            int64_t val = std::stoll(s, &pos);
            if (pos != s.size()) throw std::runtime_error("str_to_int: invalid integer format");
            return val;
        } catch (const std::exception&) {
            throw std::runtime_error("str_to_int: failed to parse '" + s + "'");
        }
    }, 1, 1, Type::INT64);

    // str_is_float(s)
    bc.register_native("str_is_float", [](std::vector<Value> args) -> Value {
        const auto& s = detail::get_str_arg(args, 0, "str_is_float");
        if (s.empty()) return false;
        try {
            size_t pos = 0;
            std::stod(s, &pos);
            return pos == s.size();
        } catch (...) {
            return false;
        }
    }, 1, 1, Type::BOOL);

    // str_to_float(s)
    bc.register_native("str_to_float", [](std::vector<Value> args) -> Value {
        const auto& s = detail::get_str_arg(args, 0, "str_to_float");
        try {
            size_t pos = 0;
            double val = std::stod(s, &pos);
            if (pos != s.size()) throw std::runtime_error("str_to_float: invalid float format");
            return val;
        } catch (const std::exception&) {
            throw std::runtime_error("str_to_float: failed to parse '" + s + "'");
        }
    }, 1, 1, Type::FLOAT64);

    // str_upper(s)
    bc.register_native("str_upper", [](std::vector<Value> args) -> Value {
        std::string s = detail::get_str_arg(args, 0, "str_upper");
        for (char& c : s) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
        return s;
    }, 1, 1, Type::STR);

    // str_lower(s)
    bc.register_native("str_lower", [](std::vector<Value> args) -> Value {
        std::string s = detail::get_str_arg(args, 0, "str_lower");
        for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        return s;
    }, 1, 1, Type::STR);

    // str_contains(s, sub)
    bc.register_native("str_contains", [](std::vector<Value> args) -> Value {
        const auto& s = detail::get_str_arg(args, detail::arg_idx(args, 0), "str_contains");
        const auto& sub = detail::get_str_arg(args, detail::arg_idx(args, 1), "str_contains");
        return s.find(sub) != std::string::npos;
    }, 2, 2, Type::BOOL);

    // str_starts_with(s, prefix)
    bc.register_native("str_starts_with", [](std::vector<Value> args) -> Value {
        const auto& s = detail::get_str_arg(args, detail::arg_idx(args, 0), "str_starts_with");
        const auto& prefix = detail::get_str_arg(args, detail::arg_idx(args, 1), "str_starts_with");
        return s.rfind(prefix, 0) == 0;
    }, 2, 2, Type::BOOL);

    // str_ends_with(s, suffix)
    bc.register_native("str_ends_with", [](std::vector<Value> args) -> Value {
        const auto& s = detail::get_str_arg(args, detail::arg_idx(args, 0), "str_ends_with");
        const auto& suffix = detail::get_str_arg(args, detail::arg_idx(args, 1), "str_ends_with");
        if (suffix.size() > s.size()) return false;
        return s.compare(s.size() - suffix.size(), suffix.size(), suffix) == 0;
    }, 2, 2, Type::BOOL);

    // str_at(s, index)
    bc.register_native("str_at", [](std::vector<Value> args) -> Value {
        const auto& s = detail::get_str_arg(args, detail::arg_idx(args, 0), "str_at");
        int64_t idx = detail::get_int_arg<int64_t>(args, detail::arg_idx(args, 1), "str_at");
        if (idx < 0 || static_cast<size_t>(idx) >= s.size()) {
            throw std::runtime_error("str_at: index out of bounds");
        }
        return s[static_cast<size_t>(idx)];
    }, 2, 2, Type::CHAR);

    // str_substr(s, start) or str_substr(s, start, length)
    bc.register_native("str_substr", [](std::vector<Value> args) -> Value {
        const auto& s = detail::get_str_arg(args, detail::arg_idx(args, 0), "str_substr");
        int64_t start = detail::get_int_arg<int64_t>(args, detail::arg_idx(args, 1), "str_substr");
        if (start < 0 || static_cast<size_t>(start) > s.size()) {
            throw std::runtime_error("str_substr: start index out of bounds");
        }
        if (args.size() == 2) {
            return s.substr(static_cast<size_t>(start));
        }
        int64_t len = detail::get_int_arg<int64_t>(args, detail::arg_idx(args, 2), "str_substr");
        if (len < 0) throw std::runtime_error("str_substr: length must be >= 0");
        return s.substr(static_cast<size_t>(start), static_cast<size_t>(len));
    }, 2, 3, Type::STR);

    // str_replace(s, from, to)
    bc.register_native("str_replace", [](std::vector<Value> args) -> Value {
        std::string s = detail::get_str_arg(args, detail::arg_idx(args, 0), "str_replace");
        const auto& from = detail::get_str_arg(args, detail::arg_idx(args, 1), "str_replace");
        const auto& to = detail::get_str_arg(args, detail::arg_idx(args, 2), "str_replace");
        if (from.empty()) return s;
        size_t pos = 0;
        while ((pos = s.find(from, pos)) != std::string::npos) {
            s.replace(pos, from.length(), to);
            pos += to.length();
        }
        return s;
    }, 3, 3, Type::STR);
}
