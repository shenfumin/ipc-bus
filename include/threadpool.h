#pragma once
#include <atomic>
#include <future>
#include <iostream>
#include <queue>
#include <stdexcept>
#include <vector>
namespace ipcbus
{
#define THREADPOOL_MAX_NUM 16
#define JOBS_HIGH_WATERMARK 1000

class Threadpool
{
    unsigned short _initSize;
    using Task = std::function<void()>;
    std::vector<std::thread> _pool;
    std::queue<Task> _tasks;
    std::mutex _lock;
#ifdef THREADPOOL_AUTO_GROW
    mutex _lockGrow;
#endif
    std::condition_variable _task_cv;
    std::atomic<bool> _run{true};
    std::atomic<int> _idlThrNum{0};

  public:
    static Threadpool &instance()
    {
        static Threadpool instance;
        return instance;
    }
    inline Threadpool(unsigned short size = std::thread::hardware_concurrency())
    {
        _initSize = size;
        addThread(size);
    }
    inline ~Threadpool()
    {
        _run = false;
        _task_cv.notify_all();
        for (std::thread &thread : _pool)
        {
            // thread.detach();
            if (thread.joinable())
                thread.join();
        }
    }

  public:
    template <class F, class... Args> auto commit(F &&f, Args &&... args) -> std::future<decltype(f(args...))>
    {
        if (!_run) // stoped ??
            throw std::runtime_error("commit on ThreadPool is stopped.");
        if (_tasks.size() >= JOBS_HIGH_WATERMARK)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            std::cout << "Warning:reach the high water mark for jobs of "
                         "Threadpool;now tasks size:"
                      << _tasks.size() << std::endl;
        }
        using RetType = decltype(f(args...));
        auto task = std::make_shared<std::packaged_task<RetType()>>(std::bind(std::forward<F>(f), std::forward<Args>(args)...));
        std::future<RetType> future = task->get_future();
        {
            std::lock_guard<std::mutex> lock{_lock};
            _tasks.emplace([task]() { (*task)(); });
        }
#ifdef THREADPOOL_AUTO_GROW
        if (_idlThrNum < 1 && _pool.size() < THREADPOOL_MAX_NUM)
            addThread(1);
#endif // !THREADPOOL_AUTO_GROW
        _task_cv.notify_one();

        return future;
    }
    template <class F> void commit2(F &&task)
    {
        if (!_run)
            return;
        {
            std::lock_guard<std::mutex> lock{_lock};
            _tasks.emplace(std::forward<F>(task));
        }
#ifdef THREADPOOL_AUTO_GROW
        if (_idlThrNum < 1 && _pool.size() < THREADPOOL_MAX_NUM)
            addThread(1);
#endif // !THREADPOOL_AUTO_GROW
        _task_cv.notify_one();
    }
    int idlCount() { return _idlThrNum; }
    int thrCount() { return _pool.size(); }

#ifndef THREADPOOL_AUTO_GROW
  private:
#endif // !THREADPOOL_AUTO_GROW
    void addThread(unsigned short size)
    {
#ifdef THREADPOOL_AUTO_GROW
        if (!_run) // stoped ??
            throw runtime_error("Grow on ThreadPool is stopped.");
        unique_lock<mutex> lockGrow{_lockGrow};
#endif
        for (; _pool.size() < THREADPOOL_MAX_NUM && size > 0; --size)
        {
            _pool.emplace_back([this] {
                while (true)
                {
                    Task task;
                    {

                        std::unique_lock<std::mutex> lock{_lock};
                        _task_cv.wait(lock, [this] { return !_run || !_tasks.empty(); });
                        if (!_run && _tasks.empty())
                            return;
                        _idlThrNum--;
                        task = move(_tasks.front());
                        _tasks.pop();
                    }
                    task();
#ifdef THREADPOOL_AUTO_GROW
                    if (_idlThrNum > 0 && _pool.size() > _initSize)
                        return;
#endif // !THREADPOOL_AUTO_GROW
                    {
                        std::unique_lock<std::mutex> lock{_lock};
                        _idlThrNum++;
                    }
                }
            });
            {
                std::unique_lock<std::mutex> lock{_lock};
                _idlThrNum++;
            }
        }
    }
};

} // namespace ipcbus
