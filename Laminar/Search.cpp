#include "Search.h"
#include "Bit.h"
#include "Board.h"
#include "Const.h"
#include "Evaluation.h"
#include "History.h"
#include "Movegen.h"
#include "NNUE.h"
#include "Ordering.h"
#include "SEE.h"
#include "Transpositions.h"
#include "Tuneables.h"
#include <chrono>
#include <cmath>
#include <cstring>
#include <iostream>
#include <limits>
#ifndef EVALFILE
    #define EVALFILE "./nnue.bin"
#endif

int lmrTable[MAXPLY][256];
void InitializeLMRTable()
{
    for (int depth = 1; depth < MAXPLY; depth++)
    {
        for (int move = 1; move < 256; move++)
        {
            lmrTable[depth][move] =
                std::floor((float)LMR_OFFSET / (float)100 + log(move) * log(depth) / ((float)LMR_DIVISOR / (float)100));
        }
    }
}
void InitializeSearch(ThreadData& data)
{
    memset(&data.histories, 0, sizeof(data.histories));
}
void InitNNUE()
{
    LoadNetwork(EVALFILE);
}
void refresh_if_cross(Move& move, Board& board)
{
    if (get_piece(move.Piece, White) == K) //king has moved
    {
        if (getFile(move.From) <= 3) //king was left before
        {
            if (getFile(move.To) >= 4) //king moved to right
            {
                //fully refresh the stm accumulator, and change that to start mirroring
                if (board.side == White)
                {
                    resetWhiteAccumulator(board, board.accumulator, true);
                }
                else
                {
                    resetBlackAccumulator(board, board.accumulator, true);
                }
            }
        }
        else //king was right before
        {
            if (getFile(move.To) <= 3) //king moved to left
            {
                //fully refresh the stm accumulator, and change that to stop mirroring
                if (board.side == White)
                {
                    resetWhiteAccumulator(board, board.accumulator, false);
                }
                else
                {
                    resetBlackAccumulator(board, board.accumulator, false);
                }
            }
        }
    }
}
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

    int rawEval = Evaluate(board);
    int staticEval = AdjustEvalWithCorrHist(board, rawEval, data);
    int currentPly = data.ply;
    data.selDepth = std::max(currentPly, data.selDepth);

    bool ttHit = false;
    TranspositionEntry ttEntry = ttLookUp(board.zobristKey);
    if (ttEntry.zobristKey == board.zobristKey && ttEntry.bound != HFNONE)
    {
        ttHit = true;
        bool ExactCutoff = (ttEntry.bound == HFEXACT);
        bool LowerCutoff = (ttEntry.bound == HFLOWER && ttEntry.score >= beta);
        bool UpperCutoff = (ttEntry.bound == HFUPPER && ttEntry.score <= alpha);
        bool DoTTCutoff = ExactCutoff || LowerCutoff || UpperCutoff;

        if (DoTTCutoff)
        {
            return ttEntry.score;
        }
    }
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

    AccumulatorPair last_accumulator = board.accumulator;
    for (int i = 0; i < moveList.count; ++i)
    {
        Move& move = moveList.moves[i];
        if (!IsMoveNoisy(move))
            continue;

        if (!SEE(board, move, QS_SEE_MARGIN))
        {
            continue;
        }
        int lastEp = board.enpassent;
        uint8_t lastCastle = board.castle;
        bool lastside = board.side;
        int captured_piece = board.mailbox[move.To];
        uint64_t last_zobrist = board.zobristKey;
        uint64_t last_pawnKey = board.pawnKey;
        uint64_t last_white_np = board.whiteNonPawnKey;
        uint64_t last_black_np = board.blackNonPawnKey;

        refresh_if_cross(move, board);
        MakeMove(board, move);

        data.ply++;

        if (!isLegal(move, board))
        {
            UnmakeMove(board, move, captured_piece);

            board.enpassent = lastEp;
            board.castle = lastCastle;
            board.side = lastside;
            board.zobristKey = last_zobrist;
            board.accumulator = last_accumulator;
            board.pawnKey = last_pawnKey;
            board.whiteNonPawnKey = last_white_np;
            board.blackNonPawnKey = last_black_np;

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
        board.accumulator = last_accumulator;
        board.pawnKey = last_pawnKey;
        board.whiteNonPawnKey = last_white_np;
        board.blackNonPawnKey = last_black_np;

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
    int currentPly = data.ply;
    bool root = (currentPly == 0);
    data.selDepth = std::max(currentPly, data.selDepth);
    data.pvLengths[currentPly] = currentPly;

    int score = 0;
    int bestValue = -MAXSCORE;
    bool isInCheck = is_in_check(board);

    if (isInCheck)
    {
        depth++;
    }
    if (depth <= 0 || currentPly >= MAXPLY - 1)
    {
        score = QuiescentSearch(board, data, alpha, beta);
        return score;
    }

    int ttFlag = HFUPPER;
    bool ttHit = false;
    TranspositionEntry ttEntry = ttLookUp(board.zobristKey);
    if (ttEntry.zobristKey == board.zobristKey && ttEntry.bound != HFNONE)
    {
        ttHit = true;
        bool ExactCutoff = (ttEntry.bound == HFEXACT);
        bool LowerCutoff = (ttEntry.bound == HFLOWER && ttEntry.score >= beta);
        bool UpperCutoff = (ttEntry.bound == HFUPPER && ttEntry.score <= alpha);
        bool DoTTCutoff = ExactCutoff || LowerCutoff || UpperCutoff;

        if (!isPvNode && !root && ttEntry.depth >= depth && DoTTCutoff)
        {
            return ttEntry.score;
        }
    }

    int rawEval = Evaluate(board);
    int staticEval = AdjustEvalWithCorrHist(board, rawEval, data);
    int ttAdjustedEval = staticEval;
    uint8_t Bound = ttEntry.bound;

    if (ttHit && !isInCheck
        && (Bound == HFEXACT || (Bound == HFLOWER && ttEntry.score >= staticEval)
            || (Bound == HFUPPER && ttEntry.score <= staticEval)))
    {
        ttAdjustedEval = ttEntry.score;
    }

    bool canPrune = !isInCheck;
    bool notMated = beta >= -MATESCORE + MAXPLY;

    if (canPrune && notMated) //do whole node pruining
    {
        //RFP
        if (depth <= RFP_MAX_DEPTH)
        {
            int rfpMargin = RFP_MULTIPLIER * depth;
            if (ttAdjustedEval - rfpMargin >= beta)
            {
                return ttAdjustedEval;
            }
        }

        //NMP
        //The null move skips our turn without making move,
        //which allows opponent to make two moves in a row
        //if a null move returns a score>= beta, we assume the current position is too strong
        //so prune the rest of the moves
        if (!isPvNode && depth >= 2 && !root && ttAdjustedEval >= beta && currentPly >= data.minNmpPly
            && !IsOnlyKingPawn(board))
        {
            int lastEp = board.enpassent;
            uint64_t last_zobrist = board.zobristKey;

            data.ply++;
            MakeNullMove(board);
            int reduction = 3;
            reduction += depth / 3;
            reduction += std::min((ttAdjustedEval - beta) / NMP_EVAL_DIVISER, MAX_NMP_EVAL_R);
            data.minNmpPly = currentPly + 2;
            int score = -AlphaBeta(board, data, depth - reduction, -beta, -beta + 1);
            data.minNmpPly = 0;
            UnmakeNullmove(board);
            board.enpassent = lastEp;
            board.zobristKey = last_zobrist;
            data.ply--;

            if (score >= beta)
            {
                return score > 49000 ? beta : score;
            }
        }
    }
    //Calculate all squares opponent is controlling
    uint64_t oppThreats = GetAttackedSquares(1 - board.side, board, board.occupancies[Both]);

    MoveList moveList;
    GeneratePseudoLegalMoves(moveList, board);
    SortMoves(moveList, board, data, ttEntry, oppThreats);

    MoveList searchedQuietMoves;
    int searchedMoves = 0;
    int quietMoves = 0;

    Move bestMove = Move(0, 0, 0, 0);

    int quietSEEMargin = PVS_QUIET_BASE - PVS_QUIET_MULTIPLIER * depth;
    int noisySEEMargin = PVS_NOISY_BASE - PVS_NOISY_MULTIPLIER * depth * depth;
    int lmpThreshold = (LMP_BASE + (LMP_MULTIPLIER)*depth * depth) / 100;

    bool skipQuiets = false;
    AccumulatorPair last_accumulator = board.accumulator;
    for (int i = 0; i < moveList.count; ++i)
    {
        Move& move = moveList.moves[i];
        bool isQuiet = !IsMoveCapture(move);
        if (skipQuiets && isQuiet)
        {
            continue;
        }
        bool isNotMated = bestValue > -49000 + 99;

        if (isNotMated && searchedMoves >= 1 && !root) //do moveloop pruning
        {
            int seeThreshold = isQuiet ? quietSEEMargin : noisySEEMargin;
            //if the Static Exchange Evaluation score is lower than certain margin, assume the move is very bad and skip the move
            if (!SEE(board, move, seeThreshold))
            {
                continue;
            }
            if (searchedMoves >= lmpThreshold)
            {
                skipQuiets = true;
                continue;
            }
        }

        int lastEp = board.enpassent;
        uint8_t lastCastle = board.castle;
        bool lastside = board.side;
        int captured_piece = board.mailbox[move.To];
        uint64_t last_zobrist = board.zobristKey;
        uint64_t last_pawnKey = board.pawnKey;
        uint64_t last_white_np = board.whiteNonPawnKey;
        uint64_t last_black_np = board.blackNonPawnKey;

        int childDepth = depth - 1;

        bool isCapture = IsMoveCapture(move);
        refresh_if_cross(move, board);
        MakeMove(board, move);
        bool givingCheck = is_in_check(board);

        data.ply++;

        if (!isLegal(move, board))
        {
            UnmakeMove(board, move, captured_piece);

            board.enpassent = lastEp;
            board.castle = lastCastle;
            board.side = lastside;
            board.zobristKey = last_zobrist;
            board.accumulator = last_accumulator;
            board.pawnKey = last_pawnKey;
            board.whiteNonPawnKey = last_white_np;
            board.blackNonPawnKey = last_black_np;
            data.ply--;
            continue;
        }
        if (!isCapture)
        {
            searchedQuietMoves.add(move);
            quietMoves++;
        }
        searchedMoves++;
        data.searchNodeCount++;
        data.searchStack[currentPly].move = move;

        int reduction = 0;

        bool doLmr = depth > MIN_LMR_DEPTH && searchedMoves > 1;
        if (doLmr)
        {
            reduction = lmrTable[depth][searchedMoves];
            if (!isPvNode && quietMoves >= 4)
            {
                reduction++;
            }
            if (givingCheck)
            {
                reduction--;
            }
        }
        if (reduction < 0)
            reduction = 0;
        bool isReduced = reduction > 0;

        if (doLmr)
        {
            score = -AlphaBeta(board, data, childDepth - reduction, -alpha - 1, -alpha);
            if (score > alpha && isReduced)
            {
                score = -AlphaBeta(board, data, childDepth, -alpha - 1, -alpha);
            }
        }
        else if (!isPvNode || searchedMoves > 1)
        {
            score = -AlphaBeta(board, data, childDepth, -alpha - 1, -alpha);
        }
        if (isPvNode && (searchedMoves == 1 || score > alpha))
        {
            score = -AlphaBeta(board, data, childDepth, -beta, -alpha);
        }
        UnmakeMove(board, move, captured_piece);
        data.ply--;

        board.enpassent = lastEp;
        board.castle = lastCastle;
        board.side = lastside;
        board.zobristKey = last_zobrist;
        board.accumulator = last_accumulator;
        board.pawnKey = last_pawnKey;
        board.whiteNonPawnKey = last_white_np;
        board.blackNonPawnKey = last_black_np;

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

            UpdateMainHist(data, board.side, move.From, move.To, mainHistBonus, oppThreats);
            MalusMainHist(data, searchedQuietMoves, move, mainHistMalus, oppThreats);

            int16_t contHistBonus = std::min(CONTHIST_BONUS_MAX, CONTHIST_BONUS_BASE + CONTHIST_BONUS_MULT * depth);
            int16_t contHistMalus = std::min(CONTHIST_MALUS_MAX, CONTHIST_MALUS_BASE + CONTHIST_MALUS_MULT * depth);
            UpdateContHist(move, contHistBonus, data);
            MalusContHist(data, searchedQuietMoves, move, contHistMalus);

            break;
        }
    }
    if (searchedMoves == 0)
    {
        if (isInCheck)
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
    if (!isInCheck && (bestMove == Move(0, 0, 0, 0) || IsMoveQuiet(bestMove))
        && !(ttFlag == HFLOWER && bestValue <= staticEval) && !(ttFlag == HFUPPER && bestValue >= staticEval))
    {
        UpdateCorrhists(board, depth, bestValue - staticEval, data);
    }
    ttEntry.bestMove = bestMove;
    ttEntry.bound = ttFlag;
    ttEntry.depth = depth;
    ttEntry.zobristKey = board.zobristKey;
    ttEntry.score = bestValue;
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
        if ((searchLimits.SoftTimeLimit != NOLIMIT && elapsedMS > searchLimits.SoftTimeLimit))
        {
            break;
        }
    }

    std::cout << "bestmove ";
    printMove(bestmove);
    std::cout << "\n" << std::flush;

    return std::pair<Move, int>(bestmove, bestScore);
}
