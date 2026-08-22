#pragma once

#include "HybridResolvedPlanSpec.hpp"
#include "media/MediaModel.hpp"

namespace margelo::nitro::transcoder {

class HybridResolvedPlan final : public HybridResolvedPlanSpec {
public:
  explicit HybridResolvedPlan(PlanData plan) : HybridObject(TAG), _plan(std::move(plan)) {}

  PlanPath getPath() override { return _plan.path; }
  std::vector<double> getSelectedStreamIds() override { return _plan.selectedStreamIds; }
  ContainerId getOutputContainer() override { return _plan.outputContainer; }
  std::optional<AudioCodecId> getOutputCodec() override { return _plan.outputCodec; }
  std::optional<double> getOutputSampleRate() override { return _plan.outputSampleRate; }
  std::optional<double> getOutputChannelCount() override { return _plan.outputChannelCount; }
  std::optional<double> getOutputBitsPerSecond() override { return _plan.outputBitsPerSecond; }
  std::optional<double> getEstimatedOutputByteSize() override { return _plan.estimatedOutputByteSize; }
  std::vector<PlanConversion> getConversions() override { return _plan.conversions; }
  std::vector<PlanWarning> getWarnings() override { return _plan.warnings; }
  bool getRequiresSourceStaging() override { return _plan.requiresSourceStaging; }
  std::optional<double> getEstimatedStagingByteSize() override { return _plan.estimatedStagingByteSize; }
  bool getIsTwoPass() override { return _plan.isTwoPass; }

  std::string toJson() override;
  void close() override {}

  const PlanData& plan() const noexcept { return _plan; }

private:
  PlanData _plan;
};

} // namespace margelo::nitro::transcoder
