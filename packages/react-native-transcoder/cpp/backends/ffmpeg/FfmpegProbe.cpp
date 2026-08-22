#include "FfmpegProbe.hpp"
#include "FfmpegIo.hpp"
#include "FfmpegSupport.hpp"
#include "media/Error.hpp"

namespace margelo::nitro::transcoder::ffmpeg {

namespace {

std::string describeChannelLayout(const AVChannelLayout& layout) {
  std::array<char, 64> buffer{};
  int written = av_channel_layout_describe(&layout, buffer.data(), buffer.size());
  if (written <= 0) return "unknown";
  return std::string(buffer.data());
}

void collectMetadata(AVDictionary* dictionary, std::unordered_map<std::string, std::string>& into) {
  const AVDictionaryEntry* entry = nullptr;
  while ((entry = av_dict_iterate(dictionary, entry)) != nullptr) {
    into.emplace(entry->key, entry->value);
  }
}

} // namespace

ProbeResult probeSource(ByteSource& source) {
  if (source.isSeekable()) source.seek(0);

  ReadBridge bridge(source, nullptr);
  AVFormatContext* rawContext = avformat_alloc_context();
  if (rawContext == nullptr) {
    throw TranscoderException(error_code::INTERNAL_ERROR, error_stage::PROBE, "Cannot allocate a format context.");
  }
  rawContext->pb = bridge.context();
  rawContext->flags |= AVFMT_FLAG_CUSTOM_IO;

  int status = avformat_open_input(&rawContext, nullptr, nullptr, nullptr);
  if (status < 0) {
    if (rawContext != nullptr) avformat_free_context(rawContext);
    throw TranscoderException(error_code::PROBE_FAILED, error_stage::PROBE,
                              "Cannot identify this media: " + describeAvError(status));
  }
  FormatContextPtr context(rawContext);

  status = avformat_find_stream_info(context.get(), nullptr);
  if (status < 0) {
    throw TranscoderException(error_code::PROBE_FAILED, error_stage::PROBE,
                              "Cannot read stream information: " + describeAvError(status));
  }

  ProbeResult result;
  result.containerFormatName = context->iformat != nullptr && context->iformat->name != nullptr
                                   ? std::string(context->iformat->name)
                                   : std::string("unknown");
  result.container = toEngineContainer(result.containerFormatName);
  if (context->duration != AV_NOPTS_VALUE) {
    result.durationSeconds = static_cast<double>(context->duration) / AV_TIME_BASE;
  }
  auto byteLength = source.byteLength();
  if (byteLength.has_value()) result.byteSize = static_cast<double>(*byteLength);

  for (unsigned int i = 0; i < context->nb_streams; i++) {
    const AVStream* stream = context->streams[i];
    const AVCodecParameters* parameters = stream->codecpar;

    if (parameters->codec_type == AVMEDIA_TYPE_VIDEO) {
      // Cover art arrives as an attached picture, not as real video.
      if ((stream->disposition & AV_DISPOSITION_ATTACHED_PIC) != 0) {
        result.artworkCount += 1;
      } else {
        result.hasVideo = true;
      }
      continue;
    }
    if (parameters->codec_type != AVMEDIA_TYPE_AUDIO) continue;

    AudioStreamDescriptor descriptor;
    descriptor.streamId = static_cast<double>(stream->index);
    descriptor.codec = toEngineCodec(parameters->codec_id);
    const AVCodecDescriptor* codecDescriptor = avcodec_descriptor_get(parameters->codec_id);
    descriptor.codecName = codecDescriptor != nullptr ? codecDescriptor->name : "unknown";
    descriptor.sampleRate = static_cast<double>(parameters->sample_rate);
    descriptor.channelCount = static_cast<double>(parameters->ch_layout.nb_channels);
    descriptor.channelLayoutName = describeChannelLayout(parameters->ch_layout);
    descriptor.sampleFormat = toEngineSampleFormat(static_cast<AVSampleFormat>(parameters->format));
    if (parameters->bit_rate > 0) descriptor.bitsPerSecond = static_cast<double>(parameters->bit_rate);
    if (stream->duration != AV_NOPTS_VALUE) {
      descriptor.durationSeconds = static_cast<double>(stream->duration) * av_q2d(stream->time_base);
    } else if (result.durationSeconds.has_value()) {
      descriptor.durationSeconds = result.durationSeconds;
    }
    if (parameters->frame_size > 0) descriptor.frameSize = static_cast<double>(parameters->frame_size);
    descriptor.isDefault = (stream->disposition & AV_DISPOSITION_DEFAULT) != 0;
    AVDictionaryEntry* language = av_dict_get(stream->metadata, "language", nullptr, 0);
    if (language != nullptr) descriptor.language = std::string(language->value);
    if (parameters->initial_padding > 0) {
      descriptor.encoderDelaySamples = static_cast<double>(parameters->initial_padding);
    }
    if (parameters->trailing_padding > 0) {
      descriptor.encoderPaddingSamples = static_cast<double>(parameters->trailing_padding);
    }

    result.audioStreams.push_back(std::move(descriptor));
  }

  if (result.audioStreams.empty()) {
    throw TranscoderException(error_code::PROBE_FAILED, error_stage::PROBE,
                              "This media has no audio stream the engine can use.");
  }

  collectMetadata(context->metadata, result.metadata);

  for (unsigned int i = 0; i < context->nb_chapters; i++) {
    const AVChapter* chapter = context->chapters[i];
    ChapterDescriptor descriptor;
    descriptor.id = static_cast<double>(chapter->id);
    descriptor.startSeconds = static_cast<double>(chapter->start) * av_q2d(chapter->time_base);
    descriptor.endSeconds = static_cast<double>(chapter->end) * av_q2d(chapter->time_base);
    AVDictionaryEntry* title = av_dict_get(chapter->metadata, "title", nullptr, 0);
    if (title != nullptr) descriptor.title = std::string(title->value);
    result.chapters.push_back(std::move(descriptor));
  }

  if (source.isSeekable()) source.seek(0);
  return result;
}

} // namespace margelo::nitro::transcoder::ffmpeg
