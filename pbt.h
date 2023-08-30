#ifndef PBT_PBT_H
#define PBT_PBT_H

#include "gen_result.h"
#include "generator.h"
#include "rand_source.h"
#include "random_run.h"
#include "shrink.h"
#include "test_exception.h"
#include "test_result.h"

#include <iostream>
#include <map>
#include <random>
#include <string>

#define MAX_GENERATED_VALUES_PER_TEST 100
#define MAX_GEN_ATTEMPTS_PER_VALUE 15

template<typename T, typename FN>
TestResult<T> run(Generator<T> generator, FN test_function) {

    std::random_device r;
    std::mt19937 rng(r());

    for (int i = 0; i < MAX_GENERATED_VALUES_PER_TEST; i++) {
        std::map<std::string, int> rejections;
        bool generated_successfully = false;
        for (int gen_attempt = 0; gen_attempt < MAX_GEN_ATTEMPTS_PER_VALUE && !generated_successfully; gen_attempt++) {
            Live live_source{RandomRun(), rng};
            GenResult<T> gen_result = generator(live_source);
            if (auto generated = std::get_if<Generated<T>>(&gen_result)) {
                generated_successfully = true;
                try {
                    test_function(generated->value);
                } catch (TestException &e) {
                    return shrink(*generated, generator, test_function, e.what());
                }
            } else if (auto rejected = std::get_if<Rejected>(&gen_result)) {
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

template<typename T, typename FN>
void run_test(const std::string &name, Generator<T> gen, FN test_function) {
    std::cout << "--------" << std::endl;
    auto result = run(gen, test_function);
    std::cout << "[" << name << "] " << to_string(result) << std::endl;
}

#endif//PBT_PBT_H
