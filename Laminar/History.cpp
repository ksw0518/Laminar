#include "History.h"
#include "Search.h"
#include "Tuneables.h"
#include <algorithm>
#include <cmath>
#include <cstdint>

void UpdateHistoryEntry(int16_t& entry, int16_t bonus)
{
    int clampedBonus = std::clamp<int16_t>(bonus, -MAX_HISTORY, MAX_HISTORY);
    entry += clampedBonus - entry * abs(clampedBonus) / MAX_HISTORY;
}

void UpdateMainHist(ThreadData& data, bool stm, int from, int to, int16_t bonus)
{
    int16_t& historyEntry = data.histories.mainHist[stm][from][to];
    UpdateHistoryEntry(historyEntry, bonus);
}
void MalusMainHist(ThreadData& data, MoveList& searchedQuietMoves, Move& bonus_move, int16_t malus)
{
    bool stm = bonus_move.Piece <= 5 ? White : Black;
    for (int i = 0; i < searchedQuietMoves.count; ++i)
    {
        Move& searchedMove = searchedQuietMoves.moves[i];
        if (searchedMove != bonus_move)
        {
            UpdateMainHist(data, stm, searchedMove.From, searchedMove.To, -malus);
        }
    }
}
int16_t GetSingleContHistScore(Move& move, const int offset, ThreadData& data)
{
    if (data.ply >= offset)
    {
        Move prevMove = data.searchStack[data.ply - offset].move;
        return data.histories.contHist[prevMove.Piece][prevMove.To][move.Piece][move.To];
    }
    return 0;
}
int GetContHistScore(Move& move, ThreadData& data)
{
    int onePlyContHist = GetSingleContHistScore(move, 1, data);
    //int twoPlyContHist = GetSingleContHistScore(move, 2, data);
    return onePlyContHist;
}
void UpdateSingleContHist(Move& move, const int bonus, const int offset, ThreadData& data)
{
    if (data.ply >= offset)
    {
        Move prevMove = data.searchStack[data.ply - offset].move;
        int clampedBonus = std::clamp(bonus, -MAX_CONTHIST, MAX_CONTHIST);
        int scaledBonus = clampedBonus - GetSingleContHistScore(move, offset, data) * abs(clampedBonus) / MAX_CONTHIST;

        data.histories.contHist[prevMove.Piece][prevMove.To][move.Piece][move.To] += scaledBonus;
    }
}
void UpdateContHist(Move& move, const int bonus, ThreadData& data)
{
    UpdateSingleContHist(move, bonus, 1, data);
    //UpdateSingleContHist(move, bonus, 2, data);
}
void MalusContHist(ThreadData& data, MoveList& searchedQuietMoves, Move& bonus_move, int16_t malus)
{
    bool stm = bonus_move.Piece <= 5 ? White : Black;
    for (int i = 0; i < searchedQuietMoves.count; ++i)
    {
        Move& searchedMove = searchedQuietMoves.moves[i];
        if (searchedMove != bonus_move)
        {
            UpdateContHist(searchedMove, -malus, data);
        }
    }
}
void UpdatePawnCorrHist(Board& board, const int depth, const int diff, ThreadData& data)
{
    uint64_t pawnKey = board.pawnKey;
    int16_t& pawnEntry = data.histories.pawnCorrHist[board.side][pawnKey % CORRHIST_SIZE];
    const int scaledDiff = diff * CORRHIST_GRAIN;
    const int newWeight = std::min(depth + 1, 16);
    pawnEntry = (pawnEntry * (CORRHIST_WEIGHT_SCALE - newWeight) + scaledDiff * newWeight) / CORRHIST_WEIGHT_SCALE;
    pawnEntry = std::clamp<int16_t>(pawnEntry, -CORRHIST_MAX, CORRHIST_MAX);
}
void UpdateCorrhists(Board& board, const int depth, const int diff, ThreadData& data)
{
    UpdatePawnCorrHist(board, depth, diff, data);
}

int AdjustEvalWithCorrHist(Board& board, const int rawEval, ThreadData& data)
{
    uint64_t pawnKey = board.pawnKey;
    const int& pawnEntry = data.histories.pawnCorrHist[board.side][pawnKey % CORRHIST_SIZE];

    int mate_found = MATESCORE - MAXPLY;

    int adjust = 0;

    adjust += pawnEntry * PAWN_CORRHIST_MULTIPLIER;
    adjust /= 128;

    return std::clamp(rawEval + adjust / CORRHIST_GRAIN, -mate_found + 1, mate_found - 1);
}