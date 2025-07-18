#pragma once
#include "Movegen.h"
#include "Board.h"
bool IsMoveCapture(Move& move);
void SortMoves(MoveList& ml, Board& board);
struct ScoredMove {
    int score;
    Move move;
};
