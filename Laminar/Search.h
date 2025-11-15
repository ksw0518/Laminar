#pragma once
#include "Board.h"
#include "Const.h"
#include "Movegen.h"
#include "Transpositions.h"
#include <atomic>
#include <chrono>
#include <cstdint>
extern bool IsUCI;
extern bool stopSearch;
struct SearchData
{
    Move move;
    int staticEval = 0;
    int reduction = 0;
    bool check = false;
    AccumulatorPair last_accumulator;
};

struct Histories
{
    //[stm][from][to][from threat][to threat]
    int16_t mainHist[2][64][64][2][2];

    //[attacking piece][to][captured piece]
    int16_t captureHistory[12][64][12];

    //[stm][pawnKey]
    int16_t pawnCorrHist[2][CORRHIST_SIZE];

    //[stm][minorkey]
    int16_t minorCorrHist[2][CORRHIST_SIZE];

    //[stm][piece color][nonpawn key]
    int16_t nonPawnCorrHist[2][2][CORRHIST_SIZE];

    //[piece][to][piece][to]
    int16_t contHist[12][64][12][64];

    //[piece][to][pawnKey]
    int16_t pawnHist[12][64][1024];
};

struct alignas(64) ThreadData
{
    SearchData searchStack[MAXPLY];
    std::chrono::steady_clock::time_point clockStart;
    int64_t searchNodeCount = 0;
    int64_t SearchTime = -1;
    int64_t hardNodeBound = -1;
    uint64_t nodesPerMove[64][64];
    int ply = 0;
    int currDepth = 0;
    int selDepth = 0;
    int minNmpPly = 0;
    int pvLengths[MAXPLY + 1] = {};
    Histories histories;
    std::atomic<bool> stopSearch{false};
    bool isMainThread = true;
    Move killerMoves[MAXPLY + 1];
    Move pvTable[MAXPLY + 1][MAXPLY + 1];
};

struct SearchLimitations
{
    int64_t HardTimeLimit = -1;
    int64_t SoftTimeLimit = -1;
    int64_t SoftNodeLimit = -1;
    int64_t HardNodeLimit = -1;
    SearchLimitations(int hardTime = -1, int softTime = -1, int64_t softNode = -1, int64_t hardNode = -1) :
            HardTimeLimit(hardTime), SoftTimeLimit(softTime), SoftNodeLimit(softNode), HardNodeLimit(hardNode)
    {
    }
};
struct CopyMake
{
    int lastEp;
    uint8_t lastCastle;
    bool lastside;
    int captured_piece;
    uint64_t last_zobrist;
    uint64_t last_pawnKey;
    uint64_t last_white_np;
    uint64_t last_black_np;
    uint64_t last_minor;
    uint64_t last_irreversible;
    uint64_t last_halfmove;
};
std::pair<Move, int> IterativeDeepening(
    Board& board,
    int depth,
    SearchLimitations searchLimits,
    ThreadData& data,
    bool isBench = false
);

void Initialize_TT(int size);
void InitializeLMRTable();
void InitializeSearch(ThreadData& data);
void InitNNUE();
void refresh_if_cross(Move& move, Board& board);
bool compareMoves(Move move1, Move16 move2);