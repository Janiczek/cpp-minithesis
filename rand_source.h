#ifndef PBT_RAND_SOURCE_H
#define PBT_RAND_SOURCE_H

#include "random_run.h"

#include <random>
#include <variant>

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

#endif//PBT_RAND_SOURCE_H
