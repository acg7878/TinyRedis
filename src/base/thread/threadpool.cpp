#include <base/thread/threadpool.h>
#include <algorithm>
#include <chrono>
#include <functional>
#include <mutex>
#include <thread>
#include <spdlog/spdlog.h>

thread_local bool ThreadPool::working_ = true;

ThreadPool::ThreadPool() : waiters_(0), shutdown_(false) {
  // 创建监控线程
  monitor_ = std::thread([this]() { this->_monitorRoutine(); });
  maxIdleThread_ = std::max(1U, std::thread::hardware_concurrency());
  pendingStopSignal_ = 0;
}

ThreadPool::~ThreadPool() {
  joinAll();
}

ThreadPool& ThreadPool::instance() {
  static ThreadPool pool;
  return pool;
}

void ThreadPool::setMaxIdleThread(unsigned int m) {
  if (0 < m && m < kMaxThreads)
    maxIdleThread_ = m;
}

void ThreadPool::_createWorker() {
  std::thread t([this]() { this->_workerRoutine(); });
  worker_.push_back(std::move(t));
}

void ThreadPool::_workerRoutine() {
  working_ = true;
  while (working_) {
    std::function<void()> task;
    {
      std::unique_lock<std::mutex> guard(mutex_);
      ++waiters_;
      cond_.wait(guard, [this]() -> bool {
        return this->shutdown_ || !tasks_.empty();
      });
      --waiters_;

      if (this->shutdown_ && tasks_.empty()) {
        return;
      }

      task = std::move(tasks_.front());
      tasks_.pop_front();
    }
    try {
      task();
    } catch (const std::exception& e) {
      spdlog::error("Task threw an exception: {}", e.what());
    }
  }
  --pendingStopSignal_;  // 循环终止，通知 monitor线程：我已经停了
}

void ThreadPool::_monitorRoutine() {
  while (!shutdown_) {
    // 暂停线程执行 1 秒钟
    std::this_thread::sleep_for(std::chrono::seconds(1));
    std::unique_lock<std::mutex> guard(mutex_);
    if (shutdown_) {
      return;
    }
    auto nw = waiters_;

    // pendingStopSignal_:已经计划回收的线程数
    // 当前空闲线程数减去待回收线程
    nw -= pendingStopSignal_;

    // 回收多余线程
    // maxIdleThread_：线程池允许的最大空闲线程数
    while (nw-- > maxIdleThread_) {
      tasks_.push_back([this]() { working_ = false; });
      cond_.notify_one();
      ++pendingStopSignal_;
    }
  }
}

void ThreadPool::joinAll() {
  // 用 decltype 自动跟随原变量类型，方便以后更改
  decltype(worker_) tmp;
  {
    std::unique_lock<std::mutex> guard(mutex_);
    if (shutdown_)
      return;

    shutdown_ = true;
    cond_.notify_all();
    // TODO：没搞懂为什么swap
    // swap 是为了先拿到线程对象，再在不持锁的情况下 join，避免死锁
    tmp.swap(worker_);
    worker_.clear();
  }
  for (auto& t : tmp) {
    if (t.joinable()) {
      t.join();
    }
  }
  if (monitor_.joinable()) {
    monitor_.join();
  }
}

