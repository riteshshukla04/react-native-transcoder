#include "HybridTranscodeJob.hpp"
#include "EngineTask.hpp"
#include "HybridTranscodeReport.hpp"
#include "backends/ffmpeg/FfmpegTranscoder.hpp"
#include "media/Error.hpp"

#include <algorithm>

namespace margelo::nitro::transcoder {

namespace {
constexpr std::size_t MAX_DIAGNOSTIC_LINES = 256;
}

HybridTranscodeJob::HybridTranscodeJob(std::shared_ptr<HybridMediaSource> source,
                                       std::shared_ptr<HybridMediaDestination> destination, PlanData plan)
    : HybridObject(TAG), _source(std::move(source)), _destination(std::move(destination)), _plan(std::move(plan)) {}

JobState HybridTranscodeJob::getState() {
  std::lock_guard<std::mutex> lock(_mutex);
  return _state;
}

void HybridTranscodeJob::cancel() {
  _cancellation->cancel();
  log("cancel requested");
}

ProgressSubscription
HybridTranscodeJob::addOnProgressListener(const std::function<void(const TranscodeProgress&)>& listener) {
  double id = 0;
  {
    std::lock_guard<std::mutex> lock(_mutex);
    id = _nextListenerId++;
    _listeners.emplace(id, listener);
  }
  // The subscription holds a weak reference, so removing it cannot resurrect a
  // finished job.
  std::weak_ptr<HybridTranscodeJob> weakSelf = std::dynamic_pointer_cast<HybridTranscodeJob>(shared_from_this());
  return ProgressSubscription([weakSelf, id]() {
    if (auto self = weakSelf.lock()) {
      std::lock_guard<std::mutex> lock(self->_mutex);
      self->_listeners.erase(id);
    }
  });
}

void HybridTranscodeJob::setProgressUpdatesPerSecond(double updatesPerSecond) {
  double clamped = std::clamp(updatesPerSecond, 1.0, 60.0);
  std::lock_guard<std::mutex> lock(_mutex);
  _progressIntervalMs = 1000.0 / clamped;
}

void HybridTranscodeJob::deliverProgress(const ProgressSnapshot& snapshot) {
  std::vector<std::function<void(const TranscodeProgress&)>> listeners;
  {
    std::lock_guard<std::mutex> lock(_mutex);
    if (_listeners.empty()) return;

    auto now = std::chrono::steady_clock::now();
    bool phaseChanged = snapshot.phase != _lastPhase;
    bool terminal = snapshot.phase == JobPhase::DONE;
    double sinceLastMs = std::chrono::duration<double, std::milli>(now - _lastDelivery).count();
    if (!phaseChanged && !terminal && sinceLastMs < _progressIntervalMs) return;

    _lastPhase = snapshot.phase;
    _lastDelivery = now;
    listeners.reserve(_listeners.size());
    for (const auto& [id, listener] : _listeners)
      listeners.push_back(listener);
  }

  TranscodeProgress progress(snapshot.phase, snapshot.inputSecondsProcessed, snapshot.totalInputSeconds,
                             snapshot.inputBytesRead, snapshot.outputBytesWritten, snapshot.speedRatio,
                             snapshot.estimatedSecondsRemaining);
  // Never invoke a listener while holding the lock.
  for (const auto& listener : listeners)
    listener(progress);
}

void HybridTranscodeJob::log(const std::string& line) {
  std::lock_guard<std::mutex> lock(_mutex);
  _diagnostics.push_back(line);
  if (_diagnostics.size() > MAX_DIAGNOSTIC_LINES) _diagnostics.pop_front();
}

std::shared_ptr<Promise<std::shared_ptr<HybridTranscodeReportSpec>>> HybridTranscodeJob::run() {
  {
    std::lock_guard<std::mutex> lock(_mutex);
    if (_state != JobState::READY) {
      throw TranscoderException(error_code::INVALID_REQUEST, error_stage::ENGINE,
                                "This job already ran. Create a new job for another run.");
    }
    _state = JobState::RUNNING;
  }

  auto self = std::dynamic_pointer_cast<HybridTranscodeJob>(shared_from_this());
  return runOnEngine<std::shared_ptr<HybridTranscodeReportSpec>>(
      [self]() -> std::shared_ptr<HybridTranscodeReportSpec> {
        try {
          ReportData report = ffmpeg::runPlan(
              self->_source->byteSource(), self->_destination->byteSink(), self->_plan, self->_cancellation,
              [self](const ProgressSnapshot& snapshot) { self->deliverProgress(snapshot); });
          report.outputUri = self->_destination->getUri();
          {
            std::lock_guard<std::mutex> lock(self->_mutex);
            self->_state = JobState::COMPLETED;
          }
          return std::make_shared<HybridTranscodeReport>(std::move(report));
        } catch (const TranscoderException& error) {
          bool cancelled = error.code() == error_code::CANCELLED;
          {
            std::lock_guard<std::mutex> lock(self->_mutex);
            self->_state = cancelled ? JobState::CANCELLED : JobState::FAILED;
          }
          self->log(std::string("job failed: ") + error.code());
          throw;
        } catch (...) {
          {
            std::lock_guard<std::mutex> lock(self->_mutex);
            self->_state = JobState::FAILED;
          }
          throw;
        }
      });
}

std::shared_ptr<Promise<std::string>> HybridTranscodeJob::exportDiagnostics() {
  auto self = std::dynamic_pointer_cast<HybridTranscodeJob>(shared_from_this());
  return runOnEngine<std::string>([self]() -> std::string {
    std::lock_guard<std::mutex> lock(self->_mutex);
    std::string out;
    for (const auto& line : self->_diagnostics) {
      out += line;
      out += "\n";
    }
    return out;
  });
}

void HybridTranscodeJob::close() {
  cancel();
  std::lock_guard<std::mutex> lock(_mutex);
  _listeners.clear();
  _source.reset();
  _destination.reset();
}

} // namespace margelo::nitro::transcoder
