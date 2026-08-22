#pragma once

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/audio_fifo.h>
#include <libavutil/channel_layout.h>
#include <libavutil/opt.h>
#include <libswresample/swresample.h>
}

#include "AudioCodecId.hpp"
#include "ContainerId.hpp"
#include "SampleFormat.hpp"

#include <memory>
#include <optional>
#include <string>

namespace margelo::nitro::transcoder::ffmpeg {

struct FormatContextDeleter {
  void operator()(AVFormatContext* context) const noexcept;
};
struct OutputFormatContextDeleter {
  void operator()(AVFormatContext* context) const noexcept;
};
struct CodecContextDeleter {
  void operator()(AVCodecContext* context) const noexcept { avcodec_free_context(&context); }
};
struct FrameDeleter {
  void operator()(AVFrame* frame) const noexcept { av_frame_free(&frame); }
};
struct PacketDeleter {
  void operator()(AVPacket* packet) const noexcept { av_packet_free(&packet); }
};
struct SwrDeleter {
  void operator()(SwrContext* context) const noexcept { swr_free(&context); }
};
struct AudioFifoDeleter {
  void operator()(AVAudioFifo* fifo) const noexcept { av_audio_fifo_free(fifo); }
};

using FormatContextPtr = std::unique_ptr<AVFormatContext, FormatContextDeleter>;
using OutputFormatContextPtr = std::unique_ptr<AVFormatContext, OutputFormatContextDeleter>;
using CodecContextPtr = std::unique_ptr<AVCodecContext, CodecContextDeleter>;
using FramePtr = std::unique_ptr<AVFrame, FrameDeleter>;
using PacketPtr = std::unique_ptr<AVPacket, PacketDeleter>;
using SwrPtr = std::unique_ptr<SwrContext, SwrDeleter>;
using AudioFifoPtr = std::unique_ptr<AVAudioFifo, AudioFifoDeleter>;

std::string describeAvError(int errorCode);

// Engine identifiers <-> FFmpeg identifiers. Every conversion lives here, so no
// FFmpeg type ever leaks into `engine/` or a Nitro spec.
std::optional<AudioCodecId> toEngineCodec(AVCodecID codecId);
AVCodecID toAvCodec(AudioCodecId codec);
std::optional<ContainerId> toEngineContainer(const std::string& formatName);
const char* toMuxerName(ContainerId container);
std::optional<SampleFormat> toEngineSampleFormat(AVSampleFormat format);

} // namespace margelo::nitro::transcoder::ffmpeg
