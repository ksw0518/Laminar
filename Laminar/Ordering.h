#pragma once
#include "Board.h"
#include "Movegen.h"
#include "Search.h"
bool IsMoveCapture(Move& move);
bool isMoveNoisy(Move& move);
void SortMoves(MoveList& ml, Board& board, ThreadData& data);
void SortNoisyMoves(MoveList& ml, Board& board);

struct ScoredMove
{
    int score;
    Move move;
};
