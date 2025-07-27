#pragma once
#include "Search.h"
#include <cstdint>
void UpdateMainHist(ThreadData& data, bool stm, int from, int to, int16_t bonus);
void MalusMainHist(ThreadData& data, MoveList& searchedQuietMoves, Move& bonus_move, int16_t malus);
void UpdateCorrhists(Board& board, const int depth, const int diff, ThreadData& data);
int AdjustEvalWithCorrHist(Board& board, const int rawEval, ThreadData& data);
void MalusContHist(ThreadData& data, MoveList& searchedQuietMoves, Move& bonus_move, int16_t malus);
void UpdateContHist(Move& move, const int bonus, ThreadData& data);
int GetContHistScore(Move& move, ThreadData& data);