#pragma once
#include "Movegen.h"
#include <cstdint>
constexpr int HFLOWER = 0;
constexpr int HFEXACT = 1;
constexpr int HFUPPER = 2;
struct TranspositionEntry
{
    uint64_t zobristKey;
    Move bestMove;
    uint8_t depth = -5;
    uint8_t bound;
    int32_t score;
};
TranspositionEntry ttLookUp(uint64_t zobrist);
void ttStore(TranspositionEntry& ttEntry, Board& board);
int get_hashfull();