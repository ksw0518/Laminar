#include "Movegen.h"
#include "Const.h"
#include <cstdint>
#include <chrono>

struct SearchData
{
	Move move;
	int staticEval = 0;
};
struct ThreadData
{
	std::chrono::steady_clock::time_point clockStart;
	int ply = 0;
	int64_t searchNodeCount = 0;
	int64_t SearchTime = -1;
	int currDepth = 0;
	bool stopSearch = false;
	int selDepth = 0;
	SearchData searchStack[MAXPLY];
	int pvLengths[MAXPLY] = {};
	Move pvTable[MAXPLY][MAXPLY];
};
struct SearchLimitations
{
	int64_t HardTimeLimit = -1;
	//int64_t SoftTimeLimit = -1;
	//int64_t SoftNodeLimit = -1;
	//int64_t HardNodeLimit = -1;
	SearchLimitations(int hardTime = -1, int softTime = -1, int64_t softNode = -1, int64_t hardNode = -1)
		: HardTimeLimit(hardTime)
		//SoftTimeLimit(softTime),
		//SoftNodeLimit(softNode),
		//HardNodeLimit(hardNode)
	{}
};
std::pair<Move, int> IterativeDeepening(Board& board, int depth, SearchLimitations& searchLimits, ThreadData& data);
void InitializeLMRTable();