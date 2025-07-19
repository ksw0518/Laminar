#include "Ordering.h"
#include "Movegen.h"
#include "Search.h"
#include "Const.h"
#include "Board.h"
#include <algorithm>
bool isMoveNoisy(Move& move)
{
	return (move.Type & (captureFlag | promotionFlag)) != 0;
}
bool IsMoveCapture(Move& move)
{
	return (move.Type & captureFlag) != 0;
}
bool IsEpCapture(Move& move)
{
	return (move.Type & ep_capture) != 0;
}
int GetMoveScore(Move& move, Board& board, ThreadData& data)
{
	if (IsMoveCapture(move))
	{
		int attacker = get_piece(move.Piece, White);

		int victim = IsEpCapture(move) ? P : get_piece(board.mailbox[move.To], White);
		int attackerValue = PieceValues[attacker];
		int victimValue = PieceValues[victim];

		int mvvlvaValue = victimValue * 10 - attackerValue;
		return mvvlvaValue + 900000;
	}
	else
	{
		int mainHistValue = data.histories.mainHist[board.side][move.From][move.To];
		return mainHistValue;
	}
}
int QsearchGetMoveScore(Move& move, Board& board)
{
	if (IsMoveCapture(move))
	{
		int attacker = get_piece(move.Piece, White);

		int victim = IsEpCapture(move) ? P : get_piece(board.mailbox[move.To], White);
		int attackerValue = PieceValues[attacker];
		int victimValue = PieceValues[victim];

		int mvvlvaValue = victimValue * 10 - attackerValue;
		return mvvlvaValue;
	}
	else
	{
		return -90000;
	}
}
void SortMoves(MoveList& ml, Board& board, ThreadData& data)
{
	ScoredMove scored[256];

	for (int i = 0; i < ml.count; ++i)
	{
		scored[i].score = GetMoveScore(ml.moves[i], board, data);
		scored[i].move = ml.moves[i];
	}

	std::stable_sort(scored, scored + ml.count, [](const ScoredMove& a, const ScoredMove& b)
		{
			return a.score > b.score;
		});

	for (int i = 0; i < ml.count; ++i)
	{
		ml.moves[i] = scored[i].move;
	}
}
void SortNoisyMoves(MoveList& ml, Board& board)
{
	ScoredMove scored[256];

	for (int i = 0; i < ml.count; ++i)
	{
		scored[i].score = QsearchGetMoveScore(ml.moves[i], board);
		scored[i].move = ml.moves[i];
	}

	std::stable_sort(scored, scored + ml.count, [](const ScoredMove& a, const ScoredMove& b)
		{
			return a.score > b.score;
		});

	for (int i = 0; i < ml.count; ++i)
	{
		ml.moves[i] = scored[i].move;
	}
}