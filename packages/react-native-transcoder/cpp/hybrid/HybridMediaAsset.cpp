#include "HybridMediaAsset.hpp"
#include "EngineTask.hpp"
#include "media/Error.hpp"

namespace margelo::nitro::transcoder {

std::shared_ptr<Promise<std::string>> HybridMediaAsset::saveArtworkToFile(double index, const std::string& path) {
  (void)index;
  (void)path;
  return runOnEngine<std::string>([]() -> std::string {
    throw TranscoderException(error_code::CAPABILITY_NOT_MET, error_stage::PROBE,
                              "Artwork extraction is not implemented yet.");
  });
}

void HybridMediaAsset::close() {
  _probe = ProbeResult{};
}

} // namespace margelo::nitro::transcoder
