#pragma once
#include <vector>
#include <thread>
#include <functional>
#include <atomic>
#include <memory>

#include "BlockingQueue.h"

class ThreadPool
{
private:
    //创建工作线程数组
    std::vector<std::thread> workers;
    BlockingQueue<std::function<void()>> taskQueue;
    std::unique_ptr<std::atomic<bool>[]> busy;
    int workerCount;
public:
    ThreadPool(const ThreadPool&)=delete;
    ThreadPool& operator=(const ThreadPool&)=delete;
    ThreadPool(int num_threads);
    void submit(std::function<void()> task);
    bool trySubmit(std::function<void()> task, size_t queueLimit);
    std::vector<bool> workerStates() const;
    size_t pendingTasks() { return taskQueue.size(); }
    ~ThreadPool();
};
