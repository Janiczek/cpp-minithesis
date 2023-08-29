#ifndef PBT_SHRINK_H
#define PBT_SHRINK_H

#include "gen_result.h"
#include "generator.h"
#include "rand_source.h"
#include "random_run.h"
#include "test_exception.h"
#include "test_result.h"

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

size_t max_chunk_size = 8;

/* Will generate ShrinkCmds for all chunks of sizes 1,2,4,8 in bounds of the
 * given RandomRun length.
 *
 * chunkCmds(10, false, [](Chunk c){ return SortChunk(c); })
 * -->
 * [ // Chunks of size 8
 *   SortChunk { chunkSize = 8, startIndex = 2 }, // [..XXXXXXXX]
 *   SortChunk { chunkSize = 8, startIndex = 1 }, // [.XXXXXXXX.]
 *   SortChunk { chunkSize = 8, startIndex = 0 }, // [XXXXXXXX..]
 *
 *   // Chunks of size 4
 *   SortChunk { chunkSize = 4, startIndex = 6 }, // [......XXXX]
 *   SortChunk { chunkSize = 4, startIndex = 5 }, // [.....XXXX.]
 *   // ...
 *   SortChunk { chunkSize = 4, startIndex = 1 }, // [.XXXX.....]
 *   SortChunk { chunkSize = 4, startIndex = 0 }, // [XXXX......]
 *
 *   // Chunks of size 3
 *   SortChunk { chunkSize = 3, startIndex = 7 }, // [.......XXX]
 *   SortChunk { chunkSize = 3, startIndex = 6 }, // [......XXX.]
 *   // ...
 *   SortChunk { chunkSize = 3, startIndex = 1 }, // [.XXX......]
 *   SortChunk { chunkSize = 3, startIndex = 0 }, // [XXX.......]
 *
 *   // Chunks of size 2
 *   SortChunk { chunkSize = 2, startIndex = 8 }, // [........XX]
 *   SortChunk { chunkSize = 2, startIndex = 7 }, // [.......XX.]
 *   // ...
 *   SortChunk { chunkSize = 2, startIndex = 1 }, // [.XX.......]
 *   SortChunk { chunkSize = 2, startIndex = 0 }, // [XX........]
 * ]
 */
template<typename T, typename FN>
std::vector<T> chunk_cmds(size_t length, bool allow_chunks_size1, FN chunk_to_cmd) {
    std::vector<T> acc;
    uint8_t chunk_size = allow_chunks_size1 ? 1 : 2;
    while (chunk_size <= max_chunk_size) {
        size_t start_index = 0;
        while ((int)start_index <= ((int)length - chunk_size)) {
            acc.push_back(chunk_to_cmd(Chunk{chunk_size, start_index}));
            start_index++;
        }

        if (chunk_size == 2 || chunk_size == 3) {
            // chunks of 3 are common, so we don't _just_ double all the time.
            chunk_size++;
        } else {
            chunk_size *= 2;
        }
    }
    return acc;
}

std::vector<ShrinkCmd> deletion_cmds(size_t length) {
    return chunk_cmds<ShrinkCmd>(
            length,
            true,
            [](Chunk c) { return DeleteChunkAndMaybeDecPrevious{c}; });
}

std::vector<ShrinkCmd> minimize_cmds(size_t length) {
    std::vector<ShrinkCmd> acc;
    for (size_t i = 0; i < length; i++) {
        acc.push_back(MinimizeChoice{i});
    }
    return acc;
}

std::vector<ShrinkCmd> sort_cmds(size_t length) {
    bool allow_chunks_size1 = false; // doesn't make sense for sorting
    return chunk_cmds<ShrinkCmd>(
            length,
            allow_chunks_size1,
            [](Chunk c) { return SortChunk{c}; });
}

std::vector<ShrinkCmd> zero_cmds(size_t length) {
    bool allow_chunks_size1 = false; // already happens in binary search
    return chunk_cmds<ShrinkCmd>(
            length,
            allow_chunks_size1,
            [](Chunk c) { return ZeroChunk{c}; });
}

std::vector<ShrinkCmd> shrink_cmds(RandomRun r) {
    std::vector<ShrinkCmd> acc;

    size_t length = r.length();
    std::vector<ShrinkCmd> deletions = deletion_cmds(length);
    std::vector<ShrinkCmd> zeros = zero_cmds(length);
    std::vector<ShrinkCmd> sorts = sort_cmds(length);
    std::vector<ShrinkCmd> minimizes = minimize_cmds(length);

    acc.insert(acc.end(), std::make_move_iterator(deletions.begin()), std::make_move_iterator(deletions.end()));
    acc.insert(acc.end(), std::make_move_iterator(zeros.begin()),     std::make_move_iterator(zeros.end()));
    acc.insert(acc.end(), std::make_move_iterator(sorts.begin()),     std::make_move_iterator(sorts.end()));
    acc.insert(acc.end(), std::make_move_iterator(minimizes.begin()), std::make_move_iterator(minimizes.end()));

    return acc;
}

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
        if (!state.run.has_a_chance(cmd)) {
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
