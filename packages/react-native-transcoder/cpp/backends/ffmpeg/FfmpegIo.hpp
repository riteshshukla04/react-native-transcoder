#pragma once

#include "FfmpegSupport.hpp"
#include "media/ByteIo.hpp"
#include "media/CancellationToken.hpp"

#include <memory>

namespace margelo::nitro::transcoder::ffmpeg {

// Wraps an engine `ByteSource` in an AVIOContext, so a local file, a memory
// buffer and (later) an HTTP source all look identical to FFmpeg. The bridge
// owns the context and its buffer; format contexts use AVFMT_FLAG_CUSTOM_IO so
// FFmpeg never frees it behind our back.
class ReadBridge final {
public:
  ReadBridge(ByteSource& source, CancellationTokenRef cancellation);
  ~ReadBridge();

  ReadBridge(const ReadBridge&) = delete;
  ReadBridge& operator=(const ReadBridge&) = delete;

  AVIOContext* context() const noexcept { return _context; }
  double bytesRead() const noexcept { return _bytesRead; }

private:
  static int read(void* opaque, uint8_t* buffer, int bufferSize);
  static int64_t seek(void* opaque, int64_t offset, int whence);

  ByteSource& _source;
  CancellationTokenRef _cancellation;
  double _bytesRead = 0;
  AVIOContext* _context = nullptr;
};

class WriteBridge final {
public:
  WriteBridge(ByteSink& sink, CancellationTokenRef cancellation);
  ~WriteBridge();

  WriteBridge(const WriteBridge&) = delete;
  WriteBridge& operator=(const WriteBridge&) = delete;

  AVIOContext* context() const noexcept { return _context; }
  double bytesWritten() const noexcept { return _bytesWritten; }

private:
  static int write(void* opaque, const uint8_t* buffer, int bufferSize);
  static int64_t seek(void* opaque, int64_t offset, int whence);

  ByteSink& _sink;
  CancellationTokenRef _cancellation;
  double _bytesWritten = 0;
  AVIOContext* _context = nullptr;
};

} // namespace margelo::nitro::transcoder::ffmpeg
