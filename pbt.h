#ifndef PBT_PBT_H
#define PBT_PBT_H

#include <functional>
#include <iostream>
#include <random>
#include <map>
#include <string>
#include <variant>
#include <vector>

#define MAX_RANDOMRUN_LENGTH (64 * 1024 * 8) // 64 kB
#define MAX_GENERATED_VALUES_PER_TEST 100
#define MAX_GEN_ATTEMPTS_PER_VALUE 15
// #define RAND_TYPE unsigned int

class RandomRun {
public:
    RandomRun() {
        run = std::vector<bool>();
        run.reserve(MAX_RANDOMRUN_LENGTH);
    }
    // TODO add API to actually use RandomRun
private:
    std::vector<bool> run;
};

// RandSource

struct Live {
    RandomRun run; // in the process of being created
    std::default_random_engine rng;
};
struct Recorded {
    RandomRun recorded_run; // in the process of being consumed
    size_t unused_i; // TODO is this the right type for a pointer into the vector?
    // TODO do we need the full run+index? Can't we just remove items from it?
};
using RandSource = std::variant<Live, Recorded>;

RandomRun random_run(RandSource rand) {
    struct getter
    {
        RandomRun operator()(const Live& l)     { return l.run; }
        RandomRun operator()(const Recorded& r) { return r.recorded_run; }
    };
    return std::visit(getter{}, rand);
}

// GenResult

template <typename T> struct Generated {
    RandomRun recorded_run; // run corresponding to the value
    T generated_value;
};
struct Rejected {
    std::string reason;
};
template <typename T> using GenResult = std::variant<Generated<T>, Rejected>;


// Generator

template <typename T> class Generator {
public:
    using FunctionType = std::function<GenResult<T>(RandSource const&)>;

    explicit Generator(FunctionType function) : fn(std::move(function)) {}

    GenResult<T> run(RandSource const& source) const {
        return fn(source);
    }

private:
    FunctionType fn;
};

// Constant Generator

template <typename T> Generator<T> constant(T const& val) {
    return Generator<T>([val](RandSource const&) {
        return GenResult<T>{Generated<T>{RandomRun(), val}};
    });
}

// TestResult

struct Passes {};
template <typename T> struct FailsWith { T value; };
struct CannotGenerateValues { std::map<std::string,int> rejections; };
template <typename T> using TestResult = std::variant<Passes,FailsWith<T>,CannotGenerateValues>;

template <typename T>
std::string to_string(const TestResult<T>& result) {
    struct stringifier {
        std::string operator()(Passes _)                      { return "Passes"; }
        std::string operator()(FailsWith<T> _)                { return "Fails"; }
        std::string operator()(const CannotGenerateValues& _) { return "Cannot generate values"; }
    };
    return std::visit(stringifier{}, result);
}

// Runner

template <typename T, typename FN>
TestResult<T> run(Generator<T> generator, FN testFunction) {

    RandomRun emptyRun;
    std::random_device r;
    std::default_random_engine rng(r());
    Live liveSource{emptyRun, rng};

    for (int i = 0; i < MAX_GENERATED_VALUES_PER_TEST; i++) {
        std::map<std::string,int> rejections;
        bool generated_successfully = false;
        for (int gen_attempt = 0; gen_attempt < MAX_GEN_ATTEMPTS_PER_VALUE && !generated_successfully; gen_attempt++) {
            GenResult<T> genResult = generator.run(liveSource);
            if (auto generated = std::get_if<Generated<T>>(&genResult)) {
                generated_successfully = true;
                try {
                    testFunction(generated->generated_value);
                } catch (...) {
                    return FailsWith<T>{generated->generated_value};
                }
            } else if (auto rejected = std::get_if<Rejected>(&genResult)) {
                rejections[rejected->reason]++;
            }
        }
        if (!generated_successfully) { // meaning the loop above got to the full MAX_GEN_ATTEMPTS_PER_VALUE and gave up
            return CannotGenerateValues{rejections};
        }
    }
    // MAX_GENERATED_VALUES_PER_TEST values generated, all passed the test.
    return Passes();
}

#endif//PBT_PBT_H
