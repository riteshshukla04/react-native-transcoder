#pragma once

#include <atomic>
#include <memory>

namespace margelo::nitro::transcoder {

// Cooperative and idempotent. Checked at every I/O and packet/frame boundary.
class CancellationToken final {
public:
  void cancel() noexcept { _cancelled.store(true, std::memory_order_release); }
  bool isCancelled() const noexcept { return _cancelled.load(std::memory_order_acquire); }

private:
  std::atomic<bool> _cancelled{false};
};

using CancellationTokenRef = std::shared_ptr<CancellationToken>;

} // namespace margelo::nitro::transcoder
