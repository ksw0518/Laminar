#include "Search.h"
#include "Evaluation.h"
#include "Board.h"
#include "Movegen.h"
#include "Ordering.h"
#include "Const.h"
#include "Bit.h"
#include "Tuneables.h"
#include <iostream>
#include <chrono>
#include <cmath>
#include <limits>
#include <cstring>
int lmrTable[MAXPLY][256];
void InitializeLMRTable()
{
    for (int depth = 1; depth < MAXPLY; depth++)
    {
        for (int move = 1; move < 256; move++)
        {
            lmrTable[depth][move] = std::floor((float)LMR_OFFSET / 100 + log(move) * log(depth) / ((float)LMR_DIVISOR / 100));
        }
    }
}

inline int QuiescentSearch(Board& board, ThreadData& data, int alpha, int beta)
{

    auto now = std::chrono::steady_clock::now();
    int64_t elapsedMS = std::chrono::duration_cast<std::chrono::milliseconds>(now - data.clockStart).count();
    if (data.stopSearch || elapsedMS > data.SearchTime) {
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


    
    int score = -INFINITY;
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
        if (!isMoveNoisy(move)) continue;
        int lastEp = board.enpassent;
        uint8_t lastCastle = board.castle;
        bool lastside = board.side;
        int captured_piece = board.mailbox[move.To];

        MakeMove(board, move);
        data.ply++;

    
        if (!isLegal(move, board))
        {
            UnmakeMove(board, move, captured_piece);

            board.enpassent = lastEp;
            board.castle = lastCastle;
            board.side = lastside;
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
    if (data.stopSearch || elapsedMS > data.SearchTime) {
        data.stopSearch = true;
        return 0;
    }

	bool isPvNode = beta - alpha > 1;

	int score = 0;
	int bestValue = -MAXSCORE;

    int currentPly = data.ply;
    data.selDepth = std::max(currentPly, data.selDepth);
    data.pvLengths[currentPly] = currentPly;
    if (depth <= 0 || currentPly >= MAXPLY - 1)
    {
        score = -QuiescentSearch(board, data, alpha, beta);
        return score;
    }
	MoveList moveList;
	GeneratePseudoLegalMoves(moveList, board);
    SortMoves(moveList, board);

    int searchedMoves = 0;
    for (int i = 0; i < moveList.count; ++i)
    {
        Move& move = moveList.moves[i];

        int lastEp = board.enpassent;
        uint8_t lastCastle = board.castle;
        bool lastside = board.side;
        int captured_piece = board.mailbox[move.To];

        int childDepth = depth - 1;
        MakeMove(board, move);
        data.ply++;
        if (!isLegal(move, board))
        {
            UnmakeMove(board, move, captured_piece);

            board.enpassent = lastEp;
            board.castle = lastCastle;
            board.side = lastside;
            data.ply--;
            continue;
        }
        searchedMoves++;
        data.searchNodeCount++;
        data.searchStack[currentPly].move = move;

        int reduction = 0;
        if (depth > MIN_LMR_DEPTH && searchedMoves > 1)
        {
            reduction = lmrTable[depth][searchedMoves] - 1;
        }
        if (reduction < 0) reduction = 0;
        bool isReduced = reduction > 0;
        if (searchedMoves <= 1)
        {
            score = -AlphaBeta(board, data, childDepth, -beta, -alpha);
        }
        else
        {
            if (isReduced)
            {
                score = -AlphaBeta(board, data, childDepth - reduction, -alpha - 1, -alpha);
            }
            else
            {
                score = alpha + 1;
            }
            if (score > alpha)
            {
                score = -AlphaBeta(board, data, childDepth, -alpha - 1, -alpha);
            }
            if (score > alpha && score < beta)
            {
                score = -AlphaBeta(board, data, childDepth, -beta, -alpha);
            }
        }
        UnmakeMove(board, move, captured_piece);
        data.ply--;

        board.enpassent = lastEp;
        board.castle = lastCastle;
        board.side = lastside;

        bestValue = std::max(score, bestValue);
        if (bestValue > alpha)
        {
            alpha = score;
            
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
            break;
        }
    }

    if (searchedMoves == 0)
    {
        if (IsSquareAttacked(get_ls1b(board.side == White ? board.bitboards[K] : board.bitboards[k]), 1 - board.side, board, board.occupancies[Both]))
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
    return bestValue;
}
void print_UCI(Move& bestmove, int score, float elapsedMS, float nps, ThreadData& data)
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

    std::cout << " time " << static_cast<int>(std::round(elapsedMS)) << " nodes " << data.searchNodeCount << " nps " << static_cast<int>(std::round(nps)) <<  " pv " << std::flush;
    for (int count = 0; count < data.pvLengths[0]; count++)
    {
        printMove(data.pvTable[0][count]);
        std::cout << " ";
    }
    std::cout << "\n" << std::flush;;
}

std::pair<Move, int> IterativeDeepening(Board& board, int depth, SearchLimitations& searchLimits, ThreadData& data, bool isBench)
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

		score = AlphaBeta(board, data, data.currDepth, -MAXSCORE, MAXSCORE);

		auto end = std::chrono::steady_clock::now();
		int64_t elapsedMS = static_cast<int64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(end - data.clockStart).count());
		float second = (float)(elapsedMS + 1) / 1000;

		float nps = data.searchNodeCount / second;

		if (!data.stopSearch)
		{
			bestmove = data.pvTable[0][0];
			bestScore = score;
		}

        if (!data.stopSearch)
        {
            if (!isBench)
            {
                print_UCI(bestmove, score, elapsedMS, nps, data);

            }
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


