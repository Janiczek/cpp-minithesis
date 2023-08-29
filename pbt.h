#ifndef PBT_PBT_H
#define PBT_PBT_H

#include <iostream>
#include <map>
#include <numeric>
#include <optional>
#include <random>
#include <utility>

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

// Chunk

struct Chunk {
    uint8_t size;
    size_t index;
};

std::string chunk_to_string(Chunk c) {
    return "Chunk<size=" + std::to_string(c.size) + ", i=" + std::to_string(c.index) + ">";
}

// ShrinkCmd

struct ZeroChunk {
    Chunk chunk;
};
struct SortChunk {
    Chunk chunk;
};
struct DeleteChunkAndMaybeDecPrevious {
    Chunk chunk;
};
struct MinimizeChoice {
    size_t index;
};

using ShrinkCmd = std::variant<
        ZeroChunk,
        SortChunk,
        DeleteChunkAndMaybeDecPrevious,
        MinimizeChoice>;

struct shrink_cmd_stringifier {
    std::string operator()(ZeroChunk c)                      { return "ZeroChunk(" + chunk_to_string(c.chunk) + ")"; }
    std::string operator()(SortChunk c)                      { return "SortChunk(" + chunk_to_string(c.chunk) + ")"; }
    std::string operator()(DeleteChunkAndMaybeDecPrevious c) { return "DeleteChunkAndMaybeDecPrevious(" + chunk_to_string(c.chunk) + ")"; }
    std::string operator()(MinimizeChoice c)                 { return "MinimizeChoice(i=" + std::to_string(c.index) + ")"; }
};

std::string shrink_cmd_to_string(ShrinkCmd cmd) {
    return std::visit(shrink_cmd_stringifier{}, cmd);
}

// RandomRun

class RandomRun {
public:
    RandomRun() {
        run = std::vector<RAND_TYPE>();
        setup();
    }
    RandomRun(const RandomRun &rhs) {
        run = rhs.run;
        setup();
    }
    RandomRun(const std::vector<RAND_TYPE> &rhs) {
        run = rhs;
        setup();
    }
    void setup() {
        // TODO: Is this too wasteful? Should we only do this for Live runs, not for Recorded ones?
        run.reserve(MAX_RANDOMRUN_LENGTH);
    }
    [[nodiscard]] bool is_empty() const { return run.empty(); }
    [[nodiscard]] bool is_full() const { return run.size() >= MAX_RANDOMRUN_LENGTH; }
    bool has_a_chance(Chunk c) {
        // size: 6
        // 0 1 2 3 4 5
        //     ^ ^ ^ ^
        // chunk size 4
        //       index 2
        return (c.index + c.size <= run.size());
    }
    bool has_a_chance(ShrinkCmd cmd) {
        struct predicate {
            RandomRun &run;
            explicit predicate(RandomRun &run) : run(run) {}

            bool operator()(ZeroChunk c) { return run.has_a_chance(c.chunk); }
            bool operator()(SortChunk c) { return run.has_a_chance(c.chunk); }
            bool operator()(DeleteChunkAndMaybeDecPrevious c) { return run.has_a_chance(c.chunk); }
            bool operator()(MinimizeChoice c) { return run.length() > c.index; }
        };
        return std::visit(predicate{*this}, cmd);
    }
    void push_back(RAND_TYPE n) { run.push_back(n); }
    size_t length() const { return run.size(); }
    RAND_TYPE next() { return run[curr_index++]; }
    friend std::ostream &operator<<(std::ostream &os, const RandomRun &random_run) {
        auto size = random_run.run.size();
        os << "[";
        for (size_t i = 0; i < size; i++) {
            os << random_run.run[i];
            if (i < size - 1) { os << ","; }
        }
        os << "]";
        return os;
    }
    RAND_TYPE& operator[](size_t index) { return run[index]; }
    RAND_TYPE at(size_t index) const { return run[index]; }
    void set_at(size_t index, RAND_TYPE value) { run[index] = value; }
    bool operator==(const RandomRun &rhs) const { return run == rhs.run; }
    bool operator!=(const RandomRun &rhs) const { return run != rhs.run; }
    bool operator< (const RandomRun &rhs) const {
        if (length() < rhs.length()) { return true; }
        if (length() > rhs.length()) { return false; }
        for (size_t i = 0; i < length(); i++) {
            if (at(i) < rhs.at(i)) return true;
            if (at(i) > rhs.at(i)) return false;
        }
        return false;
    }
    void sort_chunk(Chunk c) {
        std::partial_sort(run.begin() + c.index,
                          run.begin() + c.index + c.size,
                          run.end());
    }
    RandomRun with_deleted(Chunk c) {
        // TODO bounds checking?
        std::vector<RAND_TYPE> new_run;
        new_run.reserve(run.size() - c.size);

        new_run.insert(new_run.end(), run.begin(), run.begin() + c.index);
        new_run.insert(new_run.end(), run.begin() + c.index + c.size, run.end());

        return RandomRun(new_run);
    }

private:
    std::vector<RAND_TYPE> run;
    size_t curr_index = 0;
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
            auto descByValue = [](const auto &a, const auto &b) { return a.second > b.second; };
            auto size = std::min(5, static_cast<int>(cgv.rejections.size()));
            std::vector<std::pair<std::string, int>> sorted_items(cgv.rejections.begin(), cgv.rejections.end());
            std::partial_sort(sorted_items.begin(),
                              sorted_items.begin() + size,
                              sorted_items.end(),
                              descByValue);

            std::string reasons;
            for (int i = 0; i < size; i++) {
                reasons += "\n - " + sorted_items[i].first;
            }

            return "Cannot generate values. Most common reasons:" + reasons;
        }
    };
    return std::visit(stringifier{}, result);
}

// ShrinkState

template<typename T>
struct ShrinkState {
    RandomRun run;
    T value;
    std::string fail_message;
};

// ShrinkResult

template<typename T>
struct ShrinkResult {
    bool was_improvement;
    ShrinkState<T> state;
};

// Shrink Cmd creation

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

// Runner

template<typename T, typename FN>
TestResult<T> run(Generator<T> generator, FN test_function) {

    RandomRun empty_run;
    std::random_device r;
    std::mt19937 rng(r());
    Live live_source{empty_run, rng};

    for (int i = 0; i < MAX_GENERATED_VALUES_PER_TEST; i++) {
        std::map<std::string, int> rejections;
        bool generated_successfully = false;
        for (int gen_attempt = 0; gen_attempt < MAX_GEN_ATTEMPTS_PER_VALUE && !generated_successfully; gen_attempt++) {
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
