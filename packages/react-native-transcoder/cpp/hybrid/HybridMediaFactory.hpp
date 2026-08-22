#pragma once

#include "HybridMediaFactorySpec.hpp"

namespace margelo::nitro::transcoder {

// The one autolinked HybridObject. Its constructor is trivial on purpose:
// importing the package must not start workers or enumerate codecs.
class HybridMediaFactory final : public HybridMediaFactorySpec {
public:
  HybridMediaFactory() : HybridObject(TAG) {}

  std::string getVersion() override;
  std::string getScratchDirectory() override;

  std::shared_ptr<Promise<void>> prewarm() override;
  std::shared_ptr<Promise<MediaCapabilitiesSnapshot>> getCapabilities() override;

  std::shared_ptr<Promise<std::shared_ptr<HybridMediaSourceSpec>>>
  openFileSource(const FileSourceOptions& options) override;
  std::shared_ptr<Promise<std::shared_ptr<HybridMediaSourceSpec>>>
  openUrlSource(const UrlSourceOptions& options) override;
  std::shared_ptr<HybridMediaSourceSpec> openMemorySource(const std::shared_ptr<ArrayBuffer>& buffer) override;
  std::shared_ptr<Promise<std::shared_ptr<HybridMediaDestinationSpec>>>
  openFileDestination(const FileDestinationOptions& options) override;

  std::shared_ptr<Promise<std::shared_ptr<HybridMediaAssetSpec>>>
  probe(const std::shared_ptr<HybridMediaSourceSpec>& source) override;
  std::shared_ptr<Promise<std::shared_ptr<HybridResolvedPlanSpec>>>
  resolve(const std::shared_ptr<HybridMediaSourceSpec>& source,
          const std::shared_ptr<HybridMediaDestinationSpec>& destination, const TranscodeRequest& request) override;
  std::shared_ptr<Promise<std::shared_ptr<HybridTranscodeJobSpec>>>
  createTranscodeJob(const std::shared_ptr<HybridMediaSourceSpec>& source,
                     const std::shared_ptr<HybridMediaDestinationSpec>& destination,
                     const TranscodeRequest& request) override;
  std::shared_ptr<Promise<std::shared_ptr<HybridTranscodeJobSpec>>>
  createTranscodeJobFromPlan(const std::shared_ptr<HybridMediaSourceSpec>& source,
                             const std::shared_ptr<HybridMediaDestinationSpec>& destination,
                             const std::shared_ptr<HybridResolvedPlanSpec>& plan) override;
};

} // namespace margelo::nitro::transcoder
