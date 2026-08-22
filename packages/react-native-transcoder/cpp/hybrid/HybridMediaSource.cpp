#include "HybridMediaSource.hpp"
#include "backends/ffmpeg/FfmpegProbe.hpp"
#include "media/Error.hpp"

namespace margelo::nitro::transcoder {

bool HybridMediaSource::getIsSeekable() {
  std::lock_guard<std::mutex> lock(_mutex);
  return _byteSource != nullptr && _byteSource->isSeekable();
}

std::optional<double> HybridMediaSource::getByteLength() {
  std::lock_guard<std::mutex> lock(_mutex);
  if (_byteSource == nullptr) return std::nullopt;
  auto length = _byteSource->byteLength();
  if (!length.has_value()) return std::nullopt;
  return static_cast<double>(*length);
}

bool HybridMediaSource::getIsProbed() {
  std::lock_guard<std::mutex> lock(_mutex);
  return _probe.has_value();
}

void HybridMediaSource::close() {
  std::lock_guard<std::mutex> lock(_mutex);
  _byteSource.reset();
  _probe.reset();
}

ByteSource& HybridMediaSource::byteSource() {
  std::lock_guard<std::mutex> lock(_mutex);
  if (_byteSource == nullptr) {
    throw TranscoderException(error_code::INVALID_REQUEST, error_stage::OPEN, "This MediaSource was already closed.");
  }
  return *_byteSource;
}

const ProbeResult& HybridMediaSource::probeRecord() {
  std::lock_guard<std::mutex> lock(_mutex);
  if (_byteSource == nullptr) {
    throw TranscoderException(error_code::INVALID_REQUEST, error_stage::PROBE, "This MediaSource was already closed.");
  }
  if (!_probe.has_value()) {
    _probe = ffmpeg::probeSource(*_byteSource);
  }
  return *_probe;
}

} // namespace margelo::nitro::transcoder
