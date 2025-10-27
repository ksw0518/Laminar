#pragma once
#include "Board.h"
#include "Movegen.h"
#include "Search.h"
#include "Transpositions.h"
bool IsMoveCapture(Move& move);
bool IsMoveQuiet(Move& move);
bool IsMoveNoisy(Move& move);
bool IsEpCapture(Move& move);

void SortMoves(MoveList& ml, Board& board, ThreadData& data, TranspositionEntry& entry, uint64_t threats);
void SortNoisyMoves(MoveList& ml, Board& board, ThreadData& data);

struct ScoredMove
{
    int score;
    Move move;
};
