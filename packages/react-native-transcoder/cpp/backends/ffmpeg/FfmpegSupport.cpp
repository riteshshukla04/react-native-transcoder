#include "FfmpegSupport.hpp"

#include <array>
#include <cstring>

namespace margelo::nitro::transcoder::ffmpeg {

void FormatContextDeleter::operator()(AVFormatContext* context) const noexcept {
  // The AVIOContext is owned by ReadBridge/WriteBridge, not by FFmpeg.
  avformat_close_input(&context);
}

void OutputFormatContextDeleter::operator()(AVFormatContext* context) const noexcept {
  avformat_free_context(context);
}

std::string describeAvError(int errorCode) {
  std::array<char, AV_ERROR_MAX_STRING_SIZE> buffer{};
  av_strerror(errorCode, buffer.data(), buffer.size());
  return std::string(buffer.data());
}

namespace {

struct CodecPair {
  AudioCodecId engine;
  AVCodecID av;
};

constexpr std::array<CodecPair, 20> CODEC_TABLE{{
    {AudioCodecId::PCM_U8, AV_CODEC_ID_PCM_U8},     {AudioCodecId::PCM_S16, AV_CODEC_ID_PCM_S16LE},
    {AudioCodecId::PCM_S24, AV_CODEC_ID_PCM_S24LE}, {AudioCodecId::PCM_S32, AV_CODEC_ID_PCM_S32LE},
    {AudioCodecId::PCM_F32, AV_CODEC_ID_PCM_F32LE}, {AudioCodecId::AAC, AV_CODEC_ID_AAC},
    {AudioCodecId::MP3, AV_CODEC_ID_MP3},           {AudioCodecId::FLAC, AV_CODEC_ID_FLAC},
    {AudioCodecId::ALAC, AV_CODEC_ID_ALAC},         {AudioCodecId::OPUS, AV_CODEC_ID_OPUS},
    {AudioCodecId::VORBIS, AV_CODEC_ID_VORBIS},     {AudioCodecId::AMR_NB, AV_CODEC_ID_AMR_NB},
    {AudioCodecId::AMR_WB, AV_CODEC_ID_AMR_WB},     {AudioCodecId::AC3, AV_CODEC_ID_AC3},
    {AudioCodecId::EAC3, AV_CODEC_ID_EAC3},         {AudioCodecId::WAVPACK, AV_CODEC_ID_WAVPACK},
    {AudioCodecId::APE, AV_CODEC_ID_APE},           {AudioCodecId::MUSEPACK, AV_CODEC_ID_MUSEPACK8},
    {AudioCodecId::SPEEX, AV_CODEC_ID_SPEEX},       {AudioCodecId::DTS, AV_CODEC_ID_DTS},
}};

} // namespace

std::optional<AudioCodecId> toEngineCodec(AVCodecID codecId) {
  // Big-endian and packed PCM variants all present as their little-endian id.
  switch (codecId) {
  case AV_CODEC_ID_PCM_S16BE:
    return AudioCodecId::PCM_S16;
  case AV_CODEC_ID_PCM_S24BE:
    return AudioCodecId::PCM_S24;
  case AV_CODEC_ID_PCM_S32BE:
    return AudioCodecId::PCM_S32;
  case AV_CODEC_ID_PCM_F32BE:
    return AudioCodecId::PCM_F32;
  default:
    break;
  }
  for (const auto& pair : CODEC_TABLE) {
    if (pair.av == codecId) return pair.engine;
  }
  return std::nullopt;
}

AVCodecID toAvCodec(AudioCodecId codec) {
  for (const auto& pair : CODEC_TABLE) {
    if (pair.engine == codec) return pair.av;
  }
  return AV_CODEC_ID_NONE;
}

const char* toMuxerName(ContainerId container) {
  switch (container) {
  case ContainerId::WAV:
    return "wav";
  case ContainerId::AIFF:
    return "aiff";
  case ContainerId::CAF:
    return "caf";
  case ContainerId::RAW_PCM:
    return "s16le";
  case ContainerId::M4A:
    return "ipod";
  case ContainerId::MP4:
    return "mp4";
  case ContainerId::ADTS:
    return "adts";
  case ContainerId::MP3:
    return "mp3";
  case ContainerId::FLAC:
    return "flac";
  case ContainerId::OGG:
    return "ogg";
  case ContainerId::WEBM:
    return "webm";
  case ContainerId::MATROSKA:
    return "matroska";
  case ContainerId::AMR:
    return "amr";
  case ContainerId::AC3:
    return "ac3";
  case ContainerId::WAVPACK:
    return "wv";
  case ContainerId::APE:
    return "ape";
  case ContainerId::MUSEPACK:
    return "mpc";
  case ContainerId::ASF:
    return "asf";
  case ContainerId::RF64:
    return "w64";
  case ContainerId::MPEGTS:
    return "mpegts";
  default:
    return nullptr;
  }
}

std::optional<ContainerId> toEngineContainer(const std::string& formatName) {
  // `iformat->name` is a comma-separated list for shared demuxers.
  auto contains = [&](const char* needle) { return formatName.find(needle) != std::string::npos; };
  if (contains("wav")) return ContainerId::WAV;
  if (contains("aiff")) return ContainerId::AIFF;
  if (contains("caf")) return ContainerId::CAF;
  if (contains("mp3")) return ContainerId::MP3;
  if (contains("flac")) return ContainerId::FLAC;
  if (contains("ogg")) return ContainerId::OGG;
  // The Matroska demuxer reports "matroska,webm" for both, so WebM sources also
  // resolve to the canonical Matroska id.
  if (contains("matroska")) return ContainerId::MATROSKA;
  if (contains("webm")) return ContainerId::WEBM;
  if (contains("adts") || contains("aac")) return ContainerId::ADTS;
  if (contains("mov") || contains("mp4") || contains("m4a") || contains("ipod")) return ContainerId::M4A;
  if (contains("w64")) return ContainerId::RF64;
  return std::nullopt;
}

std::optional<SampleFormat> toEngineSampleFormat(AVSampleFormat format) {
  switch (format) {
  case AV_SAMPLE_FMT_U8:
    return SampleFormat::U8;
  case AV_SAMPLE_FMT_S16:
    return SampleFormat::S16;
  case AV_SAMPLE_FMT_S32:
    return SampleFormat::S32;
  case AV_SAMPLE_FMT_FLT:
    return SampleFormat::F32;
  case AV_SAMPLE_FMT_DBL:
    return SampleFormat::F64;
  case AV_SAMPLE_FMT_U8P:
    return SampleFormat::U8_PLANAR;
  case AV_SAMPLE_FMT_S16P:
    return SampleFormat::S16_PLANAR;
  case AV_SAMPLE_FMT_S32P:
    return SampleFormat::S32_PLANAR;
  case AV_SAMPLE_FMT_FLTP:
    return SampleFormat::F32_PLANAR;
  case AV_SAMPLE_FMT_DBLP:
    return SampleFormat::F64_PLANAR;
  default:
    return std::nullopt;
  }
}

} // namespace margelo::nitro::transcoder::ffmpeg
