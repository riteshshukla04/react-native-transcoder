#pragma once

#include <exception>
#include <optional>
#include <stdexcept>
#include <string>

namespace margelo::nitro::transcoder {

// Stable public error taxonomy. These strings are part of the API contract and
// are parsed back into a typed shape by `parseTranscoderError` in TypeScript.
namespace error_code {
inline constexpr auto INVALID_REQUEST = "invalid-request";
inline constexpr auto UNSUPPORTED_COMBINATION = "unsupported-combination";
inline constexpr auto CAPABILITY_NOT_MET = "capability-not-met";
inline constexpr auto PROBE_FAILED = "probe-failed";
inline constexpr auto DECODE_FAILED = "decode-failed";
inline constexpr auto PROCESS_FAILED = "process-failed";
inline constexpr auto ENCODE_FAILED = "encode-failed";
inline constexpr auto MUX_FAILED = "mux-failed";
inline constexpr auto SOURCE_READ_FAILED = "source-read-failed";
inline constexpr auto DESTINATION_WRITE_FAILED = "destination-write-failed";
inline constexpr auto PERMISSION_DENIED = "permission-denied";
inline constexpr auto RESOURCE_LIMIT_EXCEEDED = "resource-limit-exceeded";
inline constexpr auto RESOURCE_UNAVAILABLE = "resource-unavailable";
inline constexpr auto CANCELLED = "cancelled";
inline constexpr auto INTERNAL_ERROR = "internal-error";
} // namespace error_code

namespace error_stage {
inline constexpr auto OPEN = "open";
inline constexpr auto PROBE = "probe";
inline constexpr auto PLAN = "plan";
inline constexpr auto DEMUX = "demux";
inline constexpr auto DECODE = "decode";
inline constexpr auto PROCESS = "process";
inline constexpr auto ENCODE = "encode";
inline constexpr auto MUX = "mux";
inline constexpr auto WRITE = "write";
inline constexpr auto ENGINE = "engine";
} // namespace error_stage

class TranscoderException final : public std::runtime_error {
public:
  TranscoderException(std::string code, std::string stage, const std::string& message, bool retryable = false,
                      std::optional<double> streamId = std::nullopt)
      : std::runtime_error("[" + code + ":" + stage + ":" + (retryable ? "retryable" : "final") + "] " + message),
        _code(std::move(code)), _stage(std::move(stage)), _retryable(retryable), _streamId(streamId) {}

  const std::string& code() const noexcept { return _code; }
  const std::string& stage() const noexcept { return _stage; }
  bool isRetryable() const noexcept { return _retryable; }
  std::optional<double> streamId() const noexcept { return _streamId; }

private:
  std::string _code;
  std::string _stage;
  bool _retryable;
  std::optional<double> _streamId;
};

} // namespace margelo::nitro::transcoder
