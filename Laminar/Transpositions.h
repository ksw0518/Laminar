#pragma once
#include "Movegen.h"
#include <cstdint>
constexpr int HFLOWER = 0;
constexpr int HFEXACT = 1;
constexpr int HFUPPER = 2;
constexpr int HFNONE = 3;
struct Move16
{
    uint16_t data;

    Move16() :
            data(0)
    {
    }

    Move16(int from, int to, uint8_t type)
    {
        data = (from & 0x3F) | ((to & 0x3F) << 6) | ((type & 0xF) << 12);
    }

    int from() const
    {
        return data & 0x3F;
    }
    int to() const
    {
        return (data >> 6) & 0x3F;
    }
    uint8_t type() const
    {
        return (data >> 12) & 0xF;
    }
};

inline uint16_t packData(uint8_t depth, uint8_t bound, bool ttPv)
{
    return (uint16_t(depth) & 0xFF) | ((uint16_t(bound) & 0x3) << 8) | (uint16_t(ttPv ? 1 : 0) << 10);
}

inline uint8_t unpackDepth(uint16_t data)
{
    return data & 0xFF;
}

inline uint8_t unpackBound(uint16_t data)
{
    return (data >> 8) & 0x3;
}

inline bool unpackTtPv(uint16_t data)
{
    return (data >> 10) & 0x1;
}

struct TranspositionEntry
{
    uint64_t zobristKey;
    int32_t score;

    Move16 bestMove = Move16(0, 0, 0);
    //uint8_t depth;
    //uint8_t bound = HFNONE;
    //bool ttPv = false;

    uint16_t packedInfo = packData(0, HFNONE, false);
};

TranspositionEntry ttLookUp(uint64_t zobrist);
void ClearTT();

void ttStore(TranspositionEntry& ttEntry, Board& board);
int get_hashfull();

void prefetchTT(uint64_t zobrist);
