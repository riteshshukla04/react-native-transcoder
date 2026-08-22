#pragma once

#include "TranscodeRequest.hpp"
#include "media/MediaModel.hpp"

namespace margelo::nitro::transcoder {

// Turns a request plus a probe result into an executable plan, or fails with a
// precise error. There is exactly one planning implementation, shared by
// `resolve` and both job-creation paths.
PlanData resolvePlan(const ProbeResult& probe, const TranscodeRequest& request);

std::string planToJson(const PlanData& plan);

} // namespace margelo::nitro::transcoder
