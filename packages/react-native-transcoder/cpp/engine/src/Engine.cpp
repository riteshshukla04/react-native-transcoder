#include "media/Engine.hpp"
#include "backends/ffmpeg/FfmpegTranscoder.hpp"
#include "media/Error.hpp"

#include <algorithm>
#include <thread>

#ifndef TRANSCODER_VERSION
#define TRANSCODER_VERSION "0.1.0"
#endif

#ifndef TRANSCODER_PROFILE
#define TRANSCODER_PROFILE "mobile-core"
#endif

#ifndef TRANSCODER_CAPABILITY_MANIFEST_HASH
#define TRANSCODER_CAPABILITY_MANIFEST_HASH "unlinked"
#endif

namespace margelo::nitro::transcoder {

namespace {

std::size_t defaultWorkerCount() {
  unsigned int hardware = std::thread::hardware_concurrency();
  if (hardware == 0) hardware = 2;
  // Media work is CPU-heavy; leave headroom for JS, UI and the platform.
  return std::clamp<std::size_t>(hardware / 2, 1, 4);
}

MediaCapabilitiesSnapshot buildCapabilities(std::size_t workerCount) {
  std::vector<CodecCapability> decoders;
  std::vector<CodecCapability> encoders;
  std::vector<ContainerCapability> containers;
  ffmpeg::enumerateCapabilities(decoders, encoders, containers);

  return MediaCapabilitiesSnapshot(
      /* engineVersion */ TRANSCODER_VERSION,
      /* profile */ TRANSCODER_PROFILE,
      /* capabilityManifestHash */ TRANSCODER_CAPABILITY_MANIFEST_HASH,
      /* audioDecoders */ std::move(decoders),
      /* audioEncoders */ std::move(encoders),
      /* containers */ std::move(containers),
      /* processors */
      {ProcessorId::SAMPLE_FORMAT_CONVERT, ProcessorId::RESAMPLE, ProcessorId::CHANNEL_MAP},
      /* outcomes */
      {OutcomePolicy::BALANCED, OutcomePolicy::FASTEST, OutcomePolicy::HIGHEST_QUALITY, OutcomePolicy::LOWEST_ENERGY,
       OutcomePolicy::DETERMINISTIC},
      /* energyPolicyAvailable */ false,
      /* maxConcurrentJobs */ static_cast<double>(workerCount),
      /* supportsVideo */ false);
}

} // namespace

MediaEngine& MediaEngine::shared() {
  static MediaEngine instance;
  return instance;
}

void MediaEngine::ensureInitialized() {
  std::call_once(_initFlag, [this] {
    std::size_t workerCount = defaultWorkerCount();
    _executor = std::make_unique<Executor>(workerCount);
    _capabilities = std::make_unique<MediaCapabilitiesSnapshot>(buildCapabilities(workerCount));
  });
}

Executor& MediaEngine::executor() {
  ensureInitialized();
  return *_executor;
}

const MediaCapabilitiesSnapshot& MediaEngine::capabilities() {
  ensureInitialized();
  return *_capabilities;
}

std::string MediaEngine::version() {
  return TRANSCODER_VERSION;
}

} // namespace margelo::nitro::transcoder
