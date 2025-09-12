#include <gtest/gtest.h>
#include <base/thread/threadpool.h>

#include <chrono>
#include <future>

TEST(ThreadPoolTest, BasicTaskExecution) {
  ThreadPool& pool = ThreadPool::instance();
  auto future = pool.executeTask([]() { return 1 + 1; });
  ASSERT_EQ(future.get(), 2);
}

TEST(ThreadPoolTest, MultipleTasks) {
  ThreadPool& pool = ThreadPool::instance();
  std::vector<std::future<int>> futures;
  for (int i = 0; i < 10; ++i) {
    futures.emplace_back(pool.executeTask([i]() { return i * 2; }));
  }

  for (int i = 0; i < 10; ++i) {
    ASSERT_EQ(futures[i].get(), i * 2);
  }
}

TEST(ThreadPoolTest, SetMaxIdleThread) {
  ThreadPool& pool = ThreadPool::instance();
  pool.setMaxIdleThread(5);
  // This test is difficult to verify directly without exposing internal state.
  // We can add some logging to ThreadPool to observe the number of idle threads.
  // For now, we just call the method to ensure it doesn't crash.
}

TEST(ThreadPoolTest, JoinAll) {
    ThreadPool& pool = ThreadPool::instance();
    std::vector<std::future<void>> futures;
    for (int i = 0; i < 5; ++i) {
        futures.emplace_back(pool.executeTask([i]() {
            std::this_thread::sleep_for(std::chrono::milliseconds(10 * i));
        }));
    }
    pool.joinAll();
    // After joinAll, all tasks should be completed.  We can't directly verify
    // thread completion without exposing internal state, but we can assume
    // that if joinAll returns, the tasks have completed.
}