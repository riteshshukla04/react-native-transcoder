#include "media/ByteIo.hpp"
#include "media/Error.hpp"

#include <cerrno>
#include <cstring>
#include <sys/stat.h>
#include <unistd.h>

namespace margelo::nitro::transcoder {

namespace {

std::string describeErrno() {
  return std::string(std::strerror(errno));
}

bool isDecodedHexDigit(char c) {
  return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
}

int hexValue(char c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  return c - 'A' + 10;
}

std::string percentDecode(const std::string& input) {
  std::string output;
  output.reserve(input.size());
  for (std::size_t i = 0; i < input.size(); i++) {
    if (input[i] == '%' && i + 2 < input.size() && isDecodedHexDigit(input[i + 1]) && isDecodedHexDigit(input[i + 2])) {
      output.push_back(static_cast<char>(hexValue(input[i + 1]) * 16 + hexValue(input[i + 2])));
      i += 2;
    } else {
      output.push_back(input[i]);
    }
  }
  return output;
}

} // namespace

std::string normalizeFileUri(const std::string& uri) {
  constexpr std::string_view filePrefix = "file://";
  if (uri.rfind(filePrefix, 0) != 0) return uri;
  std::string path = uri.substr(filePrefix.size());
  // `file:///a/b` and `file://localhost/a/b` both mean `/a/b`.
  constexpr std::string_view localhost = "localhost/";
  if (path.rfind(localhost, 0) == 0) path = path.substr(localhost.size() - 1);
  return percentDecode(path);
}

void validateReadablePath(const std::string& path) {
  struct stat info{};
  if (::stat(path.c_str(), &info) != 0) {
    throw TranscoderException(error_code::SOURCE_READ_FAILED, error_stage::OPEN,
                              "Cannot open source at '" + path + "': " + describeErrno());
  }
  if (S_ISDIR(info.st_mode)) {
    throw TranscoderException(error_code::INVALID_REQUEST, error_stage::OPEN,
                              "Source path '" + path + "' is a directory, not a media file.");
  }
  if (::access(path.c_str(), R_OK) != 0) {
    throw TranscoderException(error_code::PERMISSION_DENIED, error_stage::OPEN,
                              "No read permission for source at '" + path + "'.");
  }
}

FileByteSource::FileByteSource(const std::string& path) : _path(path) {
  validateReadablePath(path);
  _file = std::fopen(path.c_str(), "rb");
  if (_file == nullptr) {
    throw TranscoderException(error_code::SOURCE_READ_FAILED, error_stage::OPEN,
                              "Cannot open source at '" + path + "': " + describeErrno());
  }
  struct stat info{};
  if (::stat(path.c_str(), &info) == 0) _byteLength = static_cast<int64_t>(info.st_size);
}

FileByteSource::~FileByteSource() {
  if (_file != nullptr) std::fclose(_file);
}

std::size_t FileByteSource::read(uint8_t* buffer, std::size_t byteCount) {
  std::size_t read = std::fread(buffer, 1, byteCount, _file);
  if (read < byteCount && std::ferror(_file) != 0) {
    throw TranscoderException(error_code::SOURCE_READ_FAILED, error_stage::DEMUX,
                              "Read failed for '" + _path + "': " + describeErrno(), true);
  }
  return read;
}

bool FileByteSource::seek(int64_t offset) {
  return std::fseek(_file, static_cast<long>(offset), SEEK_SET) == 0;
}

int64_t FileByteSource::position() const {
  return static_cast<int64_t>(std::ftell(_file));
}

std::size_t MemoryByteSource::read(uint8_t* buffer, std::size_t byteCount) {
  std::size_t available = _bytes.size() - _position;
  std::size_t toCopy = byteCount < available ? byteCount : available;
  if (toCopy > 0) std::memcpy(buffer, _bytes.data() + _position, toCopy);
  _position += toCopy;
  return toCopy;
}

bool MemoryByteSource::seek(int64_t offset) {
  if (offset < 0 || static_cast<std::size_t>(offset) > _bytes.size()) return false;
  _position = static_cast<std::size_t>(offset);
  return true;
}

AtomicFileByteSink::AtomicFileByteSink(std::string targetPath, bool allowOverwrite)
    : _targetPath(std::move(targetPath)) {
  struct stat info{};
  if (!allowOverwrite && ::stat(_targetPath.c_str(), &info) == 0) {
    throw TranscoderException(error_code::DESTINATION_WRITE_FAILED, error_stage::OPEN,
                              "Destination '" + _targetPath + "' already exists and overwrite was not requested.");
  }

  // Same volume, so `rename` stays atomic.
  _temporaryPath = _targetPath + ".transcoder-tmp";
  _file = std::fopen(_temporaryPath.c_str(), "wb");
  if (_file == nullptr) {
    throw TranscoderException(error_code::DESTINATION_WRITE_FAILED, error_stage::OPEN,
                              "Cannot create destination '" + _targetPath + "': " + describeErrno());
  }
}

AtomicFileByteSink::~AtomicFileByteSink() {
  if (!_committed) abort();
  closeFile();
}

void AtomicFileByteSink::closeFile() {
  if (_file != nullptr) {
    std::fclose(_file);
    _file = nullptr;
  }
}

void AtomicFileByteSink::write(const uint8_t* buffer, std::size_t byteCount) {
  if (_file == nullptr) {
    throw TranscoderException(error_code::DESTINATION_WRITE_FAILED, error_stage::WRITE,
                              "Destination '" + _targetPath + "' is already closed.");
  }
  std::size_t written = std::fwrite(buffer, 1, byteCount, _file);
  if (written != byteCount) {
    throw TranscoderException(error_code::DESTINATION_WRITE_FAILED, error_stage::WRITE,
                              "Write failed for '" + _targetPath + "': " + describeErrno());
  }
  _bytesWritten += static_cast<int64_t>(written);
}

bool AtomicFileByteSink::seek(int64_t offset) {
  if (_file == nullptr) return false;
  return std::fseek(_file, static_cast<long>(offset), SEEK_SET) == 0;
}

void AtomicFileByteSink::flush() {
  if (_file != nullptr) std::fflush(_file);
}

void AtomicFileByteSink::commit() {
  if (_committed) return;
  flush();
  closeFile();
  if (std::rename(_temporaryPath.c_str(), _targetPath.c_str()) != 0) {
    std::remove(_temporaryPath.c_str());
    throw TranscoderException(error_code::DESTINATION_WRITE_FAILED, error_stage::WRITE,
                              "Cannot publish output to '" + _targetPath + "': " + describeErrno());
  }
  _committed = true;
}

void AtomicFileByteSink::abort() {
  if (_committed) return;
  closeFile();
  // Only ever removes the engine-created temporary file.
  std::remove(_temporaryPath.c_str());
}

} // namespace margelo::nitro::transcoder
