#include "Ordering.h"
#include "Bit.h"
#include "Board.h"
#include "Const.h"
#include "History.h"
#include "Movegen.h"
#include "SEE.h"
#include "Search.h"
#include "Transpositions.h"
#include "Tuneables.h"
#include <algorithm>

bool IsMoveNoisy(Move& move)
{
    return (move.Type & (captureFlag | promotionFlag)) != 0;
}
bool IsMoveQuiet(Move& move)
{
    return !IsMoveCapture(move);
}
bool IsMoveCapture(Move& move)
{
    return (move.Type & captureFlag) != 0;
}
bool IsEpCapture(Move& move)
{
    return (move.Type & ep_capture) != 0;
}
int GetMoveScore(Move& move, Board& board, ThreadData& data, TranspositionEntry& entry, uint64_t threat)
{
    if (board.zobristKey == entry.zobristKey && compareMoves(move, entry.bestMove))
    {
        return 900000000;
    }
    else if (IsMoveCapture(move))
    {
        int attacker = get_piece(move.Piece, White);

        int victim = IsEpCapture(move) ? P : get_piece(board.mailbox[move.To], White);
        int attackerValue = *SEEPieceValues[attacker];
        int victimValue = *SEEPieceValues[victim];
        int coloredVictim = get_piece(victim, 1 - board.side);

        int mvvlvaValue = victimValue * 100 - attackerValue;
        bool toThreat = Get_bit(threat, move.To);

        int histScore = data.histories.captureHistory[move.Piece][move.To][coloredVictim][toThreat];
        int seeValue = SEE(board, move, PVS_SEE_ORDERING) ? 200000 : -1000000;

        return mvvlvaValue + seeValue + histScore;
    }
    else if (data.killerMoves[data.ply] == move)
    {
        return 20000;
    }
    else
    {
        bool fromThreat = Get_bit(threat, move.From);
        bool toThreat = Get_bit(threat, move.To);

        int mainHistValue = data.histories.mainHist[board.side][move.From][move.To][fromThreat][toThreat];
        int contHistValue = GetContHistScore(move, data);

        int historyScore = mainHistValue + contHistValue;

        //quiet move max value = 16384
        return historyScore - MAX_HISTORY - MAX_CONTHIST;
    }
}
int QsearchGetMoveScore(Move& move, Board& board, ThreadData& data, uint64_t threat)
{
    if (IsMoveCapture(move))
    {
        int attacker = get_piece(move.Piece, White);

        int victim = IsEpCapture(move) ? P : get_piece(board.mailbox[move.To], White);
        int attackerValue = *SEEPieceValues[attacker];
        int victimValue = *SEEPieceValues[victim];
        int coloredVictim = get_piece(victim, 1 - board.side);

        int mvvlvaValue = victimValue * 100 - attackerValue;
        bool toThreat = Get_bit(threat, move.To);

        int histScore = data.histories.captureHistory[move.Piece][move.To][coloredVictim][toThreat];
        int seeValue = SEE(board, move, QS_SEE_ORDERING) ? 0 : -1000000;

        return mvvlvaValue + histScore + seeValue;
    }
    else
    {
        return -90000;
    }
}
void SortMoves(MoveList& ml, Board& board, ThreadData& data, TranspositionEntry& entry, uint64_t threats)
{
    ScoredMove scored[256];

    for (int i = 0; i < ml.count; ++i)
    {
        scored[i].score = GetMoveScore(ml.moves[i], board, data, entry, threats);
        scored[i].move = ml.moves[i];
    }

    std::stable_sort(
        scored,
        scored + ml.count,
        [](const ScoredMove& a, const ScoredMove& b) { return a.score > b.score; }
    );

    for (int i = 0; i < ml.count; ++i)
    {
        ml.moves[i] = scored[i].move;
    }
}
void SortNoisyMoves(MoveList& ml, Board& board, ThreadData& data, uint64_t threats)
{
    ScoredMove scored[256];

    for (int i = 0; i < ml.count; ++i)
    {
        scored[i].score = QsearchGetMoveScore(ml.moves[i], board, data, threats);
        scored[i].move = ml.moves[i];
    }

    std::stable_sort(
        scored,
        scored + ml.count,
        [](const ScoredMove& a, const ScoredMove& b) { return a.score > b.score; }
    );

    for (int i = 0; i < ml.count; ++i)
    {
        ml.moves[i] = scored[i].move;
    }
}