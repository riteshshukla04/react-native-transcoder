#pragma once

#include "HybridMediaSourceSpec.hpp"
#include "media/ByteIo.hpp"
#include "media/MediaModel.hpp"

#include <memory>
#include <mutex>
#include <optional>
#include <string>

namespace margelo::nitro::transcoder {

// One native-owned input. `FileMediaSource`, `HttpMediaSource` and the memory
// source are all this one JS type — the implementation stays native-only.
// The first successful probe is cached and reused by `resolve` and both
// job-creation paths, so no media payload byte is ever read twice.
class HybridMediaSource final : public HybridMediaSourceSpec {
public:
  HybridMediaSource(MediaSourceKind kind, std::unique_ptr<ByteSource> byteSource, std::optional<std::string> uri)
      : HybridObject(TAG), _kind(kind), _byteSource(std::move(byteSource)), _uri(std::move(uri)) {}

  MediaSourceKind getKind() override { return _kind; }
  bool getIsSeekable() override;
  std::optional<double> getByteLength() override;
  bool getIsProbed() override;
  void close() override;

  // Engine-internal. Throws when the source was already closed.
  ByteSource& byteSource();
  const std::optional<std::string>& uri() const noexcept { return _uri; }

  // Probes once and caches the record; later calls reuse it.
  const ProbeResult& probeRecord();

private:
  MediaSourceKind _kind;
  std::unique_ptr<ByteSource> _byteSource;
  std::optional<std::string> _uri;
  std::optional<ProbeResult> _probe;
  std::mutex _mutex;
};

} // namespace margelo::nitro::transcoder
