#include "History.h"
#include "Bit.h"
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

void UpdateMainHist(ThreadData& data, bool stm, int from, int to, int16_t bonus, uint64_t threat)
{
    bool fromThreat = Get_bit(threat, from);
    bool toThreat = Get_bit(threat, to);
    int16_t& historyEntry = data.histories.mainHist[stm][from][to][fromThreat][toThreat];
    UpdateHistoryEntry(historyEntry, bonus);
}

void UpdateCaptHist(ThreadData& data, bool attacker, int from, int to, int victim, int16_t bonus, uint64_t threat)
{
    bool fromThreat = Get_bit(threat, from);
    bool toThreat = Get_bit(threat, to);
    int16_t& historyEntry = data.histories.captureHistory[attacker][to][victim][fromThreat][toThreat];
    UpdateHistoryEntry(historyEntry, bonus);
}

void MalusMainHist(ThreadData& data, MoveList& searchedQuietMoves, Move& bonus_move, int16_t malus, uint64_t threat)
{
    bool stm = bonus_move.Piece <= 5 ? White : Black;
    for (int i = 0; i < searchedQuietMoves.count; ++i)
    {
        Move& searchedMove = searchedQuietMoves.moves[i];

        if (searchedMove != bonus_move)
        {
            UpdateMainHist(data, stm, searchedMove.From, searchedMove.To, -malus, threat);
        }
    }
}

void MalusCaptHist(
    ThreadData& data,
    MoveList& searchedNoisyMoves,
    Move& bonus_move,
    int16_t malus,
    Board& board,
    uint64_t threat
)
{
    bool stm = bonus_move.Piece <= 5 ? White : Black;
    for (int i = 0; i < searchedNoisyMoves.count; ++i)
    {
        Move& searchedMove = searchedNoisyMoves.moves[i];

        if (searchedMove != bonus_move)
        {
            UpdateCaptHist(
                data,
                searchedMove.Piece,
                searchedMove.From,
                searchedMove.To,
                board.mailbox[searchedMove.To],
                -malus,
                threat
            );
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
    int twoPlyContHist = GetSingleContHistScore(move, 2, data);
    return onePlyContHist + twoPlyContHist;
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
    UpdateSingleContHist(move, bonus, 2, data);
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

void UpdateNonPawnCorrHist(Board& board, const int depth, const int diff, ThreadData& data)
{
    uint64_t whiteKey = board.whiteNonPawnKey;
    uint64_t blackKey = board.blackNonPawnKey;

    const int scaledDiff = diff * CORRHIST_GRAIN;
    const int newWeight = std::min(depth + 1, 16);

    int16_t& whiteEntry = data.histories.nonPawnCorrHist[White][board.side][whiteKey % CORRHIST_SIZE];

    whiteEntry = (whiteEntry * (CORRHIST_WEIGHT_SCALE - newWeight) + scaledDiff * newWeight) / CORRHIST_WEIGHT_SCALE;
    whiteEntry = std::clamp<int16_t>(whiteEntry, -CORRHIST_MAX, CORRHIST_MAX);

    int16_t& blackEntry = data.histories.nonPawnCorrHist[Black][board.side][blackKey % CORRHIST_SIZE];

    blackEntry = (blackEntry * (CORRHIST_WEIGHT_SCALE - newWeight) + scaledDiff * newWeight) / CORRHIST_WEIGHT_SCALE;
    blackEntry = std::clamp<int16_t>(blackEntry, -CORRHIST_MAX, CORRHIST_MAX);
}

void UpdateMinorCorrHist(Board& board, const int depth, const int diff, ThreadData& data)
{
    uint64_t minorKey = board.minorKey;
    int16_t& minorEntry = data.histories.minorCorrHist[board.side][minorKey % CORRHIST_SIZE];
    const int scaledDiff = diff * CORRHIST_GRAIN;
    const int newWeight = std::min(depth + 1, 16);
    minorEntry = (minorEntry * (CORRHIST_WEIGHT_SCALE - newWeight) + scaledDiff * newWeight) / CORRHIST_WEIGHT_SCALE;
    minorEntry = std::clamp<int16_t>(minorEntry, -CORRHIST_MAX, CORRHIST_MAX);
}

void UpdateCorrhists(Board& board, const int depth, const int diff, ThreadData& data)
{
    UpdatePawnCorrHist(board, depth, diff, data);
    UpdateNonPawnCorrHist(board, depth, diff, data);
    UpdateMinorCorrHist(board, depth, diff, data);
}

int16_t GetPawnCorrHistValue(Board& board, ThreadData& data)
{
    uint64_t pawnKey = board.pawnKey;
    return data.histories.pawnCorrHist[board.side][pawnKey % CORRHIST_SIZE];
}

int16_t GetWhiteNonPawnCorrHistValue(Board& board, ThreadData& data)
{
    uint64_t whiteNPKey = board.whiteNonPawnKey;
    return data.histories.nonPawnCorrHist[White][board.side][whiteNPKey % CORRHIST_SIZE];
}

int16_t GetBlackNonPawnCorrHistValue(Board& board, ThreadData& data)
{
    uint64_t blackNPKey = board.blackNonPawnKey;
    return data.histories.nonPawnCorrHist[Black][board.side][blackNPKey % CORRHIST_SIZE];
}

int16_t GetMinorCorrHistValue(Board& board, ThreadData& data)
{
    uint64_t minorKey = board.minorKey;
    return data.histories.minorCorrHist[board.side][minorKey % CORRHIST_SIZE];
}

int AdjustEvalWithCorrHist(Board& board, const int rawEval, ThreadData& data)
{
    int pawnEntry = GetPawnCorrHistValue(board, data);
    int whiteNPEntry = GetWhiteNonPawnCorrHistValue(board, data);
    int blackNPEntry = GetBlackNonPawnCorrHistValue(board, data);
    int minorEntry = GetMinorCorrHistValue(board, data);

    int mate_found = MATESCORE - MAXPLY;

    int adjust = 0;

    adjust += pawnEntry * PAWN_CORRHIST_MULTIPLIER;
    adjust += (whiteNPEntry + blackNPEntry) * NONPAWN_CORRHIST_MULTIPLIER;
    adjust += (minorEntry)*MINOR_CORRHIST_MULTIPLIER;
    adjust /= 128;

    return std::clamp(rawEval + adjust / CORRHIST_GRAIN, -mate_found + 1, mate_found - 1);
}