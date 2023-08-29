#ifndef PBT_SHRINK_CMD_H
#define PBT_SHRINK_CMD_H

#include "chunk.h"

#include <string>

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

#endif//PBT_SHRINK_CMD_H
