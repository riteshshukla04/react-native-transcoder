#pragma once

#include "AudioCodecId.hpp"
#include "AudioStreamDescriptor.hpp"
#include "ChapterDescriptor.hpp"
#include "ContainerId.hpp"
#include "JobPhase.hpp"
#include "MetadataPolicy.hpp"
#include "PlanConversion.hpp"
#include "PlanPath.hpp"
#include "PlanWarning.hpp"

#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace margelo::nitro::transcoder {

// Everything a probe learned about a source. Cached once per MediaSource.
struct ProbeResult {
  std::optional<double> durationSeconds;
  std::optional<ContainerId> container;
  std::string containerFormatName;
  std::optional<double> byteSize;
  std::vector<AudioStreamDescriptor> audioStreams;
  bool hasVideo = false;
  std::vector<ChapterDescriptor> chapters;
  std::unordered_map<std::string, std::string> metadata;
  double artworkCount = 0;
};

// The resolved plan, plus the execution detail the backend needs. The public
// `ResolvedPlan` HybridObject exposes only the first half.
struct PlanData {
  PlanPath path = PlanPath::FULL_TRANSCODE;
  std::vector<double> selectedStreamIds;
  ContainerId outputContainer = ContainerId::WAV;
  std::optional<AudioCodecId> outputCodec;
  std::optional<double> outputSampleRate;
  std::optional<double> outputChannelCount;
  std::optional<double> outputBitsPerSecond;
  std::optional<double> estimatedOutputByteSize;
  std::vector<PlanConversion> conversions;
  std::vector<PlanWarning> warnings;
  bool requiresSourceStaging = false;
  std::optional<double> estimatedStagingByteSize;
  bool isTwoPass = false;

  // Execution detail, never exposed to JavaScript.
  int sourceStreamIndex = 0;
  int targetSampleRate = 0;
  int targetChannelCount = 0;
  int64_t targetBitRate = 0;
  MetadataPolicy metadataPolicy = MetadataPolicy::COPY;
  std::unordered_map<std::string, std::string> metadataValues;
  std::optional<double> sourceDurationSeconds;
};

struct ProgressSnapshot {
  JobPhase phase = JobPhase::STARTING;
  double inputSecondsProcessed = 0;
  std::optional<double> totalInputSeconds;
  double inputBytesRead = 0;
  double outputBytesWritten = 0;
  std::optional<double> speedRatio;
  std::optional<double> estimatedSecondsRemaining;
};

struct ReportData {
  std::optional<std::string> outputUri;
  double outputByteSize = 0;
  std::optional<double> outputDurationSeconds;
  ContainerId outputContainer = ContainerId::WAV;
  std::optional<AudioCodecId> outputCodec;
  std::optional<double> outputSampleRate;
  std::optional<double> outputChannelCount;
  double elapsedSeconds = 0;
  double speedRatio = 0;
  double inputBytesRead = 0;
  double samplesProcessed = 0;
  std::vector<PlanWarning> warnings;
  bool wasCancelled = false;
  bool didCommitOutput = false;
};

} // namespace margelo::nitro::transcoder
