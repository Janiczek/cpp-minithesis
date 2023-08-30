#ifndef PBT_SHRINK_H
#define PBT_SHRINK_H

#include "gen_result.h"
#include "generator.h"
#include "rand_source.h"
#include "random_run.h"
#include "test_exception.h"
#include "test_result.h"
#include "shrink_cmd.h"

#include <iostream>
#include <string>
#include <variant>
#include <vector>

template<typename T>
struct ShrinkState {
    RandomRun run;
    T value;
    std::string fail_message;
};

template<typename T>
struct ShrinkResult {
    bool was_improvement;
    ShrinkState<T> state;
};

// Shrinker

template<typename T>
ShrinkResult<T> no_improvement(ShrinkState<T> state) {
    return ShrinkResult<T>{false, state};
}

template<typename T, typename FN>
ShrinkResult<T> keep_if_better(RandomRun new_run, ShrinkState<T> state, Generator<T> generator, FN test_function) {
    if (new_run < state.run) {
        Recorded recorded_source = Recorded{new_run};
        GenResult<T> gen_result = generator(recorded_source);
        
        if (auto generated = std::get_if<Generated<T>>(&gen_result)) {
            try {
                test_function(generated->value);
            } catch (TestException &e) {
                return ShrinkResult<T>{true, ShrinkState<T>{new_run, generated->value, e.what()}};
            }
        }
    }
    return no_improvement(state);
}

template<typename T, typename FN, typename SET_FN>
ShrinkResult<T> binary_shrink(RAND_TYPE low, RAND_TYPE high, SET_FN update_run, ShrinkState<T> state, Generator<T> generator, FN test_function) {
    // Let's try with the best case first
    RandomRun run_with_low = update_run(low, state.run);
    ShrinkResult<T> after_low = keep_if_better(run_with_low, state, generator, test_function);
    if (after_low.was_improvement) {
        // We can't do any better
        return after_low;
    }
    // Gotta do the loop!
    ShrinkResult<T> result = after_low;
    while (low + 1 < high) {
        // TODO: do the average in a safer way?
        // https://stackoverflow.com/questions/24920503/what-is-the-right-way-to-find-the-average-of-two-values
        RAND_TYPE mid = low + (high - low) / 2;
        RandomRun run_with_mid = update_run(mid, state.run);
        ShrinkResult<T> after_mid = keep_if_better(run_with_mid, state, generator, test_function);
        if (after_mid.was_improvement) {
            high = mid;
        } else {
            low = mid;
        }
        result = after_mid;
        state = after_mid.state;
    }
    return result;
    
}

template<typename T, typename FN>
ShrinkResult<T> shrink_zero(ZeroChunk c, ShrinkState<T> state, Generator<T> generator, FN test_function) {
    // TODO do we need to copy? or is it done automatically
    RandomRun new_run = state.run;
    std::cout << "Run before zeroing: " << new_run << std::endl;
    std::cout << "TODO: we need to figure out if we need to copy here." << std::endl;
    size_t end = c.chunk.index + c.chunk.size;
    for (size_t i = c.chunk.index; i < end; i++) {
        new_run[i] = 0;
    }
    std::cout << "Run after zeroing: " << new_run << std::endl;
    std::cout << "TODO: is it any different from the following, original state.run?" << std::endl;
    std::cout << "state.run: " << state.run << std::endl;
    return keep_if_better(new_run, state, generator, test_function);
}

template<typename T, typename FN>
ShrinkResult<T> shrink_sort(SortChunk c, ShrinkState<T> state, Generator<T> generator, FN test_function) {
    // TODO do we need to copy? or is it done automatically
    RandomRun new_run = state.run;
    std::cout << "Run before sorting: " << new_run << std::endl;
    std::cout << "TODO: we need to figure out if we need to copy here." << std::endl;
    new_run.sort_chunk(c.chunk);
    std::cout << "Run after sorting: " << new_run << std::endl;
    std::cout << "TODO: is it any different from the following, original state.run?" << std::endl;
    std::cout << "state.run: " << state.run << std::endl;
    return keep_if_better(new_run, state, generator, test_function);
}

template<typename T, typename FN>
ShrinkResult<T> shrink_delete(DeleteChunkAndMaybeDecPrevious c, ShrinkState<T> state, Generator<T> generator, FN test_function) {
    RandomRun run_deleted = state.run.with_deleted(c.chunk);
    RandomRun run_decremented = run_deleted;
    run_decremented[c.chunk.index - 1]--;
    
    ShrinkResult<T> after_dec = keep_if_better(run_decremented, state, generator, test_function);
    if (after_dec.was_improvement) {
        return after_dec;
    }
    if (run_deleted == run_decremented) {
        return after_dec;
    }
    return keep_if_better(run_deleted, state, generator, test_function);
}

template<typename T, typename FN>
ShrinkResult<T> shrink_minimize(MinimizeChoice c, ShrinkState<T> state, Generator<T> generator, FN test_function) {
    RandomRun new_run = state.run;
    RAND_TYPE value = state.run[c.index];
    if (value == 0) {
        return no_improvement(state);
    } else {
        return binary_shrink(0,
                             value,
                             [c](RAND_TYPE new_value, RandomRun run){
                                RandomRun new_run = run;
                                new_run.set_at(c.index,new_value);
                                return new_run;
                             },
                             state,
                             generator,
                             test_function);
    }
}

template<typename T, typename FN>
ShrinkResult<T> shrink_with_cmd(ShrinkCmd cmd, ShrinkState<T> state, Generator<T> generator, FN test_function) {
    struct handler {
        ShrinkState<T> state;
        Generator<T> generator;
        FN test_function;
        explicit handler(ShrinkState<T> state, Generator<T> generator, FN test_function) : state(state), generator(generator), test_function(test_function) {}

        ShrinkResult<T> operator()(ZeroChunk c)                      { return shrink_zero(c, state, generator, test_function); }
        ShrinkResult<T> operator()(SortChunk c)                      { return shrink_sort(c, state, generator, test_function); }
        ShrinkResult<T> operator()(DeleteChunkAndMaybeDecPrevious c) { return shrink_delete(c, state, generator, test_function); }
        ShrinkResult<T> operator()(MinimizeChoice c)                 { return shrink_minimize(c, state, generator, test_function); }
    };
    return std::visit(handler{state, generator, test_function}, cmd);
}

template<typename T, typename FN>
ShrinkState<T> shrink_once(ShrinkState<T> state, Generator<T> generator, FN test_function) {
    auto cmds = shrink_cmds(state.run);
    for (ShrinkCmd cmd: cmds) {
        /* We're keeping the list of ShrinkCmds we generated from the initial
           RandomRun, as we try to shrink our current best RandomRun.

           That means some of the ShrinkCmds might have no chance to successfully
           finish (eg. the cmd's chunk is out of bounds of the run). That's what
           we check here and based on what we skip those cmds early.

           In the next `shrink -> shrink_once` loop we'll generate a better set
           of Cmds, more tailored to the current best RandomRun.
        */
        if (!has_a_chance(cmd, state.run)) {
            continue;
        }
        ShrinkResult<T> result = shrink_with_cmd(cmd, state, generator, test_function);
        if (result.was_improvement) {
            std::cout << "Shrunk with " << shrink_cmd_to_string(cmd) << ": " << result.state.run << std::endl;
            state = result.state;
        }
    }
    return state;
}

template<typename T, typename FN>
FailsWith<T> shrink(Generated<T> generated, Generator<T> generator, FN test_function, std::string fail_message) {
    std::cout << "Let's shrink: " << generated.value << std::endl;
    std::cout << "Original RandomRun: " << generated.run << std::endl;

    if (generated.run.is_empty()) {// We can't do any better
        return FailsWith<T>{generated.value, fail_message};
    }

    ShrinkState<T> new_state{generated.run, generated.value, fail_message};
    ShrinkState<T> current_state;
    do {
        current_state = new_state;
        new_state = shrink_once(current_state, generator, test_function);
    } while (new_state.run != current_state.run);

    return FailsWith<T>{new_state.value, new_state.fail_message};
}

#endif//PBT_SHRINK_H
