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

    /* Runs the provided function on each value of the generator.

       Gen::constant(100)                                 --> 100
       Gen::constant(100).map([](int i){ return i + 2; }) --> 102

       This doesn't incur any extra RandomRun footprint.

       Shrunk values will still honor this mapping:

       Gen::unsigned_int(10).map([](auto i){ return i * 100; }) --> 0, 100, 200, ..., 1000
                                                                    even after shrinking
     */
    template<typename FN>
    Generator<std::invoke_result_t<FN, T>> map(FN map_fn) const {
        using U = std::invoke_result_t<FN, T>;
        auto fn = this->fn;
        return Generator<U>([fn, map_fn](RandSource rand) {
            GenResult<T> result = fn(rand);
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
    
    /* Filters all generated values by the provided predicate.
       (Keeps all values x where predicate(x) == true.)

       Gen::unsigned_int(10)                                         --> 0, 1, 2, 3, ..., 10
       Gen::unsigned_int(10).filter([](int i){ return i % 2 == 1; }) --> 1, 3, 5, ..., 9

       This doesn't incur any extra RandomRun footprint.

       Shrunk values will still honor this filtering.
     */
    template<typename FN>
    Generator<T> filter(FN predicate) const {
        auto fn = this->fn;
        return Generator<T>([fn, predicate](RandSource rand) {
            GenResult<T> result = fn(rand);
            
            struct filter_mapper {
                FN predicate_function;
                explicit filter_mapper(FN predicate_function) : predicate_function(predicate_function) {}
                GenResult<T> operator()(Generated<T> g) {
                    if (predicate_function(g.value)) {
                        return g;
                    } else {
                        return rejected<T>("Value filtered out");
                    }
                }
                GenResult<T> operator()(const Rejected &r) {
                    return r;
                }
            };
            
            return std::visit(filter_mapper{predicate}, result);
        });
    }

private:
    FunctionType fn;
};

namespace Gen {

    /* This generator always succeeds to generate the same value.
       FP folks will know this as `pure`, `return` or `succeed`.
     
       Gen::constant(x) -> value x, RandomRun [] (always)
     
       Shrinkers have no effect on the value.
     */
    template<typename T>
    Generator<T> constant(T const &val) {
        return Generator<T>([val](RandSource const &) {
            return generated(RandomRun(), val);
        });
    }

    /* This generator always fails to generate a value.
     
       The given reason will be noted by the test runner and reported at the end
       if the runner fails to generate any value.
     
       Gen::reject("Bad hair day") -> no value, RandomRun [] (always)
     
       Shrinkers have no effect (duh).
     */
    template<typename T>
    Generator<T> reject(std::string reason) {
        return Generator<T>([reason](RandSource const &) {
            return rejected<T>(reason);
        });
    }

    /* This is a foundational generator: it's the only one low-level enough to
       handle the adding to / reading of values from the RandSource.

       Other generators will be largely built from this one via combinators.

       The minimum value will always be 0.
       The maximum value is given by user in the argument.

       Gen::unsigned_int(10) -> value 5, RandomRun [5]
                             -> value 8, RandomRun [8]
                             etc.

       Shrinks towards 0.
     */
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

    /* An unsigned integer generator in a particular range.
       
       The minimum value is the smaller of the two arguments.
       The maximum value is the larger of the two arguments.

       In the general case this is the behaviour:
     
       Gen::unsigned_int(3,10) -> value 3,  RandomRun [3]
                               -> value 8,  RandomRun [8]
                               -> value 10, RandomRun [10]
                               etc.
     
       In case `min == max`, the RandomRun footprint will be smaller, as we'll
       switch to a `constant` and won't need any randomness to generate that
       value:

       Gen::unsigned_int(3,3) -> value 3, RandomRun [] (always)
     
       Shrinks towards the smaller of the argument.
     */
    Generator<unsigned int> unsigned_int(unsigned int min, unsigned int max) {
        if (min > max)  { return unsigned_int(max, min); }
        if (min == max) { return constant(min); }
        unsigned int range = max - min;
        return Gen::unsigned_int(range).map([min](unsigned int x){ return x + min; });
    }

}// namespace Gen

#endif//PBT_GENERATOR_H
