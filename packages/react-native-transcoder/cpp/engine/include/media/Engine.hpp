#pragma once

#include "MediaCapabilitiesSnapshot.hpp"
#include "media/Executor.hpp"

#include <memory>
#include <mutex>
#include <string>

namespace margelo::nitro::transcoder {

// Internal root of the engine. Held lazily by the autolinked `MediaFactory`
// HybridObject: importing the package starts nothing, the first async call
// initializes this once and freezes the backend registry.
class MediaEngine final {
public:
  static MediaEngine& shared();

  MediaEngine(const MediaEngine&) = delete;
  MediaEngine& operator=(const MediaEngine&) = delete;

  // Idempotent, thread-safe. Safe to call concurrently from several first-callers.
  void ensureInitialized();

  Executor& executor();

  // Frozen after `ensureInitialized`, so it never needs hot-path synchronization.
  const MediaCapabilitiesSnapshot& capabilities();

  static std::string version();

private:
  MediaEngine() = default;

  std::once_flag _initFlag;
  std::unique_ptr<Executor> _executor;
  std::unique_ptr<MediaCapabilitiesSnapshot> _capabilities;
};

} // namespace margelo::nitro::transcoder
