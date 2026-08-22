#pragma once

#include "HybridMediaAssetSpec.hpp"
#include "media/MediaModel.hpp"

namespace margelo::nitro::transcoder {

class HybridMediaAsset final : public HybridMediaAssetSpec {
public:
  explicit HybridMediaAsset(ProbeResult probe) : HybridObject(TAG), _probe(std::move(probe)) {}

  std::optional<double> getDurationSeconds() override { return _probe.durationSeconds; }
  std::optional<ContainerId> getContainer() override { return _probe.container; }
  std::string getContainerFormatName() override { return _probe.containerFormatName; }
  std::optional<double> getByteSize() override { return _probe.byteSize; }
  std::vector<AudioStreamDescriptor> getAudioStreams() override { return _probe.audioStreams; }
  bool getHasVideo() override { return _probe.hasVideo; }
  std::vector<ChapterDescriptor> getChapters() override { return _probe.chapters; }
  double getArtworkCount() override { return _probe.artworkCount; }

  std::unordered_map<std::string, std::string> getMetadata() override { return _probe.metadata; }
  std::shared_ptr<Promise<std::string>> saveArtworkToFile(double index, const std::string& path) override;
  void close() override;

private:
  ProbeResult _probe;
};

} // namespace margelo::nitro::transcoder
