#pragma once

#include "media/ByteIo.hpp"
#include "media/MediaModel.hpp"

namespace margelo::nitro::transcoder::ffmpeg {

// Reads the minimum needed to identify streams, metadata and chapters.
// Leaves the source rewound so the same MediaSource can be transcoded.
ProbeResult probeSource(ByteSource& source);

} // namespace margelo::nitro::transcoder::ffmpeg
