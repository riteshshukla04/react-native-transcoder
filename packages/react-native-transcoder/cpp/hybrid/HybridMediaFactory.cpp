#include "HybridMediaFactory.hpp"
#include "EngineTask.hpp"
#include "HybridMediaAsset.hpp"
#include "HybridMediaDestination.hpp"
#include "HybridMediaSource.hpp"
#include "HybridResolvedPlan.hpp"
#include "HybridTranscodeJob.hpp"
#include "media/ByteIo.hpp"
#include "media/Engine.hpp"
#include "media/Error.hpp"
#include "media/Paths.hpp"
#include "media/Planner.hpp"

#include <cstring>

namespace margelo::nitro::transcoder {

namespace {

// Default ceiling for `openMemorySource`. Opening temporarily retains roughly
// twice the input bytes, so this stays deliberately conservative.
constexpr std::size_t MAX_MEMORY_SOURCE_BYTES = 64ULL * 1024 * 1024;

std::shared_ptr<HybridMediaSource> requireSource(const std::shared_ptr<HybridMediaSourceSpec>& source,
                                                 const char* stage) {
  auto typed = std::dynamic_pointer_cast<HybridMediaSource>(source);
  if (typed == nullptr) {
    throw TranscoderException(error_code::INVALID_REQUEST, stage, "A valid MediaSource is required.");
  }
  return typed;
}

std::shared_ptr<HybridMediaDestination>
requireDestination(const std::shared_ptr<HybridMediaDestinationSpec>& destination, const char* stage) {
  auto typed = std::dynamic_pointer_cast<HybridMediaDestination>(destination);
  if (typed == nullptr) {
    throw TranscoderException(error_code::INVALID_REQUEST, stage, "A valid MediaDestination is required.");
  }
  return typed;
}

} // namespace

std::string HybridMediaFactory::getVersion() {
  return MediaEngine::version();
}

std::string HybridMediaFactory::getScratchDirectory() {
  return scratchDirectory();
}

std::shared_ptr<Promise<void>> HybridMediaFactory::prewarm() {
  return runOnEngine<void>([] { MediaEngine::shared().ensureInitialized(); });
}

std::shared_ptr<Promise<MediaCapabilitiesSnapshot>> HybridMediaFactory::getCapabilities() {
  return runOnEngine<MediaCapabilitiesSnapshot>([] { return MediaEngine::shared().capabilities(); });
}

std::shared_ptr<Promise<std::shared_ptr<HybridMediaSourceSpec>>>
HybridMediaFactory::openFileSource(const FileSourceOptions& options) {
  std::string uri = options.uri;
  return runOnEngine<std::shared_ptr<HybridMediaSourceSpec>>([uri]() -> std::shared_ptr<HybridMediaSourceSpec> {
    std::string path = normalizeFileUri(uri);
    if (path.rfind("content://", 0) == 0) {
      throw TranscoderException(error_code::UNSUPPORTED_COMBINATION, error_stage::OPEN,
                                "`content://` sources are not supported yet.");
    }
    auto byteSource = std::make_unique<FileByteSource>(path);
    return std::make_shared<HybridMediaSource>(MediaSourceKind::FILE, std::move(byteSource), path);
  });
}

std::shared_ptr<Promise<std::shared_ptr<HybridMediaSourceSpec>>>
HybridMediaFactory::openUrlSource(const UrlSourceOptions& options) {
  std::string url = options.url;
  return runOnEngine<std::shared_ptr<HybridMediaSourceSpec>>([url]() -> std::shared_ptr<HybridMediaSourceSpec> {
    if (url.rfind("http://", 0) != 0 && url.rfind("https://", 0) != 0) {
      throw TranscoderException(error_code::INVALID_REQUEST, error_stage::OPEN,
                                "openUrlSource only accepts http:// or https:// URLs.");
    }
    throw TranscoderException(error_code::CAPABILITY_NOT_MET, error_stage::OPEN,
                              "URL sources are not implemented yet — the platform network backends land with the "
                              "media pipeline.");
  });
}

std::shared_ptr<HybridMediaSourceSpec>
HybridMediaFactory::openMemorySource(const std::shared_ptr<ArrayBuffer>& buffer) {
  if (buffer == nullptr) {
    throw TranscoderException(error_code::INVALID_REQUEST, error_stage::OPEN, "Memory source buffer is null.");
  }
  std::size_t byteCount = buffer->size();
  if (byteCount > MAX_MEMORY_SOURCE_BYTES) {
    throw TranscoderException(error_code::RESOURCE_LIMIT_EXCEEDED, error_stage::OPEN,
                              "Memory source is larger than the 64 MiB input limit.");
  }
  // Copied into native-owned immutable memory, so later JS mutations never
  // affect the source.
  std::vector<uint8_t> bytes(byteCount);
  if (byteCount > 0) std::memcpy(bytes.data(), buffer->data(), byteCount);
  auto byteSource = std::make_unique<MemoryByteSource>(std::move(bytes));
  return std::make_shared<HybridMediaSource>(MediaSourceKind::MEMORY, std::move(byteSource), std::nullopt);
}

std::shared_ptr<Promise<std::shared_ptr<HybridMediaDestinationSpec>>>
HybridMediaFactory::openFileDestination(const FileDestinationOptions& options) {
  std::string uri = options.uri;
  OverwritePolicy overwrite = options.overwrite.value_or(OverwritePolicy::FAIL_IF_EXISTS);
  return runOnEngine<std::shared_ptr<HybridMediaDestinationSpec>>(
      [uri, overwrite]() -> std::shared_ptr<HybridMediaDestinationSpec> {
        if (overwrite == OverwritePolicy::NOT_GUARANTEED) {
          throw TranscoderException(error_code::UNSUPPORTED_COMBINATION, error_stage::OPEN,
                                    "`not-guaranteed` atomicity has no destination type yet.");
        }
        std::string path = normalizeFileUri(uri);
        auto byteSink = std::make_unique<AtomicFileByteSink>(path, overwrite == OverwritePolicy::REPLACE_ATOMICALLY);
        return std::make_shared<HybridMediaDestination>(MediaDestinationKind::FILE, overwrite, std::move(byteSink),
                                                        path);
      });
}
std::shared_ptr<Promise<std::shared_ptr<HybridMediaAssetSpec>>>
HybridMediaFactory::probe(const std::shared_ptr<HybridMediaSourceSpec>& source) {
  return runOnEngine<std::shared_ptr<HybridMediaAssetSpec>>([source]() -> std::shared_ptr<HybridMediaAssetSpec> {
    auto typed = requireSource(source, error_stage::PROBE);
    return std::make_shared<HybridMediaAsset>(typed->probeRecord());
  });
}

std::shared_ptr<Promise<std::shared_ptr<HybridResolvedPlanSpec>>>
HybridMediaFactory::resolve(const std::shared_ptr<HybridMediaSourceSpec>& source,
                            const std::shared_ptr<HybridMediaDestinationSpec>& destination,
                            const TranscodeRequest& request) {
  return runOnEngine<std::shared_ptr<HybridResolvedPlanSpec>>(
      [source, destination, request]() -> std::shared_ptr<HybridResolvedPlanSpec> {
        auto typedSource = requireSource(source, error_stage::PLAN);
        requireDestination(destination, error_stage::PLAN);
        return std::make_shared<HybridResolvedPlan>(resolvePlan(typedSource->probeRecord(), request));
      });
}

std::shared_ptr<Promise<std::shared_ptr<HybridTranscodeJobSpec>>>
HybridMediaFactory::createTranscodeJob(const std::shared_ptr<HybridMediaSourceSpec>& source,
                                       const std::shared_ptr<HybridMediaDestinationSpec>& destination,
                                       const TranscodeRequest& request) {
  return runOnEngine<std::shared_ptr<HybridTranscodeJobSpec>>(
      [source, destination, request]() -> std::shared_ptr<HybridTranscodeJobSpec> {
        auto typedSource = requireSource(source, error_stage::PLAN);
        auto typedDestination = requireDestination(destination, error_stage::PLAN);
        // The same planner, then the same validated construction path as createTranscodeJobFromPlan.
        PlanData plan = resolvePlan(typedSource->probeRecord(), request);
        return std::make_shared<HybridTranscodeJob>(typedSource, typedDestination, std::move(plan));
      });
}

std::shared_ptr<Promise<std::shared_ptr<HybridTranscodeJobSpec>>>
HybridMediaFactory::createTranscodeJobFromPlan(const std::shared_ptr<HybridMediaSourceSpec>& source,
                                               const std::shared_ptr<HybridMediaDestinationSpec>& destination,
                                               const std::shared_ptr<HybridResolvedPlanSpec>& plan) {
  return runOnEngine<std::shared_ptr<HybridTranscodeJobSpec>>([source, destination,
                                                               plan]() -> std::shared_ptr<HybridTranscodeJobSpec> {
    auto typedSource = requireSource(source, error_stage::PLAN);
    auto typedDestination = requireDestination(destination, error_stage::PLAN);
    auto typedPlan = std::dynamic_pointer_cast<HybridResolvedPlan>(plan);
    if (typedPlan == nullptr) {
      throw TranscoderException(error_code::INVALID_REQUEST, error_stage::PLAN, "A valid ResolvedPlan is required.");
    }
    return std::make_shared<HybridTranscodeJob>(typedSource, typedDestination, typedPlan->plan());
  });
}

} // namespace margelo::nitro::transcoder
