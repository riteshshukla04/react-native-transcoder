#include "media/Executor.hpp"

#include <utility>

namespace margelo::nitro::transcoder {

Executor::Executor(std::size_t threadCount) {
  _threads.reserve(threadCount);
  for (std::size_t i = 0; i < threadCount; i++) {
    _threads.emplace_back([this] { workerLoop(); });
  }
}

Executor::~Executor() {
  {
    std::lock_guard<std::mutex> lock(_mutex);
    _stopped = true;
  }
  _condition.notify_all();
  for (auto& thread : _threads) {
    if (thread.joinable()) thread.join();
  }
}

void Executor::submit(std::function<void()> task) {
  {
    std::lock_guard<std::mutex> lock(_mutex);
    _tasks.push(std::move(task));
  }
  _condition.notify_one();
}

void Executor::workerLoop() {
  while (true) {
    std::function<void()> task;
    {
      std::unique_lock<std::mutex> lock(_mutex);
      _condition.wait(lock, [this] { return _stopped || !_tasks.empty(); });
      if (_stopped && _tasks.empty()) return;
      task = std::move(_tasks.front());
      _tasks.pop();
    }
    // Never invoke a task while holding the lock.
    task();
  }
}

} // namespace margelo::nitro::transcoder
