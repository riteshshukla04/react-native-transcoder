#include "media/Planner.hpp"
#include "media/Error.hpp"

#include <algorithm>
#include <sstream>

namespace margelo::nitro::transcoder {

namespace {

std::string codecName(AudioCodecId codec) {
  switch (codec) {
  case AudioCodecId::PCM_U8:
    return "pcm-u8";
  case AudioCodecId::PCM_S16:
    return "pcm-s16";
  case AudioCodecId::PCM_S24:
    return "pcm-s24";
  case AudioCodecId::PCM_S32:
    return "pcm-s32";
  case AudioCodecId::PCM_F32:
    return "pcm-f32";
  case AudioCodecId::AAC:
    return "aac";
  case AudioCodecId::HE_AAC:
    return "he-aac";
  case AudioCodecId::HE_AAC_V2:
    return "he-aac-v2";
  case AudioCodecId::MP3:
    return "mp3";
  case AudioCodecId::FLAC:
    return "flac";
  case AudioCodecId::ALAC:
    return "alac";
  case AudioCodecId::OPUS:
    return "opus";
  case AudioCodecId::VORBIS:
    return "vorbis";
  default:
    return "other";
  }
}

const AudioStreamDescriptor& selectStream(const ProbeResult& probe, const AudioTranscodeRequest& request) {
  if (request.tracks.has_value() && !request.tracks->audioStreamIds.empty()) {
    double wanted = request.tracks->audioStreamIds.front();
    auto found = std::find_if(probe.audioStreams.begin(), probe.audioStreams.end(),
                              [wanted](const AudioStreamDescriptor& stream) { return stream.streamId == wanted; });
    if (found == probe.audioStreams.end()) {
      throw TranscoderException(error_code::INVALID_REQUEST, error_stage::PLAN,
                                "The requested audio stream does not exist in this source.");
    }
    return *found;
  }
  auto preferred = std::find_if(probe.audioStreams.begin(), probe.audioStreams.end(),
                                [](const AudioStreamDescriptor& stream) { return stream.isDefault; });
  return preferred != probe.audioStreams.end() ? *preferred : probe.audioStreams.front();
}

int64_t resolveBitRate(const AudioTranscodeRequest& request, AudioCodecId codec, std::vector<PlanWarning>& warnings) {
  if (!request.quality.has_value()) return 0;
  const AudioQualityRequest& quality = *request.quality;

  switch (quality.mode) {
  case QualityMode::BITRATE:
    if (!quality.bitsPerSecond.has_value()) {
      throw TranscoderException(error_code::INVALID_REQUEST, error_stage::PLAN,
                                "quality.mode 'bitrate' requires quality.bitsPerSecond.");
    }
    if (codec == AudioCodecId::FLAC || codec == AudioCodecId::ALAC || codecName(codec).rfind("pcm-", 0) == 0) {
      warnings.push_back(PlanWarning(PlanWarningCode::PREFERENCE_IGNORED,
                                     "A target bit rate has no meaning for a lossless codec and was ignored.",
                                     std::nullopt));
      return 0;
    }
    return static_cast<int64_t>(*quality.bitsPerSecond);
  case QualityMode::QUALITY:
    if (!quality.level.has_value()) {
      throw TranscoderException(error_code::INVALID_REQUEST, error_stage::PLAN,
                                "quality.mode 'quality' requires quality.level.");
    }
    warnings.push_back(PlanWarning(PlanWarningCode::PREFERENCE_IGNORED,
                                   "Encoder-relative quality levels are not wired up yet; the encoder default "
                                   "bit rate was used instead.",
                                   std::nullopt));
    return 0;
  case QualityMode::LOSSLESS:
    return 0;
  case QualityMode::COPY:
    throw TranscoderException(error_code::INVALID_REQUEST, error_stage::PLAN,
                              "quality.mode 'copy' is only valid with audio.mode 'copy'.");
  default:
    return 0;
  }
}

} // namespace

PlanData resolvePlan(const ProbeResult& probe, const TranscodeRequest& request) {
  const AudioTranscodeRequest& audio = request.audio;
  const AudioStreamDescriptor& stream = selectStream(probe, audio);

  PlanData plan;
  plan.selectedStreamIds.push_back(stream.streamId);
  plan.sourceStreamIndex = static_cast<int>(stream.streamId);
  plan.outputContainer = audio.container;
  plan.sourceDurationSeconds = stream.durationSeconds.has_value() ? stream.durationSeconds : probe.durationSeconds;
  plan.metadataPolicy = request.metadata.value_or(MetadataPolicy::COPY);
  if (request.metadataValues.has_value()) {
    for (const auto& [key, value] : *request.metadataValues)
      plan.metadataValues.emplace(key, value);
  }
  if ((plan.metadataPolicy == MetadataPolicy::REPLACE || plan.metadataPolicy == MetadataPolicy::MERGE) &&
      plan.metadataValues.empty()) {
    throw TranscoderException(error_code::INVALID_REQUEST, error_stage::PLAN,
                              "metadata 'replace' and 'merge' need metadataValues.");
  }

  if (request.outcome.has_value() && *request.outcome == OutcomePolicy::LOWEST_ENERGY) {
    plan.warnings.push_back(PlanWarning(PlanWarningCode::ENERGY_POLICY_UNAVAILABLE,
                                        "No validated energy measurements exist for this device class, so "
                                        "'lowest-energy' resolved through 'balanced'.",
                                        std::nullopt));
  }

  if (audio.mode == AudioRequestMode::COPY) {
    if (!stream.codec.has_value()) {
      throw TranscoderException(error_code::UNSUPPORTED_COMBINATION, error_stage::PLAN,
                                "The source codec has no stable engine identifier, so it cannot be copied.");
    }
    if (audio.quality.has_value() && audio.quality->mode != QualityMode::COPY) {
      throw TranscoderException(error_code::INVALID_REQUEST, error_stage::PLAN,
                                "audio.mode 'copy' only accepts quality.mode 'copy'.");
    }
    plan.path = plan.metadataPolicy == MetadataPolicy::COPY ? PlanPath::PACKET_REMUX : PlanPath::METADATA_REMUX;
    plan.outputCodec = stream.codec;
    plan.outputSampleRate = stream.sampleRate;
    plan.outputChannelCount = stream.channelCount;
    plan.outputBitsPerSecond = stream.bitsPerSecond;
    return plan;
  }

  AudioCodecId targetCodec;
  if (audio.mode == AudioRequestMode::DECODE) {
    targetCodec = AudioCodecId::PCM_S16;
  } else {
    if (!audio.codec.has_value()) {
      throw TranscoderException(error_code::INVALID_REQUEST, error_stage::PLAN,
                                "audio.mode 'encode' requires audio.codec.");
    }
    targetCodec = *audio.codec;
  }

  int targetSampleRate = static_cast<int>(stream.sampleRate);
  if (audio.sampleRate.has_value() && audio.sampleRate->policy != ValuePolicy::PRESERVE) {
    if (!audio.sampleRate->hertz.has_value()) {
      throw TranscoderException(error_code::INVALID_REQUEST, error_stage::PLAN,
                                "sampleRate policy 'prefer' and 'require' need sampleRate.hertz.");
    }
    targetSampleRate = static_cast<int>(*audio.sampleRate->hertz);
    if (targetSampleRate != static_cast<int>(stream.sampleRate)) {
      plan.conversions.push_back(PlanConversion(
          ProcessorId::RESAMPLE, "Requested output sample rate differs from the source.", stream.streamId));
    }
  }

  int targetChannels = static_cast<int>(stream.channelCount);
  if (audio.channels.has_value() && audio.channels->policy != ValuePolicy::PRESERVE) {
    if (!audio.channels->channelCount.has_value()) {
      throw TranscoderException(error_code::INVALID_REQUEST, error_stage::PLAN,
                                "channels policy 'prefer' and 'require' need channels.channelCount.");
    }
    targetChannels = static_cast<int>(*audio.channels->channelCount);
    if (targetChannels != static_cast<int>(stream.channelCount)) {
      plan.conversions.push_back(PlanConversion(ProcessorId::CHANNEL_MAP,
                                                "Requested channel count differs from the source.", stream.streamId));
    }
  }

  plan.path = PlanPath::FULL_TRANSCODE;
  plan.outputCodec = targetCodec;
  plan.targetSampleRate = targetSampleRate;
  plan.targetChannelCount = targetChannels;
  plan.targetBitRate = resolveBitRate(audio, targetCodec, plan.warnings);
  plan.outputSampleRate = static_cast<double>(targetSampleRate);
  plan.outputChannelCount = static_cast<double>(targetChannels);
  if (plan.targetBitRate > 0) plan.outputBitsPerSecond = static_cast<double>(plan.targetBitRate);
  if (plan.sourceDurationSeconds.has_value() && plan.targetBitRate > 0) {
    plan.estimatedOutputByteSize = *plan.sourceDurationSeconds * static_cast<double>(plan.targetBitRate) / 8.0;
  }
  plan.conversions.push_back(PlanConversion(ProcessorId::SAMPLE_FORMAT_CONVERT,
                                            "The encoder requires its own sample format.", stream.streamId));

  return plan;
}

std::string planToJson(const PlanData& plan) {
  std::ostringstream out;
  auto pathName = [](PlanPath path) {
    switch (path) {
    case PlanPath::PROBE_ONLY:
      return "probe-only";
    case PlanPath::METADATA_REMUX:
      return "metadata-remux";
    case PlanPath::PACKET_REMUX:
      return "packet-remux";
    case PlanPath::DECODE_ONLY:
      return "decode-only";
    case PlanPath::ENCODE_ONLY:
      return "encode-only";
    default:
      return "full-transcode";
    }
  };

  out << "{\"path\":\"" << pathName(plan.path) << "\"";
  out << ",\"selectedStreamIds\":[";
  for (std::size_t i = 0; i < plan.selectedStreamIds.size(); i++) {
    if (i > 0) out << ",";
    out << static_cast<long long>(plan.selectedStreamIds[i]);
  }
  out << "]";
  if (plan.outputCodec.has_value()) out << ",\"outputCodec\":\"" << codecName(*plan.outputCodec) << "\"";
  if (plan.outputSampleRate.has_value()) out << ",\"outputSampleRate\":" << *plan.outputSampleRate;
  if (plan.outputChannelCount.has_value()) out << ",\"outputChannelCount\":" << *plan.outputChannelCount;
  if (plan.outputBitsPerSecond.has_value()) out << ",\"outputBitsPerSecond\":" << *plan.outputBitsPerSecond;
  out << ",\"conversions\":" << plan.conversions.size();
  out << ",\"warnings\":" << plan.warnings.size();
  out << ",\"requiresSourceStaging\":" << (plan.requiresSourceStaging ? "true" : "false");
  out << ",\"isTwoPass\":" << (plan.isTwoPass ? "true" : "false");
  out << "}";
  return out.str();
}

} // namespace margelo::nitro::transcoder
