#ifndef PBT_CHUNK_H
#define PBT_CHUNK_H

#include <string>

struct Chunk {
    uint8_t size;
    size_t index;
};

std::string chunk_to_string(Chunk c) {
    return "Chunk<size=" + std::to_string(c.size) + ", i=" + std::to_string(c.index) + ">";
}

#endif//PBT_CHUNK_H
