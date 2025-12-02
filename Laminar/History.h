#pragma once
#include "Search.h"
#include <cstdint>
void UpdateSEHist(ThreadData& data, int piece, int to, int16_t bonus);
void UpdateMainHist(ThreadData& data, bool stm, int from, int to, int16_t bonus, uint64_t threat);
void MalusMainHist(ThreadData& data, MoveList& searchedQuietMoves, Move& bonus_move, int16_t malus, uint64_t threat);
void UpdateCaptHist(ThreadData& data, bool attacker, int to, int victim, int16_t bonus);
void MalusCaptHist(ThreadData& data, MoveList& searchedNoisyMoves, Move& bonus_move, int16_t malus, Board& board);
void UpdateCorrhists(Board& board, const int depth, const int diff, ThreadData& data);
int AdjustEvalWithCorrHist(Board& board, const int rawEval, ThreadData& data);
void MalusContHist(ThreadData& data, MoveList& searchedQuietMoves, Move& bonus_move, int16_t malus);
void UpdateContHist(Move& move, const int bonus, ThreadData& data);
int GetContHistScore(Move& move, ThreadData& data);