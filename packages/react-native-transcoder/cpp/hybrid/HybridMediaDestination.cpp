#include "HybridMediaDestination.hpp"
#include "media/Error.hpp"

namespace margelo::nitro::transcoder {

void HybridMediaDestination::close() {
  std::lock_guard<std::mutex> lock(_mutex);
  if (_byteSink != nullptr) {
    _byteSink->abort();
    _byteSink.reset();
  }
}

ByteSink& HybridMediaDestination::byteSink() {
  std::lock_guard<std::mutex> lock(_mutex);
  if (_byteSink == nullptr) {
    throw TranscoderException(error_code::INVALID_REQUEST, error_stage::OPEN,
                              "This MediaDestination was already closed.");
  }
  return *_byteSink;
}

} // namespace margelo::nitro::transcoder
