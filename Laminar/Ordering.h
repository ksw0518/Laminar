#pragma once
#include "Board.h"
#include "Movegen.h"
#include "Search.h"
#include "Transpositions.h"
struct ScoredMove
{
    int score;
    Move move;
};
void ChooseNextMove(ScoredMove* scored, MoveList& ml, int moveCount);
bool IsMoveCapture(Move& move);
bool IsMoveQuiet(Move& move);
bool IsMoveNoisy(Move& move);
void ScoreMoves(
    ScoredMove* scored,
    MoveList& ml,
    Board& board,
    ThreadData& data,
    TranspositionEntry& entry,
    uint64_t threats
);
void SortNoisyMoves(MoveList& ml, Board& board, ThreadData& data);

