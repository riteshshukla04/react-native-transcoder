#pragma once

#include <cstdint>
#include <cstdio>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace margelo::nitro::transcoder {

// Bounded, engine-owned byte input. Local files, memory and (later) HTTP all
// look the same to a demuxer.
class ByteSource {
public:
  virtual ~ByteSource() = default;

  virtual std::size_t read(uint8_t* buffer, std::size_t byteCount) = 0;
  virtual bool isSeekable() const = 0;
  virtual bool seek(int64_t offset) = 0;
  virtual int64_t position() const = 0;
  virtual std::optional<int64_t> byteLength() const = 0;
};

// Engine-owned byte output. `commit` is what makes the write visible.
class ByteSink {
public:
  virtual ~ByteSink() = default;

  virtual void write(const uint8_t* buffer, std::size_t byteCount) = 0;
  virtual bool isSeekable() const = 0;
  virtual bool seek(int64_t offset) = 0;
  virtual void flush() = 0;
  // Makes the output visible at its final location.
  virtual void commit() = 0;
  // Discards everything written. Leaves any previous destination intact.
  virtual void abort() = 0;
};

class FileByteSource final : public ByteSource {
public:
  explicit FileByteSource(const std::string& path);
  ~FileByteSource() override;

  std::size_t read(uint8_t* buffer, std::size_t byteCount) override;
  bool isSeekable() const override { return true; }
  bool seek(int64_t offset) override;
  int64_t position() const override;
  std::optional<int64_t> byteLength() const override { return _byteLength; }

  const std::string& path() const noexcept { return _path; }

private:
  std::string _path;
  std::FILE* _file = nullptr;
  int64_t _byteLength = 0;
};

class MemoryByteSource final : public ByteSource {
public:
  explicit MemoryByteSource(std::vector<uint8_t>&& bytes) : _bytes(std::move(bytes)) {}

  std::size_t read(uint8_t* buffer, std::size_t byteCount) override;
  bool isSeekable() const override { return true; }
  bool seek(int64_t offset) override;
  int64_t position() const override { return static_cast<int64_t>(_position); }
  std::optional<int64_t> byteLength() const override { return static_cast<int64_t>(_bytes.size()); }

private:
  std::vector<uint8_t> _bytes;
  std::size_t _position = 0;
};

// Writes to a same-volume temporary file, then atomically replaces the target
// on `commit`. Failure or cancellation removes only the temporary file.
class AtomicFileByteSink final : public ByteSink {
public:
  AtomicFileByteSink(std::string targetPath, bool allowOverwrite);
  ~AtomicFileByteSink() override;

  void write(const uint8_t* buffer, std::size_t byteCount) override;
  bool isSeekable() const override { return true; }
  bool seek(int64_t offset) override;
  void flush() override;
  void commit() override;
  void abort() override;

  const std::string& targetPath() const noexcept { return _targetPath; }
  int64_t bytesWritten() const noexcept { return _bytesWritten; }

private:
  void closeFile();

  std::string _targetPath;
  std::string _temporaryPath;
  std::FILE* _file = nullptr;
  int64_t _bytesWritten = 0;
  bool _committed = false;
};

// Throws `permission-denied` / `source-read-failed` when the path is unusable.
void validateReadablePath(const std::string& path);
// Normalizes a `file://` URL into a plain filesystem path. Other schemes are
// returned unchanged so the caller can reject them with a precise error.
std::string normalizeFileUri(const std::string& uri);

} // namespace margelo::nitro::transcoder
