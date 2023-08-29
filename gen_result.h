#ifndef PBT_GEN_RESULT_H
#define PBT_GEN_RESULT_H

#include "random_run.h"

#include <string>
#include <utility>
#include <variant>

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

#endif//PBT_GEN_RESULT_H
