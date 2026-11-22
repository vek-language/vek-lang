#pragma once

#include <cstdint>
#include <limits>
#include <random>
#include <stdexcept>
#include <vector>

#include "./shared.hpp"
#include "../headers/bytecode.hpp"
#include "../headers/token.hpp"

namespace detail {
    inline std::mt19937_64& rng() {
        static thread_local std::mt19937_64 gen(std::random_device{}());
        return gen;
    }
} 

inline void register_random_natives(Bytecode& bc) {
    // int32: rand_i32(), rand_i32(max), rand_i32(min, max)
    bc.register_native("rand_i32", [](std::vector<Value> args) -> Value {
        int32_t min_v = 0;
        int32_t max_v = std::numeric_limits<int32_t>::max();

        if (args.size() == 1) {
            max_v = detail::get_int_arg<int32_t>(args, 0, "rand_i32") - 1;
            if (max_v < 0) throw std::runtime_error("rand_i32(max) requires max > 0");
        } else if (args.size() == 2) {
            min_v = detail::get_int_arg<int32_t>(args, 1, "rand_i32");
            max_v = detail::get_int_arg<int32_t>(args, 0, "rand_i32");
            if (min_v > max_v) throw std::runtime_error("rand_i32(min, max) requires min <= max");
        }

        std::uniform_int_distribution<int32_t> dist(min_v, max_v);
        return dist(detail::rng());
    }, 0, 2, Type::INT32);

    // int64: rand_i64(), rand_i64(max), rand_i64(min, max)
    bc.register_native("rand_i64", [](std::vector<Value> args) -> Value {
        int64_t min_v = 0;
        int64_t max_v = std::numeric_limits<int64_t>::max();

        if (args.size() == 1) {
            max_v = detail::get_int_arg<int64_t>(args, 0, "rand_i64") - 1;
            if (max_v < 0) throw std::runtime_error("rand_i64(max) requires max > 0");
        } else if (args.size() == 2) {
            min_v = detail::get_int_arg<int64_t>(args, 1, "rand_i64");
            max_v = detail::get_int_arg<int64_t>(args, 0, "rand_i64");
            if (min_v > max_v) throw std::runtime_error("rand_i64(min, max) requires min <= max");
        }

        std::uniform_int_distribution<int64_t> dist(min_v, max_v);
        return dist(detail::rng());
    }, 0, 2, Type::INT64);

    // float32: rand_f32(), rand_f32(max), rand_f32(min, max)
    bc.register_native("rand_f32", [](std::vector<Value> args) -> Value {
        float min_v = 0.0f;
        float max_v = 1.0f;

        if (args.size() == 1) {
            max_v = detail::get_float_arg<float>(args, 0, "rand_f32");
            if (max_v <= 0.0f) throw std::runtime_error("rand_f32(max) requires max > 0");
        } else if (args.size() == 2) {
            min_v = detail::get_float_arg<float>(args, 1, "rand_f32");
            max_v = detail::get_float_arg<float>(args, 0, "rand_f32");
            if (min_v > max_v) throw std::runtime_error("rand_f32(min, max) requires min <= max");
        }

        std::uniform_real_distribution<float> dist(min_v, max_v);
        return dist(detail::rng());
    }, 0, 2, Type::FLOAT32);

    // float64: rand_f64(), rand_f64(max), rand_f64(min, max)
    bc.register_native("rand_f64", [](std::vector<Value> args) -> Value {
        double min_v = 0.0;
        double max_v = 1.0;

        if (args.size() == 1) {
            max_v = detail::get_float_arg<double>(args, 0, "rand_f64");
            if (max_v <= 0.0) throw std::runtime_error("rand_f64(max) requires max > 0");
        } else if (args.size() == 2) {
            min_v = detail::get_float_arg<double>(args, 1, "rand_f64");
            max_v = detail::get_float_arg<double>(args, 0, "rand_f64");
            if (min_v > max_v) throw std::runtime_error("rand_f64(min, max) requires min <= max");
        }

        std::uniform_real_distribution<double> dist(min_v, max_v);
        return dist(detail::rng());
    }, 0, 2, Type::FLOAT64);

    // 50/50, rand_bool(probability) -> chance in [0.0, 1.0]
    bc.register_native("rand_bool", [](std::vector<Value> args) -> Value {
        double prob = 0.5;
        if (args.size() == 1) {
            prob = detail::get_float_arg<double>(args, 0, "rand_bool");
            if (prob < 0.0 || prob > 1.0) {
                throw std::runtime_error("rand_bool(prob) requires prob between 0.0 and 1.0");
            }
        }

        std::bernoulli_distribution dist(prob);
        return dist(detail::rng());
    }, 0, 1, Type::BOOL);

    // Seed
    bc.register_native("rand_seed", [](std::vector<Value> args) -> Value {
        if (args.empty()) {
            detail::rng().seed(std::random_device{}());
        } else {
            int64_t s = detail::get_int_arg<int64_t>(args, 0, "rand_seed");
            detail::rng().seed(static_cast<uint64_t>(s));
        }
        return nil_value();
    }, 0, 1, Type::VOID);

}
