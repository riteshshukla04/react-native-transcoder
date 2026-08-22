#pragma once

#include "HybridTranscodeReportSpec.hpp"
#include "media/MediaModel.hpp"

namespace margelo::nitro::transcoder {

class HybridTranscodeReport final : public HybridTranscodeReportSpec {
public:
  explicit HybridTranscodeReport(ReportData report) : HybridObject(TAG), _report(std::move(report)) {}

  std::optional<std::string> getOutputUri() override { return _report.outputUri; }
  double getOutputByteSize() override { return _report.outputByteSize; }
  std::optional<double> getOutputDurationSeconds() override { return _report.outputDurationSeconds; }
  ContainerId getOutputContainer() override { return _report.outputContainer; }
  std::optional<AudioCodecId> getOutputCodec() override { return _report.outputCodec; }
  std::optional<double> getOutputSampleRate() override { return _report.outputSampleRate; }
  std::optional<double> getOutputChannelCount() override { return _report.outputChannelCount; }
  double getElapsedSeconds() override { return _report.elapsedSeconds; }
  double getSpeedRatio() override { return _report.speedRatio; }
  double getInputBytesRead() override { return _report.inputBytesRead; }
  double getSamplesProcessed() override { return _report.samplesProcessed; }
  std::vector<PlanWarning> getWarnings() override { return _report.warnings; }
  bool getWasCancelled() override { return _report.wasCancelled; }
  bool getDidCommitOutput() override { return _report.didCommitOutput; }
  void close() override {}

private:
  ReportData _report;
};

} // namespace margelo::nitro::transcoder
