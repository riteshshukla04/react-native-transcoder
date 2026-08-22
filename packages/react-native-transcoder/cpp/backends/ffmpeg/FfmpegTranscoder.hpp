#pragma once

#include "CodecCapability.hpp"
#include "ContainerCapability.hpp"
#include "media/ByteIo.hpp"
#include "media/CancellationToken.hpp"
#include "media/MediaModel.hpp"

#include <functional>

namespace margelo::nitro::transcoder::ffmpeg {

using ProgressCallback = std::function<void(const ProgressSnapshot&)>;

// Executes a resolved plan. Throws `TranscoderException` on failure; a
// cancelled run returns a partial report with `wasCancelled` set.
ReportData runPlan(ByteSource& source, ByteSink& sink, const PlanData& plan, const CancellationTokenRef& cancellation,
                   const ProgressCallback& onProgress);

// Codecs and containers this build can actually run, discovered at runtime.
void enumerateCapabilities(std::vector<CodecCapability>& decoders, std::vector<CodecCapability>& encoders,
                           std::vector<ContainerCapability>& containers);

} // namespace margelo::nitro::transcoder::ffmpeg
