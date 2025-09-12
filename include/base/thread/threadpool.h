#ifndef BASE_THREAD_THREADPOOL_H
#define BASE_THREAD_THREADPOOL_H

#include <atomic>
#include <condition_variable>
#include <deque>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <thread>
#include <type_traits>
#include <utility>

// final: 不能被任何其他类继承
class ThreadPool final {
 public:
  ~ThreadPool();
  ThreadPool(const ThreadPool& other) = delete;
  void operator=(const ThreadPool& other) = delete;

  static ThreadPool& instance();

  template <typename F, typename... Args>
  auto executeTask(F&& f, Args&&... args)
      -> std::future<typename std::result_of<F(Args...)>::type>;
  // 尾置返回类型（trailing return type） "->"
  // std::future<T> 模板参数 T 表示未来要返回的值的类型
  // std::result_of：计算“调用 F 并传入参数类型 Args... 后”的返回类型

  void joinAll();                         // 等待所有任务完成，销毁线程
  void setMaxIdleThread(unsigned int m);  // 设置最大空闲线程数

 private:
  ThreadPool();

  void _createWorker();
  void _workerRoutine();
  void _monitorRoutine();

  std::thread monitor_;
  std::atomic<unsigned> maxIdleThread_;
  std::atomic<unsigned> pendingStopSignal_;

  // thread_local： 线程局部存储变量（Thread-Local Storage, TLS）
  // 每个线程都会有一份独立的副本，修改这个变量不会影响其他线程
  static thread_local bool working_;

  std::deque<std::thread> worker_;
  std::mutex mutex_;
  std::condition_variable cond_;
  unsigned waiters_;
  bool shutdown_;
  std::deque<std::function<void()>> tasks_;

  static const int kMaxThreads = 256; // 线程池允许的最大线程数量
};

template <typename F, typename... Args>
auto ThreadPool::executeTask(F&& f, Args&&... args)
    -> std::future<typename std::result_of<F(Args...)>::type> {
  using resultType = typename std::result_of<F(Args...)>::type;

  // std::packaged_task<resultType()> :
  // bind:生成无参函数 f(args)
  auto task = std::make_shared<std::packaged_task<resultType()>>(
      std::bind(std::forward<F>(f), std::forward<Args>(args)...));

  {
    std::unique_lock<std::mutex> guard(mutex_);
    if (shutdown_)
      return std::future<resultType>();
    tasks_.emplace_back([=]() { (*task)(); });
    if (waiters_ == 0) {
      _createWorker();
    }
    cond_.notify_one();
  }
  return task->get_future();
}
#endif