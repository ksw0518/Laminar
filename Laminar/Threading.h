#pragma once
#include "Board.h"
#include "Search.h"
#include <thread>
extern std::vector<ThreadData*> allThreadDataPtrs;
extern std::vector<std::unique_ptr<ThreadData>> persistentThreadData;
void RunSearchInMultipleThreads(Board& board, int depth, SearchLimitations& searchLimits, int threadCount);
