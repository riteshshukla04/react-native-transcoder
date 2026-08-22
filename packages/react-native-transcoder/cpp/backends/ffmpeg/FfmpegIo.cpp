#include "FfmpegIo.hpp"
#include "media/Error.hpp"

namespace margelo::nitro::transcoder::ffmpeg {

namespace {
constexpr int IO_BUFFER_SIZE = 64 * 1024;
} // namespace

ReadBridge::ReadBridge(ByteSource& source, CancellationTokenRef cancellation)
    : _source(source), _cancellation(std::move(cancellation)) {
  auto* buffer = static_cast<uint8_t*>(av_malloc(IO_BUFFER_SIZE));
  _context = avio_alloc_context(buffer, IO_BUFFER_SIZE, 0, this, &ReadBridge::read, nullptr, &ReadBridge::seek);
  if (_context == nullptr) {
    av_free(buffer);
    throw TranscoderException(error_code::INTERNAL_ERROR, error_stage::DEMUX, "Cannot allocate a read context.");
  }
  _context->seekable = source.isSeekable() ? AVIO_SEEKABLE_NORMAL : 0;
}

ReadBridge::~ReadBridge() {
  if (_context == nullptr) return;
  av_freep(&_context->buffer);
  avio_context_free(&_context);
}

int ReadBridge::read(void* opaque, uint8_t* buffer, int bufferSize) {
  auto* self = static_cast<ReadBridge*>(opaque);
  if (self->_cancellation != nullptr && self->_cancellation->isCancelled()) return AVERROR_EXIT;
  std::size_t read = self->_source.read(buffer, static_cast<std::size_t>(bufferSize));
  if (read == 0) return AVERROR_EOF;
  self->_bytesRead += static_cast<double>(read);
  return static_cast<int>(read);
}

int64_t ReadBridge::seek(void* opaque, int64_t offset, int whence) {
  auto* self = static_cast<ReadBridge*>(opaque);
  ByteSource& source = self->_source;
  if (whence == AVSEEK_SIZE) {
    auto length = source.byteLength();
    return length.has_value() ? *length : AVERROR(ENOSYS);
  }
  if (!source.isSeekable()) return AVERROR(ENOSYS);

  int64_t target = offset;
  if (whence == SEEK_CUR) {
    target = source.position() + offset;
  } else if (whence == SEEK_END) {
    auto length = source.byteLength();
    if (!length.has_value()) return AVERROR(ENOSYS);
    target = *length + offset;
  }
  if (!source.seek(target)) return AVERROR(EIO);
  return target;
}

WriteBridge::WriteBridge(ByteSink& sink, CancellationTokenRef cancellation)
    : _sink(sink), _cancellation(std::move(cancellation)) {
  auto* buffer = static_cast<uint8_t*>(av_malloc(IO_BUFFER_SIZE));
  _context = avio_alloc_context(buffer, IO_BUFFER_SIZE, 1, this, nullptr, &WriteBridge::write, &WriteBridge::seek);
  if (_context == nullptr) {
    av_free(buffer);
    throw TranscoderException(error_code::INTERNAL_ERROR, error_stage::MUX, "Cannot allocate a write context.");
  }
  _context->seekable = sink.isSeekable() ? AVIO_SEEKABLE_NORMAL : 0;
}

WriteBridge::~WriteBridge() {
  if (_context == nullptr) return;
  av_freep(&_context->buffer);
  avio_context_free(&_context);
}

int WriteBridge::write(void* opaque, const uint8_t* buffer, int bufferSize) {
  auto* self = static_cast<WriteBridge*>(opaque);
  if (self->_cancellation != nullptr && self->_cancellation->isCancelled()) return AVERROR_EXIT;
  self->_sink.write(buffer, static_cast<std::size_t>(bufferSize));
  self->_bytesWritten += static_cast<double>(bufferSize);
  return bufferSize;
}

int64_t WriteBridge::seek(void* opaque, int64_t offset, int whence) {
  auto* self = static_cast<WriteBridge*>(opaque);
  if (whence != SEEK_SET || !self->_sink.isSeekable()) return AVERROR(ENOSYS);
  if (!self->_sink.seek(offset)) return AVERROR(EIO);
  return offset;
}

} // namespace margelo::nitro::transcoder::ffmpeg
