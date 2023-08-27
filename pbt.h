#ifndef PBT_PBT_H
#define PBT_PBT_H

#include <iostream>
#include <map>
#include <random>

#define MAX_RANDOMRUN_LENGTH (64 * 1024)// 64k items
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
    friend std::ostream& operator<< (std::ostream& os, const RandomRun& random_run) {
        auto size = random_run.run.size();
        os << "[";
        for (size_t i = 0; i < size; i++) {
            os << random_run.run[i];
            if (i < size - 1) { os << ","; }
        }
        os << "]";
        return os;
    }

private:
    std::vector<RAND_TYPE> run;
    std::vector<RAND_TYPE>::const_iterator it = run.cbegin();
};

// RandSource

struct Live {
    RandomRun run;// in the process of being created
    std::mt19937 &rng;
};
struct Recorded {
    RandomRun run;// in the process of being consumed
};
using RandSource = std::variant<Live, Recorded>;

RandomRun random_run(RandSource rand) {
    struct getter {
        RandomRun operator()(const Live &l) { return l.run; }
        RandomRun operator()(const Recorded &r) { return r.run; }
    };
    return std::visit(getter{}, rand);
}

// GenResult

template<typename T>
struct Generated {
    RandomRun run;// run corresponding to the value
    T value;
};
struct Rejected {
    std::string reason;
};
template<typename T>
using GenResult = std::variant<Generated<T>, Rejected>;
template<typename T>
GenResult<T> generated(RandomRun run, T val) {
    return GenResult<T>{Generated<T>{run, val}};
}
template<typename T>
GenResult<T> rejected(std::string reason) {
    return GenResult<T>{Rejected{std::move(reason)}};
}

// Generator

template<typename T>
class Generator {
public:
    using FunctionType = std::function<GenResult<T>(RandSource const &)>;

    explicit Generator(FunctionType function) : fn(std::move(function)) {}

    GenResult<T> operator()(RandSource source) {
        return fn(source);
    }

    template <typename FN>
    Generator<std::invoke_result_t<FN,T>> map(FN map_fn) const {
        using U = std::invoke_result_t<FN,T>;
        return Generator<U>([this,map_fn](const RandSource& rand) {
            GenResult<T> result = this->fn(rand);
            struct mapper {
                FN map_function;
                explicit mapper(FN map_function) : map_function(map_function) {}
                GenResult<U> operator()(Generated<T> g) {
                    auto val2 = map_function(g.value);
                    return generated(g.run, val2);
                }
                GenResult<U> operator()(const Rejected& r) {
                    return r;
                }
            };
            return std::visit(mapper{map_fn}, result);
        });
    }

private:
    FunctionType fn;
};

// Various generators

namespace Gen {

    template<typename T> Generator<T> constant(T const &val) {
        return Generator<T>([val](RandSource const &) {
            return generated(RandomRun(),val);
        });
    }
    Generator<unsigned int> unsigned_int(unsigned int max) {
        return Generator<unsigned int>([max](RandSource const &rand) {
            if (random_run(rand).is_full()) {
                return rejected<unsigned int>("Generators have hit maximum RandomRun length (generating too much data).");
            }
            struct handler {
                unsigned int max_value;
                explicit handler(unsigned int max) : max_value(max) {}

                GenResult<unsigned int> operator()(Live l) const {
                    std::uniform_int_distribution<unsigned int> dist(0, max_value);
                    auto val = dist(l.rng);
                    l.run.push_back(val);
                    return generated(l.run, val);
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
    template<typename T> Generator<T> reject(std::string reason) {
        return Generator<T>([reason](RandSource const &) {
          return rejected<T>(reason);
        });
    }

}// namespace Gen


// TestResult

struct Passes {};
template<typename T>
struct FailsWith {
    T value;
    std::string error;
};
struct CannotGenerateValues {
    std::map<std::string, int> rejections;
};
template<typename T>
using TestResult = std::variant<Passes, FailsWith<T>, CannotGenerateValues>;

template<typename T>
std::string to_string(const TestResult<T> &result) {
    struct stringifier {
        std::string operator()(Passes) { return "Passes"; }
        std::string operator()(FailsWith<T> f) { return "Fails:\n - value: " + std::to_string(f.value) + "\n - error: \"" + f.error + "\""; }
        std::string operator()(const CannotGenerateValues &cgv) {

            // Partially sort the map (well, a vector of pairs) to get the top 5 rejections
            auto descByValue = [](const auto& a, const auto& b) { return a.second > b.second; };
            auto size = std::min(5, static_cast<int>(cgv.rejections.size()));
            std::vector<std::pair<std::string, int>> sorted_items(cgv.rejections.begin(), cgv.rejections.end());
            std::partial_sort(sorted_items.begin(), sorted_items.begin() + size, sorted_items.end(), descByValue);

            std::string reasons;
            for (int i = 0; i < size; i++) {
                reasons += "\n - " + sorted_items[i].first;
            }

            return "Cannot generate values. Most common reasons:" + reasons;
        }
    };
    return std::visit(stringifier{}, result);
}

// Shrinker

template<typename T, typename FN>
FailsWith<T> shrink(Generated<T> generated, Generator<T> generator, FN testFunction, std::string failMessage) {
    std::cout << "----------------" << std::endl;
    std::cout << "Let's shrink: " << generated.value << std::endl;
    std::cout << "Random run: " << generated.run << std::endl;
    // TODO actually shrink
    return FailsWith<T>{generated.value, failMessage};
}

// Runner

template<typename T, typename FN>
TestResult<T> run(Generator<T> generator, FN testFunction) {

    RandomRun emptyRun;
    std::random_device r;
    std::mt19937 rng(r());
    Live liveSource{emptyRun, rng};

    for (int i = 0; i < MAX_GENERATED_VALUES_PER_TEST; i++) {
        std::map<std::string, int> rejections;
        bool generated_successfully = false;
        for (int gen_attempt = 0; gen_attempt < MAX_GEN_ATTEMPTS_PER_VALUE && !generated_successfully; gen_attempt++) {
            GenResult<T> genResult = generator(liveSource);
            if (auto generated = std::get_if<Generated<T>>(&genResult)) {
                generated_successfully = true;
                try {
                    testFunction(generated->value);
                } catch (TestException &e) {
                    return shrink(*generated, generator, testFunction, e.what());
                }
            } else if (auto rejected = std::get_if<Rejected>(&genResult)) {
                rejections[rejected->reason]++;
            }
        }
        if (!generated_successfully) {// meaning the loop above got to the full MAX_GEN_ATTEMPTS_PER_VALUE and gave up
            return CannotGenerateValues{rejections};
        }
    }
    // MAX_GENERATED_VALUES_PER_TEST values generated, all passed the test.
    return Passes();
}

template <typename T, typename FN>
void run_test(const std::string &name, Generator<T> gen, FN testFunction) {
    auto result = run(gen, testFunction);
    std::cout << "[" << name << "] " << to_string(result) << std::endl;
}

#endif//PBT_PBT_H
