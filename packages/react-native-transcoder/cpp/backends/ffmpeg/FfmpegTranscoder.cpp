#include "FfmpegTranscoder.hpp"
#include "FfmpegIo.hpp"
#include "FfmpegSupport.hpp"
#include "media/Error.hpp"

#include <algorithm>
#include <chrono>
#include <cstring>

namespace margelo::nitro::transcoder::ffmpeg {

namespace {

constexpr int DEFAULT_FRAME_SIZE = 4096;

[[noreturn]] void fail(const char* code, const char* stage, const std::string& message, int status) {
  throw TranscoderException(code, stage, message + ": " + describeAvError(status));
}

void checkCancelled(const CancellationTokenRef& cancellation) {
  if (cancellation != nullptr && cancellation->isCancelled()) {
    throw TranscoderException(error_code::CANCELLED, error_stage::ENGINE, "The job was cancelled.");
  }
}

FormatContextPtr openInput(ReadBridge& bridge) {
  AVFormatContext* raw = avformat_alloc_context();
  if (raw == nullptr) {
    throw TranscoderException(error_code::INTERNAL_ERROR, error_stage::DEMUX, "Cannot allocate a format context.");
  }
  raw->pb = bridge.context();
  raw->flags |= AVFMT_FLAG_CUSTOM_IO;

  int status = avformat_open_input(&raw, nullptr, nullptr, nullptr);
  if (status < 0) {
    if (raw != nullptr) avformat_free_context(raw);
    fail(error_code::PROBE_FAILED, error_stage::DEMUX, "Cannot open the source", status);
  }
  FormatContextPtr context(raw);
  status = avformat_find_stream_info(context.get(), nullptr);
  if (status < 0) fail(error_code::PROBE_FAILED, error_stage::DEMUX, "Cannot read stream information", status);
  return context;
}

OutputFormatContextPtr openOutput(WriteBridge& bridge, const char* muxerName) {
  AVFormatContext* raw = nullptr;
  int status = avformat_alloc_output_context2(&raw, nullptr, muxerName, nullptr);
  if (status < 0 || raw == nullptr) {
    fail(error_code::UNSUPPORTED_COMBINATION, error_stage::MUX,
         std::string("No muxer available for container '") + muxerName + "'", status);
  }
  raw->pb = bridge.context();
  raw->flags |= AVFMT_FLAG_CUSTOM_IO;
  return OutputFormatContextPtr(raw);
}

template<typename T>
std::vector<T> supportedConfig(const AVCodec* codec, AVCodecConfig config) {
  const T* values = nullptr;
  int count = 0;
  int status = avcodec_get_supported_config(nullptr, codec, config, 0, reinterpret_cast<const void**>(&values), &count);
  if (status < 0 || values == nullptr || count <= 0) return {};
  return std::vector<T>(values, values + count);
}

AVSampleFormat chooseSampleFormat(const AVCodec* encoder, AVSampleFormat preferred) {
  auto formats = supportedConfig<AVSampleFormat>(encoder, AV_CODEC_CONFIG_SAMPLE_FORMAT);
  if (formats.empty()) return preferred;
  if (std::find(formats.begin(), formats.end(), preferred) != formats.end()) return preferred;
  return formats.front();
}

int chooseSampleRate(const AVCodec* encoder, int preferred) {
  auto rates = supportedConfig<int>(encoder, AV_CODEC_CONFIG_SAMPLE_RATE);
  if (rates.empty()) return preferred;
  if (std::find(rates.begin(), rates.end(), preferred) != rates.end()) return preferred;
  // Closest supported rate at or above the preferred one, else the highest.
  int best = rates.front();
  for (int rate : rates) {
    if (std::abs(rate - preferred) < std::abs(best - preferred)) best = rate;
  }
  return best;
}

void chooseChannelLayout(const AVCodec* encoder, int preferredChannels, AVChannelLayout* out) {
  auto layouts = supportedConfig<AVChannelLayout>(encoder, AV_CODEC_CONFIG_CHANNEL_LAYOUT);
  if (layouts.empty()) {
    av_channel_layout_default(out, preferredChannels);
    return;
  }
  for (const auto& layout : layouts) {
    if (layout.nb_channels == preferredChannels) {
      av_channel_layout_copy(out, &layout);
      return;
    }
  }
  av_channel_layout_copy(out, &layouts.front());
}

void copyMetadata(const PlanData& plan, const AVFormatContext* input, AVFormatContext* output) {
  switch (plan.metadataPolicy) {
  case MetadataPolicy::DROP:
    break;
  case MetadataPolicy::REPLACE:
    for (const auto& [key, value] : plan.metadataValues) {
      av_dict_set(&output->metadata, key.c_str(), value.c_str(), 0);
    }
    break;
  case MetadataPolicy::MERGE:
    av_dict_copy(&output->metadata, input->metadata, 0);
    for (const auto& [key, value] : plan.metadataValues) {
      av_dict_set(&output->metadata, key.c_str(), value.c_str(), 0);
    }
    break;
  case MetadataPolicy::COPY:
  default:
    av_dict_copy(&output->metadata, input->metadata, 0);
    break;
  }
}

struct ProgressReporter {
  const ProgressCallback& callback;
  ProgressSnapshot snapshot;
  std::chrono::steady_clock::time_point startedAt;
  std::chrono::steady_clock::time_point lastEmit;

  void emit(JobPhase phase, bool force) {
    auto now = std::chrono::steady_clock::now();
    if (!force && now - lastEmit < std::chrono::milliseconds(16)) return;
    lastEmit = now;
    snapshot.phase = phase;
    double elapsed = std::chrono::duration<double>(now - startedAt).count();
    if (elapsed > 0 && snapshot.inputSecondsProcessed > 0) {
      double speed = snapshot.inputSecondsProcessed / elapsed;
      snapshot.speedRatio = speed;
      if (snapshot.totalInputSeconds.has_value() && speed > 0) {
        double remaining = (*snapshot.totalInputSeconds - snapshot.inputSecondsProcessed) / speed;
        snapshot.estimatedSecondsRemaining = remaining > 0 ? remaining : 0;
      }
    }
    if (callback) callback(snapshot);
  }
};

ReportData runPlanInternal(ByteSource& source, ByteSink& sink, const PlanData& plan,
                           const CancellationTokenRef& cancellation, const ProgressCallback& onProgress) {
  auto startedAt = std::chrono::steady_clock::now();
  if (source.isSeekable() && !source.seek(0)) {
    throw TranscoderException(error_code::SOURCE_READ_FAILED, error_stage::DEMUX, "Cannot rewind the source.");
  }

  ReadBridge readBridge(source, cancellation);
  WriteBridge writeBridge(sink, cancellation);

  ProgressReporter reporter{onProgress, ProgressSnapshot{}, startedAt, startedAt};
  reporter.snapshot.totalInputSeconds = plan.sourceDurationSeconds;
  reporter.emit(JobPhase::STARTING, true);

  FormatContextPtr input = openInput(readBridge);
  if (plan.sourceStreamIndex < 0 || static_cast<unsigned>(plan.sourceStreamIndex) >= input->nb_streams) {
    throw TranscoderException(error_code::INVALID_REQUEST, error_stage::DEMUX, "The selected stream no longer exists.");
  }
  AVStream* inputStream = input->streams[plan.sourceStreamIndex];

  const char* muxerName = toMuxerName(plan.outputContainer);
  if (muxerName == nullptr) {
    throw TranscoderException(error_code::UNSUPPORTED_COMBINATION, error_stage::MUX,
                              "This build cannot write the requested container.");
  }
  OutputFormatContextPtr output = openOutput(writeBridge, muxerName);

  bool isRemux = plan.path == PlanPath::PACKET_REMUX || plan.path == PlanPath::METADATA_REMUX;

  CodecContextPtr decoderContext;
  CodecContextPtr encoderContext;
  SwrPtr resampler;
  AudioFifoPtr fifo;
  AVStream* outputStream = avformat_new_stream(output.get(), nullptr);
  if (outputStream == nullptr) {
    throw TranscoderException(error_code::INTERNAL_ERROR, error_stage::MUX, "Cannot create an output stream.");
  }

  int encoderFrameSize = DEFAULT_FRAME_SIZE;

  if (isRemux) {
    int status = avcodec_parameters_copy(outputStream->codecpar, inputStream->codecpar);
    if (status < 0) fail(error_code::MUX_FAILED, error_stage::MUX, "Cannot copy stream parameters", status);
    outputStream->codecpar->codec_tag = 0;
    outputStream->time_base = inputStream->time_base;
  } else {
    const AVCodec* decoder = avcodec_find_decoder(inputStream->codecpar->codec_id);
    if (decoder == nullptr) {
      throw TranscoderException(error_code::CAPABILITY_NOT_MET, error_stage::DECODE,
                                "No decoder for the source codec is linked into this build.");
    }
    decoderContext.reset(avcodec_alloc_context3(decoder));
    if (decoderContext == nullptr) {
      throw TranscoderException(error_code::INTERNAL_ERROR, error_stage::DECODE, "Cannot allocate a decoder.");
    }
    int status = avcodec_parameters_to_context(decoderContext.get(), inputStream->codecpar);
    if (status < 0) fail(error_code::DECODE_FAILED, error_stage::DECODE, "Cannot configure the decoder", status);
    decoderContext->pkt_timebase = inputStream->time_base;
    status = avcodec_open2(decoderContext.get(), decoder, nullptr);
    if (status < 0) fail(error_code::DECODE_FAILED, error_stage::DECODE, "Cannot open the decoder", status);

    AVCodecID encoderCodecId = plan.outputCodec.has_value() ? toAvCodec(*plan.outputCodec) : AV_CODEC_ID_NONE;
    const AVCodec* encoder = avcodec_find_encoder(encoderCodecId);
    if (encoder == nullptr) {
      throw TranscoderException(error_code::CAPABILITY_NOT_MET, error_stage::ENCODE,
                                "No encoder for the requested codec is linked into this build.");
    }
    encoderContext.reset(avcodec_alloc_context3(encoder));
    if (encoderContext == nullptr) {
      throw TranscoderException(error_code::INTERNAL_ERROR, error_stage::ENCODE, "Cannot allocate an encoder.");
    }

    int targetSampleRate = plan.targetSampleRate > 0 ? plan.targetSampleRate : decoderContext->sample_rate;
    int targetChannels = plan.targetChannelCount > 0 ? plan.targetChannelCount : decoderContext->ch_layout.nb_channels;

    encoderContext->sample_rate = chooseSampleRate(encoder, targetSampleRate);
    encoderContext->sample_fmt = chooseSampleFormat(encoder, decoderContext->sample_fmt);
    chooseChannelLayout(encoder, targetChannels, &encoderContext->ch_layout);
    if (plan.targetBitRate > 0) encoderContext->bit_rate = plan.targetBitRate;
    encoderContext->time_base = AVRational{1, encoderContext->sample_rate};
    if ((output->oformat->flags & AVFMT_GLOBALHEADER) != 0) {
      encoderContext->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
    }

    status = avcodec_open2(encoderContext.get(), encoder, nullptr);
    if (status < 0) fail(error_code::ENCODE_FAILED, error_stage::ENCODE, "Cannot open the encoder", status);

    encoderFrameSize = encoderContext->frame_size > 0 ? encoderContext->frame_size : DEFAULT_FRAME_SIZE;

    status = avcodec_parameters_from_context(outputStream->codecpar, encoderContext.get());
    if (status < 0) fail(error_code::MUX_FAILED, error_stage::MUX, "Cannot describe the output stream", status);
    outputStream->time_base = encoderContext->time_base;

    SwrContext* rawResampler = nullptr;
    status = swr_alloc_set_opts2(&rawResampler, &encoderContext->ch_layout, encoderContext->sample_fmt,
                                 encoderContext->sample_rate, &decoderContext->ch_layout, decoderContext->sample_fmt,
                                 decoderContext->sample_rate, 0, nullptr);
    if (status < 0 || rawResampler == nullptr) {
      fail(error_code::PROCESS_FAILED, error_stage::PROCESS, "Cannot configure the resampler", status);
    }
    resampler.reset(rawResampler);
    status = swr_init(resampler.get());
    if (status < 0) fail(error_code::PROCESS_FAILED, error_stage::PROCESS, "Cannot start the resampler", status);

    fifo.reset(
        av_audio_fifo_alloc(encoderContext->sample_fmt, encoderContext->ch_layout.nb_channels, encoderFrameSize * 4));
    if (fifo == nullptr) {
      throw TranscoderException(error_code::INTERNAL_ERROR, error_stage::PROCESS, "Cannot allocate a sample buffer.");
    }
  }

  copyMetadata(plan, input.get(), output.get());

  int status = avformat_write_header(output.get(), nullptr);
  if (status < 0) {
    sink.abort();
    checkCancelled(cancellation);
    fail(error_code::MUX_FAILED, error_stage::MUX, "Cannot write the output header", status);
  }

  PacketPtr packet(av_packet_alloc());
  PacketPtr outPacket(av_packet_alloc());
  FramePtr decodedFrame(av_frame_alloc());
  FramePtr encodeFrame(av_frame_alloc());
  if (packet == nullptr || outPacket == nullptr || decodedFrame == nullptr || encodeFrame == nullptr) {
    throw TranscoderException(error_code::INTERNAL_ERROR, error_stage::ENGINE, "Cannot allocate media buffers.");
  }

  int64_t encodedSamples = 0;
  double samplesProcessed = 0;
  bool cancelled = false;

  auto writeEncodedPackets = [&](bool flushing) {
    while (true) {
      int receiveStatus = avcodec_receive_packet(encoderContext.get(), outPacket.get());
      if (receiveStatus == AVERROR(EAGAIN) || receiveStatus == AVERROR_EOF) break;
      if (receiveStatus < 0) {
        fail(error_code::ENCODE_FAILED, error_stage::ENCODE, "Encoding failed", receiveStatus);
      }
      outPacket->stream_index = outputStream->index;
      av_packet_rescale_ts(outPacket.get(), encoderContext->time_base, outputStream->time_base);
      int writeStatus = av_interleaved_write_frame(output.get(), outPacket.get());
      av_packet_unref(outPacket.get());
      if (writeStatus < 0) {
        fail(error_code::MUX_FAILED, error_stage::MUX, "Cannot write an output packet", writeStatus);
      }
    }
    (void)flushing;
  };

  auto encodeFromFifo = [&](bool drain) {
    while (av_audio_fifo_size(fifo.get()) >= (drain ? 1 : encoderFrameSize)) {
      int frameSamples = std::min(encoderFrameSize, av_audio_fifo_size(fifo.get()));
      av_frame_unref(encodeFrame.get());
      encodeFrame->nb_samples = frameSamples;
      encodeFrame->format = encoderContext->sample_fmt;
      encodeFrame->sample_rate = encoderContext->sample_rate;
      av_channel_layout_copy(&encodeFrame->ch_layout, &encoderContext->ch_layout);
      int allocStatus = av_frame_get_buffer(encodeFrame.get(), 0);
      if (allocStatus < 0) {
        fail(error_code::ENCODE_FAILED, error_stage::ENCODE, "Cannot allocate an encoder frame", allocStatus);
      }
      int readSamples = av_audio_fifo_read(fifo.get(), reinterpret_cast<void**>(encodeFrame->data), frameSamples);
      if (readSamples < frameSamples) {
        throw TranscoderException(error_code::PROCESS_FAILED, error_stage::PROCESS, "Sample buffer underrun.");
      }
      encodeFrame->pts = encodedSamples;
      encodedSamples += readSamples;
      samplesProcessed += readSamples;

      int sendStatus = avcodec_send_frame(encoderContext.get(), encodeFrame.get());
      if (sendStatus < 0) fail(error_code::ENCODE_FAILED, error_stage::ENCODE, "Cannot encode audio", sendStatus);
      writeEncodedPackets(false);
    }
  };

  try {
    while (true) {
      checkCancelled(cancellation);
      int readStatus = av_read_frame(input.get(), packet.get());
      if (readStatus == AVERROR_EOF) break;
      if (readStatus == AVERROR_EXIT) {
        cancelled = true;
        break;
      }
      if (readStatus < 0)
        fail(error_code::SOURCE_READ_FAILED, error_stage::DEMUX, "Cannot read the source", readStatus);

      if (packet->stream_index != plan.sourceStreamIndex) {
        av_packet_unref(packet.get());
        continue;
      }

      if (packet->pts != AV_NOPTS_VALUE) {
        reporter.snapshot.inputSecondsProcessed = static_cast<double>(packet->pts) * av_q2d(inputStream->time_base);
      }
      reporter.snapshot.inputBytesRead = readBridge.bytesRead();
      reporter.snapshot.outputBytesWritten = writeBridge.bytesWritten();
      reporter.emit(JobPhase::TRANSCODING, false);

      if (isRemux) {
        av_packet_rescale_ts(packet.get(), inputStream->time_base, outputStream->time_base);
        packet->stream_index = outputStream->index;
        packet->pos = -1;
        int writeStatus = av_interleaved_write_frame(output.get(), packet.get());
        av_packet_unref(packet.get());
        if (writeStatus < 0) {
          fail(error_code::MUX_FAILED, error_stage::MUX, "Cannot write an output packet", writeStatus);
        }
        continue;
      }

      int sendStatus = avcodec_send_packet(decoderContext.get(), packet.get());
      av_packet_unref(packet.get());
      if (sendStatus < 0) fail(error_code::DECODE_FAILED, error_stage::DECODE, "Cannot decode audio", sendStatus);

      while (true) {
        int receiveStatus = avcodec_receive_frame(decoderContext.get(), decodedFrame.get());
        if (receiveStatus == AVERROR(EAGAIN) || receiveStatus == AVERROR_EOF) break;
        if (receiveStatus < 0) {
          fail(error_code::DECODE_FAILED, error_stage::DECODE, "Cannot decode audio", receiveStatus);
        }

        int outSamples = swr_get_out_samples(resampler.get(), decodedFrame->nb_samples);
        FramePtr converted(av_frame_alloc());
        converted->nb_samples = outSamples;
        converted->format = encoderContext->sample_fmt;
        converted->sample_rate = encoderContext->sample_rate;
        av_channel_layout_copy(&converted->ch_layout, &encoderContext->ch_layout);
        int allocStatus = av_frame_get_buffer(converted.get(), 0);
        if (allocStatus < 0) {
          fail(error_code::PROCESS_FAILED, error_stage::PROCESS, "Cannot allocate a converted frame", allocStatus);
        }
        int produced = swr_convert(resampler.get(), converted->data, outSamples,
                                   const_cast<const uint8_t**>(decodedFrame->data), decodedFrame->nb_samples);
        if (produced < 0) fail(error_code::PROCESS_FAILED, error_stage::PROCESS, "Resampling failed", produced);
        if (produced > 0) {
          if (av_audio_fifo_realloc(fifo.get(), av_audio_fifo_size(fifo.get()) + produced) < 0) {
            throw TranscoderException(error_code::RESOURCE_LIMIT_EXCEEDED, error_stage::PROCESS,
                                      "Cannot grow the sample buffer.");
          }
          av_audio_fifo_write(fifo.get(), reinterpret_cast<void**>(converted->data), produced);
        }
        av_frame_unref(decodedFrame.get());
        encodeFromFifo(false);
      }
    }

    if (!cancelled && !isRemux) {
      reporter.emit(JobPhase::FLUSHING, true);
      // Flush decoder.
      avcodec_send_packet(decoderContext.get(), nullptr);
      while (true) {
        int receiveStatus = avcodec_receive_frame(decoderContext.get(), decodedFrame.get());
        if (receiveStatus == AVERROR(EAGAIN) || receiveStatus == AVERROR_EOF) break;
        if (receiveStatus < 0) break;
        int outSamples = swr_get_out_samples(resampler.get(), decodedFrame->nb_samples);
        FramePtr converted(av_frame_alloc());
        converted->nb_samples = outSamples;
        converted->format = encoderContext->sample_fmt;
        converted->sample_rate = encoderContext->sample_rate;
        av_channel_layout_copy(&converted->ch_layout, &encoderContext->ch_layout);
        if (av_frame_get_buffer(converted.get(), 0) < 0) break;
        int produced = swr_convert(resampler.get(), converted->data, outSamples,
                                   const_cast<const uint8_t**>(decodedFrame->data), decodedFrame->nb_samples);
        if (produced > 0) {
          av_audio_fifo_realloc(fifo.get(), av_audio_fifo_size(fifo.get()) + produced);
          av_audio_fifo_write(fifo.get(), reinterpret_cast<void**>(converted->data), produced);
        }
        av_frame_unref(decodedFrame.get());
      }
      // Drain the resampler itself.
      while (true) {
        int pending = swr_get_out_samples(resampler.get(), 0);
        if (pending <= 0) break;
        FramePtr converted(av_frame_alloc());
        converted->nb_samples = pending;
        converted->format = encoderContext->sample_fmt;
        converted->sample_rate = encoderContext->sample_rate;
        av_channel_layout_copy(&converted->ch_layout, &encoderContext->ch_layout);
        if (av_frame_get_buffer(converted.get(), 0) < 0) break;
        int produced = swr_convert(resampler.get(), converted->data, pending, nullptr, 0);
        if (produced <= 0) break;
        av_audio_fifo_realloc(fifo.get(), av_audio_fifo_size(fifo.get()) + produced);
        av_audio_fifo_write(fifo.get(), reinterpret_cast<void**>(converted->data), produced);
      }
      encodeFromFifo(true);
      avcodec_send_frame(encoderContext.get(), nullptr);
      writeEncodedPackets(true);
    }

    if (cancelled) {
      throw TranscoderException(error_code::CANCELLED, error_stage::ENGINE, "The job was cancelled.");
    }

    reporter.emit(JobPhase::FINALIZING, true);
    status = av_write_trailer(output.get());
    if (status < 0) fail(error_code::MUX_FAILED, error_stage::MUX, "Cannot finalize the output", status);
  } catch (...) {
    sink.abort();
    // A cancelled run surfaces as an I/O error deep inside FFmpeg; report the
    // real cause instead.
    if (cancellation != nullptr && cancellation->isCancelled()) {
      throw TranscoderException(error_code::CANCELLED, error_stage::ENGINE, "The job was cancelled.");
    }
    throw;
  }

  sink.commit();

  ReportData report;
  report.outputByteSize = writeBridge.bytesWritten();
  report.outputContainer = plan.outputContainer;
  report.outputCodec = plan.outputCodec;
  report.inputBytesRead = readBridge.bytesRead();
  report.samplesProcessed = samplesProcessed;
  report.warnings = plan.warnings;
  report.didCommitOutput = true;
  if (encoderContext != nullptr) {
    report.outputSampleRate = static_cast<double>(encoderContext->sample_rate);
    report.outputChannelCount = static_cast<double>(encoderContext->ch_layout.nb_channels);
    if (encoderContext->sample_rate > 0) {
      report.outputDurationSeconds = static_cast<double>(encodedSamples) / encoderContext->sample_rate;
    }
  } else {
    report.outputDurationSeconds = plan.sourceDurationSeconds;
  }
  double elapsed = std::chrono::duration<double>(std::chrono::steady_clock::now() - startedAt).count();
  report.elapsedSeconds = elapsed;
  if (elapsed > 0 && report.outputDurationSeconds.has_value()) {
    report.speedRatio = *report.outputDurationSeconds / elapsed;
  }

  reporter.snapshot.inputBytesRead = report.inputBytesRead;
  reporter.snapshot.outputBytesWritten = report.outputByteSize;
  reporter.emit(JobPhase::DONE, true);
  return report;
}

// Combinations `avformat_query_codec` accepts but that do not survive a round
// trip. Only demonstrated defects belong here.
//   AAC: the CAF muxer rejects it at write_header time.
//   FLAC: the CAF muxer writes a `kuki` magic cookie only for ALAC, AMR-NB and
//   QDM2, so a FLAC stream lands without its STREAMINFO and the CAF demuxer
//   cannot read the file back.
bool isKnownBrokenPair(ContainerId container, AudioCodecId codec) {
  return container == ContainerId::CAF && (codec == AudioCodecId::AAC || codec == AudioCodecId::FLAC);
}

// The Ogg family shares one muxer implementation but declares a single
// `audio_codec`, so `avformat_query_codec` can only ever confirm that one.
// Every entry here is verified by actually muxing it.
bool isKnownGoodPair(ContainerId container, AudioCodecId codec) {
  return container == ContainerId::OGG &&
         (codec == AudioCodecId::VORBIS || codec == AudioCodecId::OPUS || codec == AudioCodecId::FLAC);
}

} // namespace

ReportData runPlan(ByteSource& source, ByteSink& sink, const PlanData& plan, const CancellationTokenRef& cancellation,
                   const ProgressCallback& onProgress) {
  try {
    return runPlanInternal(source, sink, plan, cancellation, onProgress);
  } catch (...) {
    sink.abort();
    // Cancellation surfaces as an I/O error deep inside FFmpeg, wherever the run
    // happened to be. Report the real cause instead of the symptom.
    if (cancellation != nullptr && cancellation->isCancelled()) {
      throw TranscoderException(error_code::CANCELLED, error_stage::ENGINE, "The job was cancelled.");
    }
    throw;
  }
}

void enumerateCapabilities(std::vector<CodecCapability>& decoders, std::vector<CodecCapability>& encoders,
                           std::vector<ContainerCapability>& containers) {
  constexpr std::array<AudioCodecId, 13> KNOWN_CODECS{
      AudioCodecId::PCM_U8, AudioCodecId::PCM_S16, AudioCodecId::PCM_S24, AudioCodecId::PCM_S32, AudioCodecId::PCM_F32,
      AudioCodecId::AAC,    AudioCodecId::MP3,     AudioCodecId::FLAC,    AudioCodecId::ALAC,    AudioCodecId::OPUS,
      AudioCodecId::VORBIS, AudioCodecId::AC3,     AudioCodecId::EAC3};

  // Keyed off the encoder actually linked in, so it cannot drift from the build.
  auto licenseFor = [](const AVCodec* av) -> const char* {
    if (std::strcmp(av->name, "libmp3lame") == 0) return "LGPL-2.0-or-later";
    if (std::strcmp(av->name, "libopus") == 0 || std::strcmp(av->name, "libvorbis") == 0) return "BSD-3-Clause";
    return "LGPL-2.1-or-later";
  };

  auto describe = [&licenseFor](AudioCodecId codec, const AVCodec* av, CodecDirection direction) {
    CodecCapability capability;
    capability.codec = codec;
    capability.codecName = av->name;
    capability.direction = direction;
    capability.licenseClass = licenseFor(av);
    capability.maxChannelCount = 8;
    capability.supportsVbr = (av->capabilities & AV_CODEC_CAP_VARIABLE_FRAME_SIZE) != 0;
    for (int rate : supportedConfig<int>(av, AV_CODEC_CONFIG_SAMPLE_RATE)) {
      capability.sampleRates.push_back(static_cast<double>(rate));
    }
    return capability;
  };

  for (AudioCodecId codec : KNOWN_CODECS) {
    AVCodecID avId = toAvCodec(codec);
    if (const AVCodec* decoder = avcodec_find_decoder(avId); decoder != nullptr) {
      decoders.push_back(describe(codec, decoder, CodecDirection::DECODE));
    }
    if (const AVCodec* encoder = avcodec_find_encoder(avId); encoder != nullptr) {
      encoders.push_back(describe(codec, encoder, CodecDirection::ENCODE));
    }
  }

  constexpr std::array<ContainerId, 11> KNOWN_CONTAINERS{ContainerId::WAV,  ContainerId::AIFF,     ContainerId::CAF,
                                                         ContainerId::M4A,  ContainerId::MP4,      ContainerId::ADTS,
                                                         ContainerId::FLAC, ContainerId::MATROSKA, ContainerId::OGG,
                                                         ContainerId::RF64, ContainerId::MP3};

  for (ContainerId container : KNOWN_CONTAINERS) {
    const char* muxerName = toMuxerName(container);
    if (muxerName == nullptr) continue;
    const AVOutputFormat* format = av_guess_format(muxerName, nullptr, nullptr);
    if (format == nullptr) continue;

    ContainerCapability capability;
    capability.container = container;
    capability.supportsMetadata = true;
    capability.supportsChapters =
        container == ContainerId::M4A || container == ContainerId::MP4 || container == ContainerId::MATROSKA;
    capability.supportsArtwork = container == ContainerId::M4A || container == ContainerId::MP4 ||
                                 container == ContainerId::FLAC || container == ContainerId::MATROSKA;
    capability.requiresSeekableOutput =
        (format->flags & AVFMT_NOFILE) == 0 && container != ContainerId::ADTS && container != ContainerId::MATROSKA;
    for (const auto& encoder : encoders) {
      AVCodecID avId = toAvCodec(encoder.codec);
      if (avformat_query_codec(format, avId, FF_COMPLIANCE_NORMAL) != 1 && !isKnownGoodPair(container, encoder.codec))
        continue;
      if (isKnownBrokenPair(container, encoder.codec)) continue;
      capability.codecs.push_back(encoder.codec);
    }
    containers.push_back(std::move(capability));
  }
}

} // namespace margelo::nitro::transcoder::ffmpeg
