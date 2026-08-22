#include "HybridResolvedPlan.hpp"
#include "media/Planner.hpp"

namespace margelo::nitro::transcoder {

std::string HybridResolvedPlan::toJson() {
  return planToJson(_plan);
}

} // namespace margelo::nitro::transcoder
