#include "media/Paths.hpp"
#include "media/Error.hpp"

#include <cstdlib>
#include <sys/stat.h>
#include <unistd.h>

#if defined(__ANDROID__)
#include <cstdio>
#endif

namespace margelo::nitro::transcoder {

namespace {

bool isWritableDirectory(const std::string& path) {
  if (path.empty()) return false;
  struct stat info{};
  if (::stat(path.c_str(), &info) != 0) return false;
  if (!S_ISDIR(info.st_mode)) return false;
  return ::access(path.c_str(), W_OK) == 0;
}

#if defined(__ANDROID__)
// ponytail: package name via /proc/self/cmdline; swap for a platform-context HybridObject if an app needs another
// location.
std::string androidCacheDirectory() {
  std::FILE* file = std::fopen("/proc/self/cmdline", "rb");
  if (file == nullptr) return {};
  char buffer[256] = {};
  std::size_t read = std::fread(buffer, 1, sizeof(buffer) - 1, file);
  std::fclose(file);
  if (read == 0) return {};
  std::string packageName(buffer);
  std::size_t colon = packageName.find(':');
  if (colon != std::string::npos) packageName = packageName.substr(0, colon);
  if (packageName.empty()) return {};
  return "/data/data/" + packageName + "/cache";
}
#endif

std::string resolveScratchDirectory() {
#if defined(__ANDROID__)
  std::string cache = androidCacheDirectory();
  if (isWritableDirectory(cache)) return cache;
#endif
  const char* tmpdir = std::getenv("TMPDIR");
  if (tmpdir != nullptr && isWritableDirectory(tmpdir)) {
    std::string path(tmpdir);
    if (!path.empty() && path.back() == '/') path.pop_back();
    return path;
  }
  if (isWritableDirectory("/tmp")) return "/tmp";
  throw TranscoderException(error_code::RESOURCE_UNAVAILABLE, error_stage::ENGINE,
                            "No writable scratch directory is available to the engine.");
}

} // namespace

const std::string& scratchDirectory() {
  static const std::string directory = resolveScratchDirectory();
  return directory;
}

} // namespace margelo::nitro::transcoder
