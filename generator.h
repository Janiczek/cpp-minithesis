#ifndef PBT_GENERATOR_H
#define PBT_GENERATOR_H

#include "gen_result.h"
#include "rand_source.h"
#include "random_run.h"

#include <functional>
#include <random>
#include <string>
#include <utility>
#include <variant>

template<typename T>
class Generator {
public:
    using FunctionType = std::function<GenResult<T>(RandSource const &)>;

    explicit Generator(FunctionType function) : fn(std::move(function)) {}

    GenResult<T> operator()(RandSource source) {
        return fn(source);
    }

    template<typename FN>
    Generator<std::invoke_result_t<FN, T>> map(FN map_fn) const {
        using U = std::invoke_result_t<FN, T>;
        return Generator<U>([this, map_fn](const RandSource &rand) {
            GenResult<T> result = this->fn(rand);
            struct mapper {
                FN map_function;
                explicit mapper(FN map_function) : map_function(map_function) {}
                GenResult<U> operator()(Generated<T> g) {
                    auto val2 = map_function(g.value);
                    return generated(g.run, val2);
                }
                GenResult<U> operator()(const Rejected &r) {
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

    template<typename T>
    Generator<T> constant(T const &val) {
        return Generator<T>([val](RandSource const &) {
            return generated(RandomRun(), val);
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
    template<typename T>
    Generator<T> reject(std::string reason) {
        return Generator<T>([reason](RandSource const &) {
            return rejected<T>(reason);
        });
    }

}// namespace Gen

#endif//PBT_GENERATOR_H
