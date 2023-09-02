#ifndef PBT_SHRINK_CMD_H
#define PBT_SHRINK_CMD_H

#include "random_run.h"
#include "chunk.h"

#include <string>
#include <ranges>

struct ZeroChunk { Chunk chunk; };
struct SortChunk { Chunk chunk; };
struct DeleteChunkAndMaybeDecPrevious { Chunk chunk; };
struct MinimizeChoice { size_t index; };

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

size_t max_chunk_size = 8;

/* Will generate ShrinkCmds for all chunks of sizes 1,2,3,4,8 in bounds of the
 * given RandomRun length.
 *
 * They will be given in a reverse order (largest chunks first), to maximize our
 * chances of saving work (minimizing the RandomRun faster).
 *
 * chunkCmds(10, false, [](Chunk c){ return SortChunk(c); })
 * -->
 * [ // Chunks of size 8
 *   SortChunk { chunk_size = 8, start_index = 0 }, // [XXXXXXXX..]
 *   SortChunk { chunk_size = 8, start_index = 1 }, // [.XXXXXXXX.]
 *   SortChunk { chunk_size = 8, start_index = 2 }, // [..XXXXXXXX]
 *
 *   // Chunks of size 4
 *   SortChunk { chunk_size = 4, start_index = 0 }, // [XXXX......]
 *   SortChunk { chunk_size = 4, start_index = 1 }, // [.XXXX.....]
 *   // ...
 *   SortChunk { chunk_size = 4, start_index = 5 }, // [.....XXXX.]
 *   SortChunk { chunk_size = 4, start_index = 6 }, // [......XXXX]
 *
 *   // Chunks of size 3
 *   SortChunk { chunk_size = 3, start_index = 0 }, // [XXX.......]
 *   SortChunk { chunk_size = 3, start_index = 1 }, // [.XXX......]
 *   // ...
 *   SortChunk { chunk_size = 3, start_index = 6 }, // [......XXX.]
 *   SortChunk { chunk_size = 3, start_index = 7 }, // [.......XXX]
 *
 *   // Chunks of size 2
 *   SortChunk { chunk_size = 2, start_index = 0 }, // [XX........]
 *   SortChunk { chunk_size = 2, start_index = 1 }, // [.XX.......]
 *   // ...
 *   SortChunk { chunk_size = 2, start_index = 7 }, // [.......XX.]
 *   SortChunk { chunk_size = 2, start_index = 8 }, // [........XX]
 * ]
 */
template<typename T, typename FN>
std::vector<T> chunk_cmds(size_t length, bool allow_chunks_size1, FN chunk_to_cmd) {
    std::vector<T> all;
    
    uint8_t min_chunk_size = allow_chunks_size1 ? 1 : 2;
    
    std::vector<uint8_t> sizes = {8,4,3,2,1};
    
    for (auto chunk_size : sizes
                         | std::views::filter([=](auto size){ return size <= length && size >= min_chunk_size; }))
    {
        auto cmds = std::views::iota((size_t)0, length - chunk_size + 1)
                        | std::views::transform([=](auto const i){ return chunk_to_cmd(Chunk{chunk_size, i}); });
        
        all.insert(all.end(), cmds.begin(), cmds.end());
    }
    
    return all;
    
}

std::vector<ShrinkCmd> deletion_cmds(size_t length) {
    return chunk_cmds<ShrinkCmd>(
            length,
            true,
            [](Chunk c) { return DeleteChunkAndMaybeDecPrevious{c}; });
}

std::vector<ShrinkCmd> minimize_cmds(size_t length) {
    auto cmds = std::views::iota((size_t)0, length)
                    | std::views::transform([](auto const i){ return MinimizeChoice{i}; });
    return std::vector<ShrinkCmd>(cmds.begin(), cmds.end());
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
    size_t length = r.length();
    
    std::vector<std::vector<ShrinkCmd>> all;
    all.push_back(deletion_cmds(length));
    all.push_back(zero_cmds(length));
    all.push_back(sort_cmds(length));
    all.push_back(minimize_cmds(length));
    
    auto joined = all | std::views::join;
    return std::vector<ShrinkCmd>(joined.begin(), joined.end());
}

bool has_a_chance(ShrinkCmd cmd, RandomRun run) {
    struct predicate {
        RandomRun &run;
        explicit predicate(RandomRun &run) : run(run) {}

        bool operator()(ZeroChunk c) { return run.has_a_chance(c.chunk); }
        bool operator()(SortChunk c) { return run.has_a_chance(c.chunk); }
        bool operator()(DeleteChunkAndMaybeDecPrevious c) { return run.has_a_chance(c.chunk); }
        bool operator()(MinimizeChoice c) { return run.length() > c.index; }
    };
    return std::visit(predicate{run}, cmd);
}

#endif//PBT_SHRINK_CMD_H
