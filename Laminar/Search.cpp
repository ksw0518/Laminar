#include "Search.h"
#include "Bit.h"
#include "Board.h"
#include "Const.h"
#include "Evaluation.h"
#include "History.h"
#include "Movegen.h"
#include "Ordering.h"
#include "Transpositions.h"
#include "Tuneables.h"
#include <chrono>
#include <cmath>
#include <cstring>
#include <iostream>
#include <limits>

inline int QuiescentSearch(Board& board, ThreadData& data, int alpha, int beta)
{
    auto now = std::chrono::steady_clock::now();
    int64_t elapsedMS = std::chrono::duration_cast<std::chrono::milliseconds>(now - data.clockStart).count();
    if (data.stopSearch || elapsedMS > data.SearchTime)
    {
        data.stopSearch = true;
        return 0;
    }

    bool isPvNode = beta - alpha > 1;

    int staticEval = Evaluate(board);

    int currentPly = data.ply;
    data.selDepth = std::max(currentPly, data.selDepth);
    if (currentPly >= MAXPLY - 1)
    {
        return staticEval;
    }

    int score = -MAXSCORE;
    int bestValue = staticEval;
    if (bestValue >= beta)
    {
        return bestValue;
    }
    if (bestValue > alpha)
    {
        alpha = bestValue;
    }

    MoveList moveList;
    GeneratePseudoLegalMoves(moveList, board);
    SortNoisyMoves(moveList, board);

    int searchedMoves = 0;
    for (int i = 0; i < moveList.count; ++i)
    {
        Move& move = moveList.moves[i];
        if (!isMoveNoisy(move))
            continue;
        int lastEp = board.enpassent;
        uint8_t lastCastle = board.castle;
        bool lastside = board.side;
        int captured_piece = board.mailbox[move.To];
        uint64_t last_zobrist = board.zobristKey;

        MakeMove(board, move);
        data.ply++;

        if (!isLegal(move, board))
        {
            UnmakeMove(board, move, captured_piece);

            board.enpassent = lastEp;
            board.castle = lastCastle;
            board.side = lastside;
            board.zobristKey = last_zobrist;
            data.ply--;
            continue;
        }
        searchedMoves++;
        data.searchNodeCount++;
        data.searchStack[currentPly].move = move;

        score = -QuiescentSearch(board, data, -beta, -alpha);
        UnmakeMove(board, move, captured_piece);
        data.ply--;

        board.enpassent = lastEp;
        board.castle = lastCastle;
        board.side = lastside;
        board.zobristKey = last_zobrist;

        bestValue = std::max(score, bestValue);
        if (bestValue > alpha)
        {
            alpha = score;
        }
        if (alpha >= beta)
        {
            break;
        }
    }

    if (searchedMoves == 0)
    {
        return staticEval;
    }
    return bestValue;
}
inline int AlphaBeta(Board& board, ThreadData& data, int depth, int alpha, int beta)
{
    auto now = std::chrono::steady_clock::now();
    int64_t elapsedMS = std::chrono::duration_cast<std::chrono::milliseconds>(now - data.clockStart).count();
    if (data.stopSearch || elapsedMS > data.SearchTime)
    {
        data.stopSearch = true;
        return 0;
    }

    bool isPvNode = beta - alpha > 1;

    int score = 0;
    int bestValue = -MAXSCORE;

    int ttFlag = HFUPPER;
    bool ttHit = false;
    TranspositionEntry ttEntry = ttLookUp(board.zobristKey);
    if (ttEntry.zobristKey == board.zobristKey && ttEntry.bestMove != Move(0, 0, 0, 0))
    {
        ttHit = true;
    }
    int eval = Evaluate(board);
    int rawEval = Evaluate(board);
    bool isInCheck = is_in_check(board);

    bool canPrune = !isInCheck;
    bool notMated = beta >= -MATESCORE + MAXPLY;
    if (canPrune && notMated) //do whole node pruining
    {
        if (depth <= RFP_MAX_DEPTH) //rfp
        {
            int rfpMargin = RFP_MULTIPLIER * depth;
            if (rawEval - rfpMargin >= beta)
            {
                return rawEval;
            }
        }
    }
    int currentPly = data.ply;
    data.selDepth = std::max(currentPly, data.selDepth);
    data.pvLengths[currentPly] = currentPly;
    if (depth <= 0 || currentPly >= MAXPLY - 1)
    {
        score = QuiescentSearch(board, data, alpha, beta);
        return score;
    }
    MoveList moveList;
    GeneratePseudoLegalMoves(moveList, board);
    SortMoves(moveList, board, data, ttEntry);

    MoveList searchedQuietMoves;
    int searchedMoves = 0;

    Move bestMove;
    for (int i = 0; i < moveList.count; ++i)
    {
        Move& move = moveList.moves[i];

        int lastEp = board.enpassent;
        uint8_t lastCastle = board.castle;
        bool lastside = board.side;
        int captured_piece = board.mailbox[move.To];
        uint64_t last_zobrist = board.zobristKey;

        int childDepth = depth - 1;

        bool isCapture = IsMoveCapture(move);
        MakeMove(board, move);
        data.ply++;

        if (!isLegal(move, board))
        {
            UnmakeMove(board, move, captured_piece);

            board.enpassent = lastEp;
            board.castle = lastCastle;
            board.side = lastside;
            board.zobristKey = last_zobrist;
            data.ply--;
            continue;
        }
        if (!isCapture)
        {
            searchedQuietMoves.add(move);
        }
        searchedMoves++;
        data.searchNodeCount++;
        data.searchStack[currentPly].move = move;

        if (searchedMoves == 1)
        {
            //search with full window
            score = -AlphaBeta(board, data, childDepth, -beta, -alpha);
        }
        else
        {
            //zero window search
            score = -AlphaBeta(board, data, childDepth, -alpha - 1, -alpha);
            if (isPvNode && score > alpha)
            {
                score = -AlphaBeta(board, data, childDepth, -beta, -alpha);
            }
        }
        UnmakeMove(board, move, captured_piece);
        data.ply--;

        board.enpassent = lastEp;
        board.castle = lastCastle;
        board.side = lastside;
        board.zobristKey = last_zobrist;

        bestValue = std::max(score, bestValue);
        if (bestValue > alpha)
        {
            ttFlag = HFEXACT;
            alpha = score;

            bestMove = move;

            if (isPvNode)
            {
                data.pvTable[data.ply][data.ply] = move;
                for (int next_ply = data.ply + 1; next_ply < data.pvLengths[data.ply + 1]; next_ply++)
                {
                    data.pvTable[data.ply][next_ply] = data.pvTable[data.ply + 1][next_ply];
                }
                data.pvLengths[data.ply] = data.pvLengths[data.ply + 1];
            }
        }
        if (alpha >= beta)
        {
            ttFlag = HFLOWER;
            int16_t mainHistBonus = std::min(MAINHIST_BONUS_MAX, MAINHIST_BONUS_BASE + MAINHIST_BONUS_MULT * depth);
            int16_t mainHistMalus = std::min(MAINHIST_MALUS_MAX, MAINHIST_MALUS_BASE + MAINHIST_MALUS_MULT * depth);

            UpdateMainHist(data, board.side, move.From, move.To, mainHistBonus);
            MalusMainHist(data, searchedQuietMoves, move, mainHistMalus);

            break;
        }
    }

    if (searchedMoves == 0)
    {
        if (IsSquareAttacked(
                get_ls1b(board.side == White ? board.bitboards[K] : board.bitboards[k]),
                1 - board.side,
                board,
                board.occupancies[Both]
            ))
        {
            //checkmate
            return -49000 + data.ply;
        }
        else
        {
            //stalemate
            return 0;
        }
    }
    if (ttFlag == HFUPPER && ttHit)
    {
        bestMove = ttEntry.bestMove;
    }
    ttEntry.bestMove = bestMove;
    ttEntry.bound = ttFlag;
    ttEntry.depth = depth;
    ttEntry.zobristKey = board.zobristKey;
    ttStore(ttEntry, board);
    return bestValue;
}
void print_UCI(Move& bestmove, int score, int64_t elapsedMS, float nps, ThreadData& data)
{
    bestmove = data.pvTable[0][0];
    //int hashfull = get_hashfull();
    std::cout << "info depth " << data.currDepth;
    std::cout << " seldepth " << data.selDepth;
    if (std::abs(score) > 40000)
    {
        int mate_ply = 49000 - std::abs(score);
        int mate_fullmove = (int)std::ceil(static_cast<double>(mate_ply) / 2);
        if (score < 0)
        {
            mate_fullmove *= -1;
        }
        std::cout << " score mate " << mate_fullmove;
    }
    else
    {
        std::cout << " score cp " << score;
    }
    int hashfull = get_hashfull();
    std::cout << " time " << static_cast<int>(std::round(elapsedMS)) << " nodes " << data.searchNodeCount << " nps "
              << static_cast<int>(std::round(nps)) << " hashfull " << hashfull << " pv " << std::flush;

    for (int count = 0; count < data.pvLengths[0]; count++)
    {
        printMove(data.pvTable[0][count]);
        std::cout << " ";
    }
    std::cout << "\n" << std::flush;
    ;
}

std::pair<Move, int> IterativeDeepening(
    Board& board,
    int depth,
    SearchLimitations& searchLimits,
    ThreadData& data,
    bool isBench
)
{
    int64_t hardTimeLimit = searchLimits.HardTimeLimit;
    data.SearchTime = hardTimeLimit != NOLIMIT ? hardTimeLimit : std::numeric_limits<int64_t>::max();

    data.searchNodeCount = 0;
    Move bestmove = Move(0, 0, 0, 0);
    data.clockStart = std::chrono::steady_clock::now();

    int score = 0;
    int bestScore = 0;

    memset(data.pvTable, 0, sizeof(data.pvTable));
    memset(data.pvLengths, 0, sizeof(data.pvLengths));

    for (data.currDepth = 1; data.currDepth <= depth; data.currDepth++)
    {
        data.ply = 0;
        data.selDepth = 0;
        data.searchNodeCount = 0;
        data.stopSearch = false;
        for (int i = 0; i < MAXPLY; i++)
        {
            data.searchStack[i].move = Move(0, 0, 0, 0);
        }
        //ensure depth 1 search always finishes
        if (data.currDepth == 1)
        {
            data.SearchTime = std::numeric_limits<int64_t>::max();
        }
        else
        {
            data.SearchTime = hardTimeLimit != NOLIMIT ? hardTimeLimit : std::numeric_limits<int64_t>::max();
        }

        int delta = ASP_WINDOW_INITIAL;
        int adjustedAlpha = std::max(-MAXSCORE, score - delta);
        int adjustedBeta = std::min(MAXSCORE, score + delta);

        //aspiration window
        //start with small window and gradually widen to allow more cutoffs
        while (true)
        {
            auto end = std::chrono::steady_clock::now();
            int64_t MS = static_cast<int64_t>(
                std::chrono::duration_cast<std::chrono::milliseconds>(end - data.clockStart).count()
            );
            if ((searchLimits.HardTimeLimit != NOLIMIT && MS > searchLimits.HardTimeLimit))
            {
                break;
            }

            score = AlphaBeta(board, data, data.currDepth, adjustedAlpha, adjustedBeta);

            delta += delta;
            if (score <= adjustedAlpha)
            {
                //aspiration window failed low, give wider alpha
                adjustedAlpha = std::max(-MAXSCORE, score - delta);
            }
            else if (score >= adjustedBeta)
            {
                //aspiration window failed high, give wider beta
                adjustedBeta = std::min(MAXSCORE, score + delta);
            }
            else
            {
                //score is inside the window, so we can safely use it
                break;
            }

            if (delta >= ASP_WINDOW_MAX)
            {
                //asp window failed multiple times - change to full window
                //(mostly for mate finding)
                delta = MAXSCORE;
            }
        }

        auto end = std::chrono::steady_clock::now();
        int64_t elapsedMS =
            static_cast<int64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(end - data.clockStart).count());
        float second = (float)(elapsedMS + 1) / 1000;

        float nps = data.searchNodeCount / second;

        if (!data.stopSearch)
        {
            bestmove = data.pvTable[0][0];
            bestScore = score;
        }

        if (!data.stopSearch && !isBench)
        {
            print_UCI(bestmove, score, elapsedMS, nps, data);
        }

        if ((searchLimits.HardTimeLimit != NOLIMIT && elapsedMS > searchLimits.HardTimeLimit))
        {
            break;
        }
    }

    std::cout << "bestmove ";
    printMove(bestmove);
    std::cout << "\n" << std::flush;

    return std::pair<Move, int>(bestmove, bestScore);
}
