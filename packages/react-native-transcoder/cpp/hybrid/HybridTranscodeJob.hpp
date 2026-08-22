#pragma once

#include "HybridMediaDestination.hpp"
#include "HybridMediaSource.hpp"
#include "HybridTranscodeJobSpec.hpp"
#include "media/CancellationToken.hpp"
#include "media/MediaModel.hpp"

#include <atomic>
#include <chrono>
#include <deque>
#include <mutex>
#include <vector>

namespace margelo::nitro::transcoder {

// Single-use. Owns its cancellation token, listeners, diagnostics and terminal
// result; keeps the source and destination alive for the whole run.
class HybridTranscodeJob final : public HybridTranscodeJobSpec {
public:
  HybridTranscodeJob(std::shared_ptr<HybridMediaSource> source, std::shared_ptr<HybridMediaDestination> destination,
                     PlanData plan);

  JobState getState() override;
  std::shared_ptr<Promise<std::shared_ptr<HybridTranscodeReportSpec>>> run() override;
  void cancel() override;
  ProgressSubscription addOnProgressListener(const std::function<void(const TranscodeProgress&)>& listener) override;
  void setProgressUpdatesPerSecond(double updatesPerSecond) override;
  std::shared_ptr<Promise<std::string>> exportDiagnostics() override;
  void close() override;

private:
  void deliverProgress(const ProgressSnapshot& snapshot);
  void log(const std::string& line);

  std::shared_ptr<HybridMediaSource> _source;
  std::shared_ptr<HybridMediaDestination> _destination;
  PlanData _plan;
  CancellationTokenRef _cancellation = std::make_shared<CancellationToken>();

  std::mutex _mutex;
  JobState _state = JobState::READY;
  std::unordered_map<double, std::function<void(const TranscodeProgress&)>> _listeners;
  double _nextListenerId = 1;
  double _progressIntervalMs = 100;
  JobPhase _lastPhase = JobPhase::STARTING;
  std::chrono::steady_clock::time_point _lastDelivery{};
  std::deque<std::string> _diagnostics;
};

} // namespace margelo::nitro::transcoder
