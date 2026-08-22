#pragma once

#include "HybridMediaDestinationSpec.hpp"
#include "media/ByteIo.hpp"

#include <memory>
#include <mutex>
#include <optional>
#include <string>

namespace margelo::nitro::transcoder {

// A destination owns its sink until a job commits it. Closing without a commit
// removes only the engine-created temporary file.
class HybridMediaDestination final : public HybridMediaDestinationSpec {
public:
  HybridMediaDestination(MediaDestinationKind kind, OverwritePolicy atomicity, std::unique_ptr<ByteSink> byteSink,
                         std::optional<std::string> uri)
      : HybridObject(TAG), _kind(kind), _atomicity(atomicity), _byteSink(std::move(byteSink)), _uri(std::move(uri)) {}

  MediaDestinationKind getKind() override { return _kind; }
  std::optional<std::string> getUri() override { return _uri; }
  OverwritePolicy getAtomicity() override { return _atomicity; }
  void close() override;

  // Engine-internal. Throws when the destination was already closed.
  ByteSink& byteSink();

private:
  MediaDestinationKind _kind;
  OverwritePolicy _atomicity;
  std::unique_ptr<ByteSink> _byteSink;
  std::optional<std::string> _uri;
  std::mutex _mutex;
};

} // namespace margelo::nitro::transcoder
