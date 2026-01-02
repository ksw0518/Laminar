#include "Threading.h"
#include "Board.h"
#include "Search.h"
#include <iostream>
#include <thread>

std::vector<std::unique_ptr<Worker>> threadPool = {};

void workerLoop(Worker* worker)
{
    std::unique_lock<std::mutex> lock(worker->mtx);

    while (true)
    {
        worker->cv.wait(lock, [&] { return worker->searching.load() || worker->exit.load(); });

        //stop if exit is true
        if (worker->exit.load())
        {
            worker->data.stopSearch.store(true);
            return;
        }

        worker->data.stopSearch.store(false);
        worker->data.isMainThread = (worker->id == 0);

        Board localBoard = worker->board;
        SearchLimitations limits = worker->limits;
        int depth = worker->depth;

        //searching == true
        lock.unlock();

        IterativeDeepening(localBoard, depth, limits, worker->data, false);

        lock.lock();
        worker->searching.store(false);
    }
}
void startWorkers(int threadCount)
{
    threadPool.clear();
    threadPool.reserve(threadCount);
    for (int i = 0; i < threadCount; i++)
    {
        auto worker = std::make_unique<Worker>();
        worker->id = i;
        InitializeSearch(worker->data);
        worker->data.stopSearch.store(false);
        worker->searching.store(false);
        worker->exit.store(false);

        worker->thread = std::thread(workerLoop, worker.get());
        threadPool.push_back(std::move(worker));
    }
}
void destroyWorkers()
{
    for (auto& worker : threadPool)
    {
        worker->exit.store(true, std::memory_order_release);
        worker->data.stopSearch.store(true, std::memory_order_release);
    }
    for (auto& worker : threadPool)
    {
        worker->cv.notify_all();
    }
    for (auto& worker : threadPool)
    {
        if (worker->thread.joinable())
            worker->thread.join();
    }
    threadPool.clear();
}
void startSearch(const Board& board, SearchLimitations limits, int depth)
{
    for (auto& worker : threadPool)
    {
        std::lock_guard<std::mutex> lock(worker->mtx);

        worker->board = board;
        worker->limits = limits;
        worker->depth = depth;

        worker->searching.store(true);
    }
    for (auto& worker : threadPool)
    {
        worker->cv.notify_all();
    }
}
void stopCurrentSearch()
{
    for (auto& worker : threadPool)
        worker->data.stopSearch.store(true, std::memory_order_release);
}

//Lazy SMP
//simply run multiple searches in helper threads, and share TT
//to allow more cutoffs in the main thread
