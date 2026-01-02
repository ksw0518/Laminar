#include "Transpositions.h"
#include "Const.h"
#include <cstdint>
#include <stddef.h>

size_t TTSize = 1; //initial value
TranspositionEntry* TranspositionTable = nullptr;
void Initialize_TT(int size)
{
    //std::cout <<"size" << size << "\n";
    uint64_t bytes = static_cast<uint64_t>(size) * 1024ULL * 1024ULL;

    //std::cout << bytes<<"\n";
    TTSize = bytes / sizeof(TranspositionEntry);

    if (TTSize % 2 != 0)
    {
        TTSize -= 1;
    }

    if (TranspositionTable)
        delete[] TranspositionTable;

    TranspositionTable = new TranspositionEntry[TTSize]();
}
void ClearTT()
{
    if (TranspositionTable && TTSize > 0)
    {
        std::fill(TranspositionTable, TranspositionTable + TTSize, TranspositionEntry());
    }
}
TranspositionEntry ttLookUp(uint64_t zobrist)
{
    int tt_index = zobrist % TTSize;
    return TranspositionTable[tt_index];
}
void ttStore(TranspositionEntry& ttEntry, Board& board)
{
    TranspositionTable[board.zobristKey % TTSize] = ttEntry;
}
int get_hashfull()
{
    int entryCount = 0;
    for (int i = 0; i < 1000; i++)
    {
        if (unpackBound(TranspositionTable[i].packedInfo) != HFNONE)
        {
            entryCount++;
        }
    }
    return entryCount;
}
int adjustMateStore(int score, int ply)
{
    //modify mate scores for TT
    if (score >= MATESCORE - MAXPLY)
    {
        return score + ply;
    }
    else if (score <= -(MATESCORE - MAXPLY))
    {
        return score - ply;
    }
    else
    {
        return score;
    }
}
int adjustMateProbe(int score, int ply)
{
    if (score >= MATESCORE - MAXPLY)
    {
        return score - ply;
    }
    else if (score <= -(MATESCORE - MAXPLY))
    {
        return score + ply;
    }
    else
    {
        return score;
    }
}
void prefetchTT(uint64_t zobrist)
{
    __builtin_prefetch(&TranspositionTable[zobrist % TTSize]);
}