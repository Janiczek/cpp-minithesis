#ifndef PBT_PBT_H
#define PBT_PBT_H

#include <functional>
#include <iostream>
#include <map>
#include <random>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#define MAX_RANDOMRUN_LENGTH (64 * 1024) // 64 k items
#define MAX_GENERATED_VALUES_PER_TEST 100
#define MAX_GEN_ATTEMPTS_PER_VALUE 15
#define RAND_TYPE unsigned int

// TestException

class TestException : public std::exception {
private:
    std::string message;

public:
    explicit TestException(std::string msg) : message(std::move(msg)) {}
    std::string what() { return message; }
};

// RandomRun

class RandomRun {
public:
    RandomRun() {
        run = std::vector<RAND_TYPE>();
        run.reserve(MAX_RANDOMRUN_LENGTH);
    }
    [[nodiscard]] bool is_empty() const { return run.empty(); }
    [[nodiscard]] bool is_full() const { return run.size() >= MAX_RANDOMRUN_LENGTH; }
    void push_back(RAND_TYPE n) { run.push_back(n); }
    RAND_TYPE next() { return *it++; }
private:
    std::vector<RAND_TYPE> run;
    std::vector<RAND_TYPE>::const_iterator it = run.cbegin();
};

// RandSource

struct Live {
    RandomRun run; // in the process of being created
    std::mt19937 &rng;
};
struct Recorded {
    RandomRun run; // in the process of being consumed
};
using RandSource = std::variant<Live, Recorded>;

RandomRun random_run(RandSource rand) {
    struct getter
    {
        RandomRun operator()(const Live& l)     { return l.run; }
        RandomRun operator()(const Recorded& r) { return r.run; }
    };
    return std::visit(getter{}, rand);
}

// GenResult

template <typename T> struct Generated {
    RandomRun run; // run corresponding to the value
    T value;
};
struct Rejected {
    std::string reason;
};
template <typename T> using GenResult = std::variant<Generated<T>, Rejected>;
template <typename T> GenResult<T> generated(RandomRun run, T val) {
    return GenResult<T>{Generated<T>{run, val}};
}
template <typename T> GenResult<T> generated(T val) {
    return generated(RandomRun(), val);
}
template <typename T> GenResult<T> rejected(std::string reason) {
    return GenResult<T>{Rejected{std::move(reason)}};
}

// Generator

template <typename T> class Generator {
public:
    using FunctionType = std::function<GenResult<T>(RandSource const&)>;

    explicit Generator(FunctionType function) : fn(std::move(function)) {}

    [[nodiscard]] GenResult<T> run(RandSource source) const {
        return fn(source);
    }

private:
    FunctionType fn;
};

// Various generators

namespace Gen {

    template <typename T> Generator<T> constant(T const& val) {
        return Generator<T>([val](RandSource const&) {
            return generated(val);
        });
    }
    Generator<unsigned int> unsigned_int(unsigned int max) {
        return Generator<unsigned int>([max](RandSource const& rand) {
          if (random_run(rand).is_full()) {
              return rejected<unsigned int>("Generators have hit maximum RandomRun length (generating too much data).");
          }
          struct handler {
              unsigned int max_value;
              explicit handler(unsigned int max) : max_value(max) {}

              GenResult<unsigned int> operator()(Live l) const {
                  std::uniform_int_distribution<unsigned int> dist(0,max_value);
                  auto val = dist(l.rng);
                  l.run.push_back(val);
                  return generated(l.run,val);
              }
              GenResult<unsigned int> operator()(Recorded r) {
                  if (r.run.is_empty()) {
                      return rejected<unsigned int>("Ran out of recorded bits");
                  }
                  auto val = r.run.next();
                  return generated(r.run, val);
              }
          };
          return std::visit(handler{max}, rand);
        });
    }

}


// TestResult

struct Passes {};
template <typename T> struct FailsWith {
    T value;
    std::string error;
};
struct CannotGenerateValues { std::map<std::string,int> rejections; };
template <typename T> using TestResult = std::variant<Passes,FailsWith<T>,CannotGenerateValues>;

template <typename T>
std::string to_string(const TestResult<T>& result) {
    struct stringifier {
        std::string operator()(Passes _)                        { return "Passes"; }
        std::string operator()(FailsWith<T> f)                  { return "Fails: \"" + f.error + "\""; } // TODO show the value
        std::string operator()(const CannotGenerateValues& cgv) { return "Cannot generate values. Most common reason: "
                                                                         + std::max_element(cgv.rejections.begin(),
                                                                                            cgv.rejections.end()
                                                                                            )->first; } // TODO show more rejections
    };
    return std::visit(stringifier{}, result);
}

// Runner

template <typename T, typename FN>
TestResult<T> run(Generator<T> generator, FN testFunction) {

    RandomRun emptyRun;
    std::random_device r;
    std::mt19937 rng(r());
    Live liveSource{emptyRun, rng};

    for (int i = 0; i < MAX_GENERATED_VALUES_PER_TEST; i++) {
        std::map<std::string,int> rejections;
        bool generated_successfully = false;
        for (int gen_attempt = 0; gen_attempt < MAX_GEN_ATTEMPTS_PER_VALUE && !generated_successfully; gen_attempt++) {
            GenResult<T> genResult = generator.run(liveSource);
            if (auto generated = std::get_if<Generated<T>>(&genResult)) {
                generated_successfully = true;
                try {
                    testFunction(generated->value);
                } catch (TestException &e) {
                    return FailsWith<T>{generated->value, e.what()};
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
