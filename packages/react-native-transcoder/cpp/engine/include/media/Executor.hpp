#pragma once

#include <condition_variable>
#include <cstddef>
#include <functional>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

namespace margelo::nitro::transcoder {

// Engine-owned worker pool. No media work ever runs on the JS or UI thread.
class Executor final {
public:
  explicit Executor(std::size_t threadCount);
  ~Executor();

  Executor(const Executor&) = delete;
  Executor& operator=(const Executor&) = delete;

  void submit(std::function<void()> task);
  std::size_t threadCount() const noexcept { return _threads.size(); }

private:
  void workerLoop();

  std::vector<std::thread> _threads;
  std::queue<std::function<void()>> _tasks;
  std::mutex _mutex;
  std::condition_variable _condition;
  bool _stopped = false;
};

} // namespace margelo::nitro::transcoder
