#include "Board.h"
#include "Search.h"
#include <iostream>
#include <thread>
std::vector<ThreadData*> allThreadDataPtrs = {};
std::vector<std::unique_ptr<ThreadData>> persistentThreadData = {};

void RunSearchInMultipleThreads(Board& board, int depth, SearchLimitations& searchLimits, int threadCount)
{
    std::vector<std::thread> threads;

    for (int i = 0; i < threadCount; i++)
    {
        ThreadData* localData = persistentThreadData[i].get();
        threads.emplace_back(
            [=, &board, &searchLimits]()
            {
                if (i == 0)
                    localData->isMainThread = true;
                else
                    localData->isMainThread = false;

                Board localBoard = board; // copy board
                IterativeDeepening(localBoard, depth, searchLimits, *localData, false);
            }
        );
    }

    for (auto& t : threads)
        t.join();
}