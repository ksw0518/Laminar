#pragma once
#include "Board.h"
#include "Const.h"
#include "Movegen.h"
#include <chrono>
#include <cstdint>
extern bool IsUCI;
struct SearchData
{
    Move move;
    int staticEval = 0;
};
struct Histories
{
    //[stm][from][to][from threat][to threat]
    int16_t mainHist[2][64][64][2][2];

    //[stm][pawnKey]
    int16_t pawnCorrHist[2][CORRHIST_SIZE];

    //[stm][piece color][nonpawn key]
    int16_t nonPawnCorrHist[2][2][CORRHIST_SIZE];

    //[piece][to][piece][to]
    int16_t contHist[12][64][12][64];
};
struct ThreadData
{
    std::chrono::steady_clock::time_point clockStart;
    int ply = 0;
    int64_t searchNodeCount = 0;
    int64_t SearchTime = -1;
    int currDepth = 0;
    bool stopSearch = false;
    int selDepth = 0;
    SearchData searchStack[MAXPLY];
    int pvLengths[MAXPLY + 1] = {};
    Move pvTable[MAXPLY + 1][MAXPLY + 1];
    Histories histories;
    int minNmpPly = 0;
};
struct SearchLimitations
{
    int64_t HardTimeLimit = -1;
    int64_t SoftTimeLimit = -1;
    //int64_t SoftNodeLimit = -1;
    //int64_t HardNodeLimit = -1;
    SearchLimitations(int hardTime = -1, int softTime = -1, int64_t softNode = -1, int64_t hardNode = -1) :
            HardTimeLimit(hardTime), SoftTimeLimit(softTime)
    //SoftNodeLimit(softNode),
    //HardNodeLimit(hardNode)
    {
    }
};

std::pair<Move, int> IterativeDeepening(
    Board& board,
    int depth,
    SearchLimitations& searchLimits,
    ThreadData& data,
    bool isBench = false
);
void Initialize_TT(int size);
void InitializeLMRTable();
void InitializeSearch(ThreadData& data);
void InitNNUE();
void refresh_if_cross(Move& move, Board& board);
bool operator==(const Accumulator& a, const Accumulator& b);
bool operator==(const AccumulatorPair& a, const AccumulatorPair& b);