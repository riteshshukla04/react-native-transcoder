#pragma once

#include <string>

namespace margelo::nitro::transcoder {

// An app-private, writable directory. Resolved once, without any platform glue:
// TMPDIR on Apple platforms, the app's own cache directory on Android.
const std::string& scratchDirectory();

} // namespace margelo::nitro::transcoder
