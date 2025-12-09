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
    memset(&data.killerMoves, 0, sizeof(data.killerMoves));
    memset(&data.searchStack, 0, sizeof(data.searchStack));
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

inline void refresh_if_cross(Move& move, Board& board)
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
inline void SaveCopyMakeInfo(Board& board, Move& move, CopyMake& info)
{
    info.lastEp = board.enpassent;
    info.lastCastle = board.castle;
    info.lastside = board.side;
    info.captured_piece = board.mailbox[move.To];
    info.last_pawnKey = board.pawnKey;
    info.last_white_np = board.whiteNonPawnKey;
    info.last_black_np = board.blackNonPawnKey;
    info.last_minor = board.minorKey;
    info.last_irreversible = board.lastIrreversiblePly;
    info.last_halfmove = board.halfmove;
    info.last_zobrist = board.zobristKey;
}
inline void ApplyCopyMake(Board& board, CopyMake& info, ThreadData& data, int ply)
{
    board.enpassent = info.lastEp;
    board.castle = info.lastCastle;
    board.side = info.lastside;
    board.zobristKey = info.last_zobrist;
    board.accumulator = data.searchStack[ply].last_accumulator;
    board.pawnKey = info.last_pawnKey;
    board.whiteNonPawnKey = info.last_white_np;
    board.blackNonPawnKey = info.last_black_np;
    board.minorKey = info.last_minor;
    board.lastIrreversiblePly = info.last_irreversible;
    board.halfmove = info.last_halfmove;
}
inline int QuiescentSearch(Board& board, ThreadData& data, int alpha, int beta)
{
    //only search "noisy" moves (captures, promos) to only evaluate quiet positions
    if (data.stopSearch.load())
    {
        return 0;
    }
    if (data.ply != 0 && data.searchNodeCount % 1024 == 0)
    {
        auto now = std::chrono::steady_clock::now();
        int64_t elapsedMS = std::chrono::duration_cast<std::chrono::milliseconds>(now - data.clockStart).count();
        if (elapsedMS > data.SearchTime || (data.hardNodeBound != -1 && data.hardNodeBound <= data.searchNodeCount))
        {
            data.stopSearch.store(true);
            return 0;
        }
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
    //if static eval is higher than beta, we can assume fail high
    if (bestValue >= beta)
    {
        return bestValue;
    }
    //raise alpha if static eval is higher than alpha
    //since the static eval is a good lower bound for a node
    if (bestValue > alpha)
    {
        alpha = bestValue;
    }

    MoveList moveList;

    Move bestMove;
    uint8_t ttFlag = HFUPPER;
    GeneratePseudoLegalMoves(moveList, board, true);
    SortNoisyMoves(moveList, board, data);

    int searchedMoves = 0;

    data.searchStack[currentPly].last_accumulator = board.accumulator;

    CopyMake undoInfo{};
    int futilityValue = bestValue + 100;

    for (int i = 0; i < moveList.count; ++i)
    {
        Move& move = moveList.moves[i];

        if (!IsMoveNoisy(move))
            continue;

        if (IsMoveCapture(move) && futilityValue <= alpha && !SEE(board, move, 1))
        {
            bestValue = std::max(bestValue, futilityValue);
            continue;
        }
        //skip moves that have bad static exchange evaluation score,
        //since they are likely bad
        if (!SEE(board, move, QS_SEE_MARGIN))
        {
            continue;
        }

        prefetchTT(zobristAfterMove(board, move));
        SaveCopyMakeInfo(board, move, undoInfo);
        refresh_if_cross(move, board);
        MakeMove(board, move);

        data.ply++;

        if (!isLegal(move, board))
        {
            UnmakeMove(board, move, undoInfo.captured_piece);
            ApplyCopyMake(board, undoInfo, data, currentPly);
            board.history.pop_back();
            data.ply--;

            continue;
        }
        searchedMoves++;
        data.searchNodeCount++;
        data.searchStack[currentPly].move = move;

        score = -QuiescentSearch(board, data, -beta, -alpha);

        UnmakeMove(board, move, undoInfo.captured_piece);
        ApplyCopyMake(board, undoInfo, data, currentPly);
        board.history.pop_back();
        data.ply--;

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

    //store TT with the depth of 0
    //to use later for qs search or tt adjusted eval
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
    if (data.stopSearch.load())
    {
        return 0;
    }
    if (data.ply != 0 && data.searchNodeCount % 1024 == 0)
    {
        auto now = std::chrono::steady_clock::now();
        int64_t elapsedMS = std::chrono::duration_cast<std::chrono::milliseconds>(now - data.clockStart).count();
        if (elapsedMS > data.SearchTime || (data.hardNodeBound != -1 && data.hardNodeBound <= data.searchNodeCount))
        {
            data.stopSearch.store(true);
            return 0;
        }
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

    //increase depth if we're in check
    if (isInCheck)
    {
        depth++;
    }
    if (depth <= 0 || currentPly >= MAXPLY - 2)
    {
        //return quiescence search score at the end of the tree
        //to avoid horizon effect
        score = QuiescentSearch(board, data, alpha, beta);
        return score;
    }
    if (currentPly >= MAXPLY - 2)
    {
        //Reset killer moves for the next ply to make the killer move more local
        data.killerMoves[currentPly + 1] = Move(0, 0, 0, 0);
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

    //if we have a transposition entry for current position, change static eval
    //based on the tt score since it'll be more accurate
    if (!isSingularSearch && ttHit && !isInCheck
        && (ttBound == HFEXACT || (ttBound == HFLOWER && ttEntry.score >= staticEval)
            || (ttBound == HFUPPER && ttEntry.score <= staticEval)))
    {
        ttAdjustedEval = ttEntry.score;
    }
    data.searchStack[currentPly].staticEval = staticEval;
    data.searchStack[currentPly].check = isInCheck;

    //is the current position better than the position of 2 plies before?
    bool improving = !isInCheck && currentPly >= 2 && staticEval > data.searchStack[currentPly - 2].staticEval;

    bool canPrune = !isInCheck && !isPvNode && !isSingularSearch;
    bool notMated = beta >= -MATESCORE + MAXPLY;

    if (canPrune && notMated)
    {
        //hindsight extension
        if (!root && data.searchStack[currentPly - 1].reduction >= 3 && !data.searchStack[currentPly - 1].check
            && staticEval + data.searchStack[currentPly - 1].staticEval < 0)
        {
            depth++;
        }

        //do whole node pruining

        //Reverse futility pruning
        //if static eval is higher than beta with some margin, assume it'll fail high
        if (depth <= RFP_MAX_DEPTH)
        {
            int rfpMargin = (RFP_MULTIPLIER - (improving * RFP_IMPROVING_SUB)) * depth + RFP_BASE;
            if (ttAdjustedEval - rfpMargin >= beta)
            {
                return (ttAdjustedEval + beta) / 2;
            }
        }
        if (depth <= 3 && ttAdjustedEval + RAZORING_MULTIPLIER * depth + RAZORING_BASE <= alpha)
        {
            int razor_score = QuiescentSearch(board, data, alpha, alpha + 1);
            if (razor_score <= alpha)
            {
                return razor_score;
            }
        }
        //Null move pruning
        //The null move skips our turn without making move,
        //which allows opponent to make two moves in a row
        //if a null move returns a score>= beta, we assume the current position is too strong
        //so prune the rest of the moves
        //disable nmp in KP endgame, because of zugzwang
        if (!isPvNode && depth >= 2 && !root && ttAdjustedEval >= beta + NMP_BETA_OFFSET && currentPly >= data.minNmpPly
            && !IsOnlyKingPawn(board))
        {
            int lastEp = board.enpassent;
            uint64_t last_zobrist = board.zobristKey;

            data.ply++;
            prefetchTT(board.zobristKey ^ side_key);
            MakeNullMove(board);
            int reduction = 3;
            reduction += depth / 3;
            reduction += std::min((ttAdjustedEval - beta) / NMP_EVAL_DIVISOR, MAX_NMP_EVAL_R);
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

    ScoredMove scored[256];

    //order moves from best to worse for more cutoff
    ScoreMoves(scored, moveList, board, data, ttEntry, oppThreats);

    MoveList searchedQuietMoves;
    MoveList searchedNoisyMoves;

    int searchedMoves = 0;
    int quietMoves = 0;

    Move bestMove = Move(0, 0, 0, 0);

    int quietSEEMargin = PVS_QUIET_BASE - PVS_QUIET_MULT * depth;
    int noisySEEMargin = PVS_NOISY_BASE - PVS_NOISY_MULT * depth * depth;
    int lmpThreshold = (LMP_BASE + (LMP_MULTIPLIER)*depth * depth) / 100;

    bool skipQuiets = false;
    data.searchStack[currentPly].last_accumulator = board.accumulator;

    int materialValue = material_eval(board);

    CopyMake undoInfo{};
    for (int i = 0; i < moveList.count; ++i)
    {
        ChooseNextMove(scored, moveList, i);
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
            //Late move pruning
            //skip moves that are "late"(moves later in the move list)
            //because good moves are usually in the front
            if (searchedMoves >= lmpThreshold)
            {
                skipQuiets = true;
                continue;
            }

            //History pruning
            //skip moves that have bad history score
            int historyPruningMargin = HISTORY_PRUNING_BASE - HISTORY_PRUNING_MULTIPLIER * depth;
            if (quietMoves > 1 && depth <= 5 && historyScore < historyPruningMargin)
            {
                continue;
            }
            int seeThreshold = isQuiet ? quietSEEMargin : noisySEEMargin;
            if (isQuiet)
            {
                seeThreshold -= historyScore / PVS_SEE_HISTORY_DIV;
            }
            //if the Static Exchange Evaluation score is lower than certain margin,
            //assume the move is very bad and skip the move
            if (!SEE(board, move, seeThreshold))
            {
                continue;
            }
        }
        bool isCapture = IsMoveCapture(move);

        prefetchTT(zobristAfterMove(board, move));
        SaveCopyMakeInfo(board, move, undoInfo);
        refresh_if_cross(move, board);
        MakeMove(board, move);

        data.ply++;

        if (!isLegal(move, board))
        {
            UnmakeMove(board, move, undoInfo.captured_piece);
            ApplyCopyMake(board, undoInfo, data, currentPly);
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
        //If we have a TT move, we try to verify if it's the only good move.
        //if the move is singular, search the move with increased depth
        if (ttHit && !root && depth >= 7 && compareMoves(move, ttEntry.bestMove) && !isSingularSearch
            && ttDepth >= depth - 3 && ttBound != HFUPPER && std::abs(ttEntry.score) < MATESCORE - MAXPLY)
        {
            UnmakeMove(board, move, undoInfo.captured_piece);
            ApplyCopyMake(board, undoInfo, data, currentPly);
            board.history.pop_back();
            data.ply--;

            int s_beta = ttEntry.score - depth * 2;
            int s_depth = (depth - 1) / 2;
            int s_score = AlphaBeta(board, data, s_depth, s_beta - 1, s_beta, cutnode, move);
            if (s_score < s_beta - 20)
            {
                if (!(ttEntry.bestMove.type() & captureFlag)) //quiets
                {
                    int16_t mainHistBonus =
                        std::min((int)SE_MAINHIST_BONUS_MAX, SE_MAINHIST_BONUS_BASE + SE_MAINHIST_BONUS_MULT * depth);
                    UpdateMainHist(data, board.side, move.From, move.To, mainHistBonus, oppThreats);
                }
                else
                {
                    int16_t captHistBonus =
                        std::min((int)SE_CAPTHIST_BONUS_MAX, SE_CAPTHIST_BONUS_BASE + SE_CAPTHIST_BONUS_MULT * depth);
                    UpdateCaptHist(data, move.Piece, move.To, board.mailbox[move.To], captHistBonus);
                }
            }
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
            //Multicut
            //excluded search failed high,
            //we can assume current node will also fail high
            else if (s_beta >= beta)
            {
                return s_beta;
            }
            else if (cutnode)
            {
                extension = -2;
            }
            else if (ttEntry.score >= beta)
            {
                extension = -1;
            }
            refresh_if_cross(move, board);
            MakeMove(board, move);
            data.ply++;
        }
        bool doLmr = depth > MIN_LMR_DEPTH && searchedMoves > 1 + root * 2 && !(isPvNode && isCapture);

        if (doLmr)
        {
            reduction = lmrTable[depth][searchedMoves];
            int lmrAdjustments = 0;
            if (!isPvNode && quietMoves >= 4)
            {
                lmrAdjustments += PV_LMR_ADD;
            }
            if (isQuiet)
            {
                lmrAdjustments -= std::clamp(historyScore / HIST_LMR_DIV, -2, 2) * 1024;
            }
            if (isQuiet)
            {
                lmrAdjustments += QUIET_LMR_ADD;
            }
            if (cutnode)
            {
                lmrAdjustments += CUTNODE_LMR_ADD;
            }
            if (ttPv)
            {
                lmrAdjustments -= TTPV_LMR_SUB;
            }
            if (improving)
            {
                lmrAdjustments -= IMPROVING_LMR_SUB;
            }
            if (abs(rawEval - staticEval) > CORRPLEXITY_LMR_THRESHOLD)
            {
                lmrAdjustments -= CORRPLEXITY_LMR_SUB;
            }
            if (move == data.killerMoves[currentPly])
            {
                lmrAdjustments -= KILLER_LMR_SUB;
            }
            if (abs(staticEval - materialValue * 1024 / EVALPLEXITY_LMR_SCALE) > EVALPLEXITY_LMR_THRESHOLD)
            {
                lmrAdjustments -= EVALPLEXITY_LMR_SUB;
            }
            lmrAdjustments /= 1024;
            reduction += lmrAdjustments;
        }

        data.searchStack[currentPly].reduction = reduction;
        if (reduction < 0)
            reduction = 0;
        bool isReduced = reduction > 0;

        int maxReduction = depth - 2;
        if (maxReduction < 0)
            maxReduction = 0;

        reduction = std::min(reduction, maxReduction);

        int childDepth = depth + extension - 1;

        uint64_t nodesBeforeSearch = data.searchNodeCount;

        //Late move reduction
        //do reduced zero window search for late moves
        //to prove they are worse than previously serached moves
        if (doLmr)
        {
            score = -AlphaBeta(board, data, childDepth - reduction, -alpha - 1, -alpha, true);
            if (score > alpha && isReduced)
            {
                //do deeper research if the move is promising,
                //and do shallower research if the move looks bad
                bool doDeeper = score > bestValue + DODEEPER_MULTIPLIER + depth * childDepth;
                bool doShallower = score < bestValue + childDepth;
                childDepth += doDeeper - doShallower;

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
        UnmakeMove(board, move, undoInfo.captured_piece);
        ApplyCopyMake(board, undoInfo, data, currentPly);
        board.history.pop_back();
        data.ply--;

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
        //beta cutoff
        if (alpha >= beta)
        {
            ttFlag = HFLOWER;

            if (isQuiet)
            {
                //update killer moves for move ordering
                data.killerMoves[currentPly] = move;

                //update history scores
                int16_t mainHistBonus =
                    std::min((int)MAINHIST_BONUS_MAX, MAINHIST_BONUS_BASE + MAINHIST_BONUS_MULT * depth);
                int16_t mainHistMalus =
                    std::min((int)MAINHIST_MALUS_MAX, MAINHIST_MALUS_BASE + MAINHIST_MALUS_MULT * depth);

                UpdateMainHist(data, board.side, move.From, move.To, mainHistBonus, oppThreats);
                MalusMainHist(data, searchedQuietMoves, move, mainHistMalus, oppThreats);

                int16_t contHistBonus =
                    std::min((int)CONTHIST_BONUS_MAX, CONTHIST_BONUS_BASE + CONTHIST_BONUS_MULT * depth);
                int16_t contHistMalus =
                    std::min((int)CONTHIST_MALUS_MAX, CONTHIST_MALUS_BASE + CONTHIST_MALUS_MULT * depth);
                UpdateContHist(move, contHistBonus, data);
                MalusContHist(data, searchedQuietMoves, move, contHistMalus);

                int16_t captHistMalus =
                    std::min((int)CAPTHIST_MALUS_MAX, CAPTHIST_MALUS_BASE + CAPTHIST_MALUS_MULT * depth);
                MalusCaptHist(data, searchedNoisyMoves, move, captHistMalus, board);
            }
            else
            {
                //update capture history scores
                int16_t captHistBonus =
                    std::min((int)CAPTHIST_BONUS_MAX, CAPTHIST_BONUS_BASE + CAPTHIST_BONUS_MULT * depth);
                int16_t captHistMalus =
                    std::min((int)CAPTHIST_MALUS_MAX, CAPTHIST_MALUS_BASE + CAPTHIST_MALUS_MULT * depth);

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
                return -MATESCORE + data.ply;
            }
            else
            {
                //stalemate
                return 0;
            }
        }
    }
    //keep the best move for tt if we're in fail low node
    if (ttFlag == HFUPPER && ttHit)
    {
        bestMove.From = ttEntry.bestMove.from();
        bestMove.To = ttEntry.bestMove.to();
        bestMove.Type = ttEntry.bestMove.type();
    }
    //update corrhist values based on the difference between static eval and search score
    if (!isSingularSearch && !isInCheck && (bestMove == Move(0, 0, 0, 0) || IsMoveQuiet(bestMove))
        && !(ttFlag == HFLOWER && bestValue <= staticEval) && !(ttFlag == HFUPPER && bestValue >= staticEval))
    {
        UpdateCorrhists(board, depth, bestValue - staticEval, data);
    }

    //store transposition table
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

    //Iterative deepening
    //gradually increase the search depth to search as deep as possible
    //and allow more cutoffs
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
            if (data.currDepth != 1 && (searchLimits.HardTimeLimit != NOLIMIT && MS > searchLimits.HardTimeLimit)
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

        //Node tm
        //scale soft time bound based on the ratio of nodes spent for searching the best move
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
        if (data.currDepth != 1 && (searchLimits.HardTimeLimit != NOLIMIT && elapsedMS > searchLimits.HardTimeLimit)
            || data.stopSearch.load()
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
        if (data.currDepth != 1
                && (searchLimits.SoftTimeLimit != NOLIMIT
                    && elapsedMS > (double)searchLimits.SoftTimeLimit * nodesTmScale)
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
