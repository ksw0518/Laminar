#include "Search.h"
#include "Bit.h"
#include "Board.h"
#include "Const.h"
#include "Evaluation.h"
#include "History.h"
#include "Movegen.h"
#include "NNUE.h"
#include "Ordering.h"
#include "PrettyPrinting.h"
#include "SEE.h"
#include "Transpositions.h"
#include "Tuneables.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstring>
#include <iostream>
#include <limits>
#ifndef EVALFILE
    #define EVALFILE "./nnue.bin"
#endif
#include "Threading.h"

#define NULLMOVE Move(0, 0, 0, 0)

bool IsUCI = false;
int lmrTable[MAXPLY][256];

bool stopSearch = false;
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
bool IsThreefold(std::vector<uint64_t>& history_table, int last_irreversible)
{
    uint64_t lastmove = history_table[history_table.size() - 1];

    int repetition_count = 1;
    for (int i = history_table.size() - 2; i > last_irreversible; i--)
    {
        if (i < 0)
            break;

        if (history_table[i] == lastmove)
        {
            repetition_count++;
        }

        if (repetition_count >= 2)
        {
            return true;
        }
    }
    return false;
}
bool isInsufficientMaterial(const Board& board)
{
    int whiteBishops = count_bits(board.bitboards[B]);
    int blackBishops = count_bits(board.bitboards[b]);
    int whiteKnights = count_bits(board.bitboards[N]);
    int blackKnights = count_bits(board.bitboards[n]);
    int whiteRooks = count_bits(board.bitboards[R]);
    int blackRooks = count_bits(board.bitboards[r]);
    int whiteQueens = count_bits(board.bitboards[Q]);
    int blackQueens = count_bits(board.bitboards[q]);
    int whitePawns = count_bits(board.bitboards[P]);
    int blackPawns = count_bits(board.bitboards[p]);
    if (whiteQueens == 0 && blackQueens == 0 && whiteRooks == 0 && blackRooks == 0 && whitePawns == 0
        && blackPawns == 0)
    {
        if (whiteBishops == 0 && blackBishops == 0 && whiteKnights == 0 && blackKnights == 0)
        {
            return true;
        }
        else if (whiteBishops == 1 && blackBishops == 0 && whiteKnights == 0 && blackKnights == 0)
        {
            return true;
        }
        else if (whiteBishops == 0 && blackBishops == 1 && whiteKnights == 0 && blackKnights == 0)
        {
            return true;
        }
        else if (whiteBishops == 0 && blackBishops == 0 && whiteKnights == 1 && blackKnights == 0)
        {
            return true;
        }
        else if (whiteBishops == 0 && blackBishops == 0 && whiteKnights == 0 && blackKnights == 1)
        {
            return true;
        }
        else if (whiteBishops == 1 && blackBishops == 1 && whiteKnights == 0 && blackKnights == 0)
        {
            return true;
        }
        return false;
    }
    return false;
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
    if (data.stopSearch.load() || elapsedMS > data.SearchTime
        || (data.hardNodeBound != -1 && data.hardNodeBound <= data.searchNodeCount))
    {
        data.stopSearch.store(true);
        return 0;
    }
    bool isPvNode = beta - alpha > 1;

    int rawEval = Evaluate(board);
    int staticEval = AdjustEvalWithCorrHist(board, rawEval, data);
    int currentPly = data.ply;

    data.searchStack[currentPly].staticEval = staticEval;
    data.selDepth = std::max(currentPly, data.selDepth);

    bool ttHit = false;
    TranspositionEntry ttEntry = ttLookUp(board.zobristKey);
    int ttBound = unpackBound(ttEntry.packedInfo);
    if (ttEntry.zobristKey == board.zobristKey && ttBound != HFNONE)
    {
        ttHit = true;
        bool ExactCutoff = (ttBound == HFEXACT);
        bool LowerCutoff = (ttBound == HFLOWER && ttEntry.score >= beta);
        bool UpperCutoff = (ttBound == HFUPPER && ttEntry.score <= alpha);
        bool DoTTCutoff = ExactCutoff || LowerCutoff || UpperCutoff;

        if (DoTTCutoff)
        {
            return ttEntry.score;
        }
    }
    if (currentPly >= MAXPLY - 2)
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

    Move bestMove;
    uint8_t ttFlag = HFUPPER;
    GeneratePseudoLegalMoves(moveList, board);
    SortNoisyMoves(moveList, board, data);

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
        prefetchTT(zobristAfterMove(board, move));
        int lastEp = board.enpassent;
        uint8_t lastCastle = board.castle;
        bool lastside = board.side;
        int captured_piece = board.mailbox[move.To];
        uint64_t last_zobrist = board.zobristKey;
        uint64_t last_pawnKey = board.pawnKey;
        uint64_t last_white_np = board.whiteNonPawnKey;
        uint64_t last_black_np = board.blackNonPawnKey;
        uint64_t last_minor = board.minorKey;
        int last_irreversible = board.lastIrreversiblePly;
        int last_halfmove = board.halfmove;

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
            board.minorKey = last_minor;
            board.lastIrreversiblePly = last_irreversible;
            board.halfmove = last_halfmove;

            board.history.pop_back();

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
        board.lastIrreversiblePly = last_irreversible;
        board.halfmove = last_halfmove;

        board.history.pop_back();

        bestValue = std::max(score, bestValue);
        if (bestValue > alpha)
        {
            alpha = score;
            ttFlag = HFEXACT;
        }
        if (alpha >= beta)
        {
            ttFlag = HFLOWER;
            break;
        }
    }
    ttEntry.score = bestValue;
    ttEntry.bestMove = Move16(bestMove.From, bestMove.To, bestMove.Type);
    ttEntry.zobristKey = board.zobristKey;
    ttEntry.packedInfo = packData(0, ttFlag, false);
    if (ttBound == HFNONE)
    {
        ttStore(ttEntry, board);
    }
    if (searchedMoves == 0)
    {
        return staticEval;
    }

    return bestValue;
}
bool compareMoves(Move move1, Move16 move2)
{
    return (move1.From == move2.from() && move1.To == move2.to() && move1.Type == move2.type());
}

inline int AlphaBeta(
    Board& board,
    ThreadData& data,
    int depth,
    int alpha,
    int beta,
    bool cutnode = false,
    const Move& excludedMove = NULLMOVE
)
{
    data.pvLengths[data.ply] = 0;

    bool isSingularSearch = excludedMove != NULLMOVE;
    auto now = std::chrono::steady_clock::now();
    int64_t elapsedMS = std::chrono::duration_cast<std::chrono::milliseconds>(now - data.clockStart).count();
    if (data.stopSearch.load() || elapsedMS > data.SearchTime
        || (data.hardNodeBound != -1 && data.hardNodeBound <= data.searchNodeCount))
    {
        data.stopSearch.store(true);
        return 0;
    }
    bool isPvNode = beta - alpha > 1;
    int currentPly = data.ply;

    bool root = (currentPly == 0);

    if (!root)
    {
        if (IsThreefold(board.history, board.lastIrreversiblePly))
        {
            return 0;
        }
        if (board.halfmove >= 100)
        {
            return 0;
        }
        if (isInsufficientMaterial(board))
        {
            return 0;
        }
    }

    data.selDepth = std::max(currentPly, data.selDepth);
    //std::cout << "update pv length ply " << currentPly << "\n";

    int score = 0;
    int bestValue = -MAXSCORE;
    bool isInCheck = is_in_check(board);

    if (isInCheck)
    {
        depth++;
    }
    if (depth <= 0 || currentPly >= MAXPLY - 2)
    {
        score = QuiescentSearch(board, data, alpha, beta);
        return score;
    }

    int ttFlag = HFUPPER;
    bool ttHit = false;
    TranspositionEntry ttEntry = ttLookUp(board.zobristKey);

    bool ttPv = isPvNode;

    int ttBound = unpackBound(ttEntry.packedInfo);
    int ttDepth = unpackDepth(ttEntry.packedInfo);
    if (ttEntry.zobristKey == board.zobristKey && ttBound != HFNONE)
    {
        ttHit = true;
        bool ExactCutoff = (ttBound == HFEXACT);
        bool LowerCutoff = (ttBound == HFLOWER && ttEntry.score >= beta);
        bool UpperCutoff = (ttBound == HFUPPER && ttEntry.score <= alpha);
        bool DoTTCutoff = ExactCutoff || LowerCutoff || UpperCutoff;

        if (!isSingularSearch && !isPvNode && !root && ttDepth >= depth && DoTTCutoff)
        {
            return ttEntry.score;
        }
    }
    //Internal Iterative Reduction
    //If no hash move was found, reduce depth
    if (!isSingularSearch && depth >= 4 && (isPvNode || cutnode) && (!ttHit))
    {
        depth--;
    }
    //checks if the node has been in a pv node in the past
    ttPv |= unpackTtPv(ttEntry.packedInfo);

    int rawEval = Evaluate(board);
    int staticEval = AdjustEvalWithCorrHist(board, rawEval, data);
    int ttAdjustedEval = staticEval;

    if (!isSingularSearch && ttHit && !isInCheck
        && (ttBound == HFEXACT || (ttBound == HFLOWER && ttEntry.score >= staticEval)
            || (ttBound == HFUPPER && ttEntry.score <= staticEval)))
    {
        ttAdjustedEval = ttEntry.score;
    }
    data.searchStack[currentPly].staticEval = staticEval;

    bool improving = !isInCheck && currentPly >= 2 && staticEval > data.searchStack[currentPly - 2].staticEval;

    bool canPrune = !isInCheck && !isPvNode && !isSingularSearch;
    bool notMated = beta >= -MATESCORE + MAXPLY;

    if (canPrune && notMated) //do whole node pruining
    {
        //RFP
        if (depth <= RFP_MAX_DEPTH)
        {
            int rfpMargin = (RFP_MULTIPLIER - (improving * 20)) * depth;
            if (ttAdjustedEval - rfpMargin >= beta)
            {
                return (ttAdjustedEval + beta) / 2;
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
            prefetchTT(board.zobristKey ^ side_key);
            MakeNullMove(board);
            int reduction = 3;
            reduction += depth / 3;
            reduction += std::min((ttAdjustedEval - beta) / NMP_EVAL_DIVISER, MAX_NMP_EVAL_R);
            data.minNmpPly = currentPly + 2;

            int score = -AlphaBeta(board, data, depth - reduction, -beta, -beta + 1, !cutnode);
            data.minNmpPly = 0;
            UnmakeNullmove(board);
            board.enpassent = lastEp;
            board.zobristKey = last_zobrist;
            data.ply--;

            if (score >= beta)
            {
                if (depth <= 14 || data.minNmpPly > 0)
                {
                    return score > 49000 ? beta : score;
                }
                data.minNmpPly = currentPly + (depth - reduction) * 3 / 4;
                score = AlphaBeta(board, data, depth - reduction, beta - 1, beta);
                data.minNmpPly = 0;
                if (score >= beta)
                {
                    return score;
                }
            }
        }
    }
    //Calculate all squares opponent is controlling
    uint64_t oppThreats = GetAttackedSquares(1 - board.side, board, board.occupancies[Both]);

    MoveList moveList;
    GeneratePseudoLegalMoves(moveList, board);
    SortMoves(moveList, board, data, ttEntry, oppThreats);

    MoveList searchedQuietMoves;
    MoveList searchedNoisyMoves;
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
        if (move == excludedMove)
        {
            continue;
        }
        bool isQuiet = !IsMoveCapture(move);
        if (skipQuiets && isQuiet)
        {
            continue;
        }
        bool isNotMated = bestValue > -MATESCORE + MAXPLY;

        bool fromThreat = Get_bit(oppThreats, move.From);
        bool toThreat = Get_bit(oppThreats, move.To);
        int mainHistScore = data.histories.mainHist[board.side][move.From][move.To][fromThreat][toThreat];
        int contHistScore = GetContHistScore(move, data);

        int historyScore = mainHistScore + contHistScore;

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
            int historyPruningMargin = HISTORY_PRUNING_BASE - HISTORY_PRUNING_MULTIPLIER * depth;
            if (quietMoves > 1 && depth <= 5 && historyScore < historyPruningMargin)
            {
                continue;
            }
        }
        prefetchTT(zobristAfterMove(board, move));

        int lastEp = board.enpassent;
        uint8_t lastCastle = board.castle;
        bool lastside = board.side;
        int captured_piece = board.mailbox[move.To];
        uint64_t last_zobrist = board.zobristKey;
        uint64_t last_pawnKey = board.pawnKey;
        uint64_t last_white_np = board.whiteNonPawnKey;
        uint64_t last_black_np = board.blackNonPawnKey;
        uint64_t last_minor = board.minorKey;
        uint64_t last_irreversible = board.lastIrreversiblePly;
        uint64_t last_halfmove = board.halfmove;

        bool isCapture = IsMoveCapture(move);

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
            board.minorKey = last_minor;
            board.lastIrreversiblePly = last_irreversible;
            board.halfmove = last_halfmove;
            board.history.pop_back();

            data.ply--;
            continue;
        }
        if (isCapture)
        {
            searchedNoisyMoves.add(move);
        }
        else
        {
            searchedQuietMoves.add(move);
            quietMoves++;
        }
        searchedMoves++;
        data.searchNodeCount++;
        data.searchStack[currentPly].move = move;

        int reduction = 0;
        int extension = 0;

        //Singular Extension
        //If we have a TT move, we try to verify if it's the only good move. if the move is singular, search the move with increased depth
        if (ttHit && !root && depth >= 7 && compareMoves(move, ttEntry.bestMove) && !isSingularSearch
            && ttDepth >= depth - 3 && ttBound != HFUPPER && std::abs(ttEntry.score) < MATESCORE - MAXPLY)
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
            board.minorKey = last_minor;
            board.lastIrreversiblePly = last_irreversible;
            board.history.pop_back();
            board.halfmove = last_halfmove;

            data.ply--;

            int s_beta = ttEntry.score - depth * 2;
            int s_depth = (depth - 1) / 2;
            int s_score = AlphaBeta(board, data, s_depth, s_beta - 1, s_beta, cutnode, move);
            if (s_score < s_beta)
            {
                extension++;
                //Double Extensions
                //TT move is very singular, increase depth by 2
                if (!isPvNode && s_score <= s_beta - DEXT_MARGIN)
                {
                    extension++;
                }
            }
            else if (s_beta >= beta)
            {
                return s_beta;
            }
            refresh_if_cross(move, board);
            MakeMove(board, move);
            data.ply++;
        }
        bool doLmr = depth > MIN_LMR_DEPTH && searchedMoves > 1;
        if (doLmr)
        {
            reduction = lmrTable[depth][searchedMoves];
            if (!isPvNode && quietMoves >= 4)
            {
                reduction++;
            }
            if (isQuiet)
            {
                reduction -= std::clamp(historyScore / 16384, -2, 2);
            }
            if (isQuiet)
            {
                reduction++;
            }
            if (cutnode)
            {
                reduction++;
            }
            if (ttPv)
            {
                reduction--;
            }
        }
        if (reduction < 0)
            reduction = 0;
        bool isReduced = reduction > 0;

        int childDepth = depth + extension - 1;

        uint64_t nodesBeforeSearch = data.searchNodeCount;
        if (doLmr)
        {
            score = -AlphaBeta(board, data, childDepth - reduction, -alpha - 1, -alpha, true);
            if (score > alpha && isReduced)
            {
                score = -AlphaBeta(board, data, childDepth, -alpha - 1, -alpha, !cutnode);
            }
        }
        else if (!isPvNode || searchedMoves > 1)
        {
            score = -AlphaBeta(board, data, childDepth, -alpha - 1, -alpha, !cutnode);
        }
        if (isPvNode && (searchedMoves == 1 || score > alpha))
        {
            score = -AlphaBeta(board, data, childDepth, -beta, -alpha, false);
        }
        uint64_t nodesAfterSearch = data.searchNodeCount;
        uint64_t nodesSpent = nodesAfterSearch - nodesBeforeSearch;
        if (root)
        {
            data.nodesPerMove[move.From][move.To] += nodesSpent;
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
        board.minorKey = last_minor;
        board.lastIrreversiblePly = last_irreversible;
        board.halfmove = last_halfmove;

        board.history.pop_back();

        bestValue = std::max(score, bestValue);
        if (bestValue > alpha)
        {
            ttFlag = HFEXACT;
            alpha = score;

            bestMove = move;
            if (isPvNode)
            {
                // first move at this ply = column 0
                data.pvTable[data.ply][0] = move;

                // copy child PV
                int k = 0;
                while (k < data.pvLengths[data.ply + 1])
                {
                    data.pvTable[data.ply][k + 1] = data.pvTable[data.ply + 1][k];
                    k++;
                }

                // set PV length
                data.pvLengths[data.ply] = data.pvLengths[data.ply + 1] + 1;
            }
        }
        if (alpha >= beta)
        {
            ttFlag = HFLOWER;

            if (isQuiet)
            {
                int16_t mainHistBonus = std::min(MAINHIST_BONUS_MAX, MAINHIST_BONUS_BASE + MAINHIST_BONUS_MULT * depth);
                int16_t mainHistMalus = std::min(MAINHIST_MALUS_MAX, MAINHIST_MALUS_BASE + MAINHIST_MALUS_MULT * depth);

                UpdateMainHist(data, board.side, move.From, move.To, mainHistBonus, oppThreats);
                MalusMainHist(data, searchedQuietMoves, move, mainHistMalus, oppThreats);

                int16_t contHistBonus = std::min(CONTHIST_BONUS_MAX, CONTHIST_BONUS_BASE + CONTHIST_BONUS_MULT * depth);
                int16_t contHistMalus = std::min(CONTHIST_MALUS_MAX, CONTHIST_MALUS_BASE + CONTHIST_MALUS_MULT * depth);
                UpdateContHist(move, contHistBonus, data);
                MalusContHist(data, searchedQuietMoves, move, contHistMalus);

                int16_t captHistMalus = std::min(CAPTHIST_MALUS_MAX, CAPTHIST_MALUS_BASE + CAPTHIST_MALUS_MULT * depth);
                MalusCaptHist(data, searchedNoisyMoves, move, captHistMalus, board);
            }
            else
            {
                int16_t captHistBonus = std::min(CAPTHIST_BONUS_MAX, CAPTHIST_BONUS_BASE + CAPTHIST_BONUS_MULT * depth);
                int16_t captHistMalus = std::min(CAPTHIST_MALUS_MAX, CAPTHIST_MALUS_BASE + CAPTHIST_MALUS_MULT * depth);

                UpdateCaptHist(data, move.Piece, move.To, board.mailbox[move.To], captHistBonus);
                MalusCaptHist(data, searchedNoisyMoves, move, captHistMalus, board);
            }

            break;
        }
    }
    if (searchedMoves == 0)
    {
        if (isSingularSearch)
        {
            //checkmate
            return alpha;
        }
        else
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
    }
    if (ttFlag == HFUPPER && ttHit)
    {
        bestMove.From = ttEntry.bestMove.from();
        bestMove.To = ttEntry.bestMove.to();
        bestMove.Type = ttEntry.bestMove.type();
    }
    if (!isSingularSearch && !isInCheck && (bestMove == Move(0, 0, 0, 0) || IsMoveQuiet(bestMove))
        && !(ttFlag == HFLOWER && bestValue <= staticEval) && !(ttFlag == HFUPPER && bestValue >= staticEval))
    {
        UpdateCorrhists(board, depth, bestValue - staticEval, data);
    }

    ttEntry.bestMove = Move16(bestMove.From, bestMove.To, bestMove.Type);
    ttEntry.zobristKey = board.zobristKey;
    ttEntry.score = bestValue;
    ttEntry.packedInfo = packData(depth, ttFlag, ttPv);
    if (!isSingularSearch && !data.stopSearch.load())
    {
        ttStore(ttEntry, board);
    }

    return bestValue;
}
void print_UCI(Move& bestmove, int score, int64_t elapsedMS, float nps, ThreadData& data, uint64_t nodes)
{
    bestmove = data.pvTable[0][0];
    //int hashfull = get_hashfull();
    std::cout << "info depth " << data.currDepth;
    std::cout << " seldepth " << data.selDepth;
    if (std::abs(score) > MATESCORE - MAXPLY)
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
    std::cout << " time " << static_cast<int>(std::round(elapsedMS)) << " nodes " << nodes << " nps "
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
    SearchLimitations searchLimits,
    ThreadData& data,
    bool isBench
)
{
    int64_t hardTimeLimit = searchLimits.HardTimeLimit;
    data.SearchTime = hardTimeLimit != NOLIMIT ? hardTimeLimit : std::numeric_limits<int64_t>::max();

    data.searchNodeCount = 0;
    data.hardNodeBound = searchLimits.HardNodeLimit;
    Move bestmove = Move(0, 0, 0, 0);
    data.clockStart = std::chrono::steady_clock::now();

    int score = 0;
    int bestScore = 0;

    memset(data.pvTable, 0, sizeof(data.pvTable));
    memset(data.pvLengths, 0, sizeof(data.pvLengths));

    if (!data.isMainThread)
    {
        data.SearchTime = std::numeric_limits<int64_t>::max();
        searchLimits.SoftTimeLimit = NOLIMIT;
        searchLimits.HardTimeLimit = NOLIMIT;
    }
    bool mainThread = data.isMainThread;
    double nodesTmScale = 1.0;

    for (data.currDepth = 1; data.currDepth <= depth; data.currDepth++)
    {
        memset(data.pvTable, 0, sizeof(data.pvTable));
        memset(data.pvLengths, 0, sizeof(data.pvLengths));
        memset(data.nodesPerMove, 0, sizeof(data.nodesPerMove));
        data.ply = 0;
        data.selDepth = 0;
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
        //std::cout << data.stopSearch.load(std::memory_order_relaxed);
        int delta = ASP_WINDOW_INITIAL;
        int adjustedAlpha = std::max(-MAXSCORE, score - delta);
        int adjustedBeta = std::min(MAXSCORE, score + delta);
        int aspWindowDepth = data.currDepth;

        //aspiration window
        //start with small window and gradually widen to allow more cutoffs
        while (true)
        {
            auto end = std::chrono::steady_clock::now();
            int64_t MS = static_cast<int64_t>(
                std::chrono::duration_cast<std::chrono::milliseconds>(end - data.clockStart).count()
            );
            if ((searchLimits.HardTimeLimit != NOLIMIT && MS > searchLimits.HardTimeLimit) || data.stopSearch.load())
            {
                if (mainThread)
                {
                    for (auto ptr : allThreadDataPtrs)
                    {
                        ptr->stopSearch.store(true);
                    }
                }
                break;
            }

            score = AlphaBeta(board, data, std::max(aspWindowDepth, 1), adjustedAlpha, adjustedBeta);

            delta += delta;
            if (score <= adjustedAlpha)
            {
                //aspiration window failed low, give wider alpha
                adjustedAlpha = std::max(-MAXSCORE, score - delta);
                aspWindowDepth = data.currDepth;
            }
            else if (score >= adjustedBeta)
            {
                //aspiration window failed high, give wider beta
                adjustedBeta = std::min(MAXSCORE, score + delta);
                aspWindowDepth = std::max(aspWindowDepth - 1, data.currDepth - 5);
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

        if (data.currDepth >= 6 && searchLimits.SoftTimeLimit != NOLIMIT && searchLimits.HardTimeLimit != NOLIMIT)
        {
            nodesTmScale = (1.5 - ((double)data.nodesPerMove[bestmove.From][bestmove.To] / data.searchNodeCount)) * 1;
        }

        if (!data.stopSearch.load())
        {
            bestmove = data.pvTable[0][0];
            bestScore = score;
        }

        if (!data.stopSearch.load() && !isBench)
        {
            if (data.isMainThread)
            {
                float nps = data.searchNodeCount / second;
                int64_t combinedNodeCount = 0;
                for (auto ptr : allThreadDataPtrs)
                {
                    combinedNodeCount += ptr->searchNodeCount;
                }
                float combinedNps = combinedNodeCount / second;

                if (IsUCI)
                {
                    print_UCI(bestmove, score, elapsedMS, combinedNps, data, combinedNodeCount);
                }
                else
                {
                    printPretty(score, elapsedMS, combinedNps, data, combinedNodeCount);
                }
            }
        }

        if ((searchLimits.HardTimeLimit != NOLIMIT && elapsedMS > searchLimits.HardTimeLimit) || data.stopSearch.load()
            || (searchLimits.HardNodeLimit != NOLIMIT && data.searchNodeCount > searchLimits.HardNodeLimit))
        {
            if (mainThread)
            {
                for (auto ptr : allThreadDataPtrs)
                {
                    ptr->stopSearch.store(true);
                }
            }
            break;
        }
        if ((searchLimits.SoftTimeLimit != NOLIMIT && elapsedMS > (double)searchLimits.SoftTimeLimit * nodesTmScale)
            || data.stopSearch.load())
        {
            if (mainThread)
            {
                for (auto ptr : allThreadDataPtrs)
                {
                    ptr->stopSearch.store(true);
                }
            }
            break;
        }
    }
    if (data.isMainThread)
    {
        std::cout << "bestmove ";
        printMove(bestmove);
        std::cout << "\n" << std::flush;
    }

    return std::pair<Move, int>(bestmove, bestScore);
}
