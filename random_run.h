#ifndef PBT_RANDOM_RUN_H
#define PBT_RANDOM_RUN_H

#include "chunk.h"
#include "shrink_cmd.h"

#include <iostream>
#include <vector>

#define MAX_RANDOMRUN_LENGTH (64 * 1024) // 64k items
#define RAND_TYPE unsigned int

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

#endif//PBT_RANDOM_RUN_H
