#pragma once
#include "Board.h"
#include "Search.h"
#include <atomic>
#include <condition_variable>
#include <mutex>
#include <thread>

struct alignas(64) Worker
{
    ThreadData data;
    std::thread thread;

    std::condition_variable cv;
    std::mutex mtx;

    int id = 0;

    std::atomic<bool> searching{false};
    std::atomic<bool> exit{false};

    // search inputs
    Board board;
    SearchLimitations limits;
    int depth = 0;
};

extern std::vector<std::unique_ptr<Worker>> threadPool;
void workerLoop(Worker* worker);
void startWorkers(int threadCount);
void destroyWorkers();
void destroyWorkers();
void startSearch(const Board& board, SearchLimitations limits, int depth);
void stopCurrentSearch();
