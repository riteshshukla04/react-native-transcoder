/**
 * Shared, backend-neutral media value types.
 *
 * Nothing in this file names FFmpeg, AudioToolbox, MediaCodec, a platform or a
 * server. Callers describe *outcomes*; the native planner picks the backend.
 */

/** Stable engine identifier for an audio codec. */
export type AudioCodecId =
  | 'pcm-u8'
  | 'pcm-s16'
  | 'pcm-s24'
  | 'pcm-s32'
  | 'pcm-f32'
  | 'aac'
  | 'he-aac'
  | 'he-aac-v2'
  | 'mp3'
  | 'flac'
  | 'alac'
  | 'opus'
  | 'vorbis'
  | 'amr-nb'
  | 'amr-wb'
  | 'ac3'
  | 'eac3'
  | 'wavpack'
  | 'ape'
  | 'musepack'
  | 'speex'
  | 'wma'
  | 'dts'

/** Stable engine identifier for a container format. */
export type ContainerId =
  | 'wav'
  | 'aiff'
  | 'caf'
  | 'raw-pcm'
  | 'm4a'
  | 'mp4'
  | 'adts'
  | 'mp3'
  | 'flac'
  | 'ogg'
  | 'webm'
  | 'matroska'
  | 'amr'
  | 'ac3'
  | 'wavpack'
  | 'ape'
  | 'musepack'
  | 'asf'
  | 'rf64'
  | 'mpegts'

/** Decoded PCM sample format. `*-planar` variants keep one plane per channel. */
export type SampleFormat =
  | 'u8'
  | 's16'
  | 's32'
  | 'f32'
  | 'f64'
  | 'u8-planar'
  | 's16-planar'
  | 's32-planar'
  | 'f32-planar'
  | 'f64-planar'

/** What the caller wants the planner to optimise for. Never a backend name. */
export type OutcomePolicy =
  'balanced' | 'fastest' | 'highest-quality' | 'lowest-energy' | 'deterministic'

/** How container metadata is carried into the output. */
export type MetadataPolicy = 'copy' | 'drop' | 'replace' | 'merge'

/** Encoder rate control. */
export type BitRateMode = 'cbr' | 'vbr' | 'abr' | 'constrained-vbr'

/**
 * Strength of a requested output property.
 * - `preserve`: keep whatever the source has.
 * - `prefer`: best-effort; a mismatch degrades with a warning in the resolved plan.
 * - `require`: planning fails when the value cannot be produced exactly.
 */
export type ValuePolicy = 'preserve' | 'prefer' | 'require'

/** Which codec directions a capability entry covers. */
export type CodecDirection = 'decode' | 'encode' | 'decode-and-encode'

/** Engine-owned audio processors. Applied only when explicitly requested. */
export type ProcessorId =
  | 'sample-format-convert'
  | 'resample'
  | 'channel-map'
  | 'trim'
  | 'concat'
  | 'gain'
  | 'fade'
  | 'loudness-measure'
  | 'loudness-normalize'
  | 'limiter'
  | 'dither'

/** A single audio stream inside a probed asset. */
export interface AudioStreamDescriptor {
  streamId: number
  /** Engine codec id, absent when the source codec has no stable engine identifier. */
  codec?: AudioCodecId
  /** Always present, human-readable, for diagnostics and unsupported codecs. */
  codecName: string
  sampleRate: number
  channelCount: number
  channelLayoutName: string
  sampleFormat?: SampleFormat
  bitsPerSecond?: number
  durationSeconds?: number
  /** Encoder frame size in samples, when the codec declares one. */
  frameSize?: number
  isDefault: boolean
  language?: string
  /** Gapless facts. Preserved through remux; recomputed after decode-based paths. */
  encoderDelaySamples?: number
  encoderPaddingSamples?: number
}

/** A chapter marker in the source container. */
export interface ChapterDescriptor {
  id: number
  startSeconds: number
  endSeconds: number
  title?: string
}

/** One codec the current build can actually run on this device, right now. */
export interface CodecCapability {
  codec: AudioCodecId
  codecName: string
  direction: CodecDirection
  /** e.g. `LGPL-2.1-or-later`, `BSD-3-Clause`, `platform-framework`. */
  licenseClass: string
  /** Empty when the codec accepts arbitrary rates. */
  sampleRates: number[]
  maxChannelCount: number
  supportsVbr: boolean
}

/** One container the current build can mux, and what it accepts. */
export interface ContainerCapability {
  container: ContainerId
  codecs: AudioCodecId[]
  supportsMetadata: boolean
  supportsChapters: boolean
  supportsArtwork: boolean
  requiresSeekableOutput: boolean
}

/**
 * Immutable snapshot of what this build, on this device, can do right now.
 * Crosses JSI once so codec-picker UI reads plain JavaScript data.
 */
export interface MediaCapabilitiesSnapshot {
  engineVersion: string
  /** `mobile-core`, `mobile-extended`, `gpl` or a custom profile name. */
  profile: string
  /** Hash of the generated capability manifest this build was shipped with. */
  capabilityManifestHash: string
  audioDecoders: CodecCapability[]
  audioEncoders: CodecCapability[]
  containers: ContainerCapability[]
  processors: ProcessorId[]
  outcomes: OutcomePolicy[]
  /**
   * `false` when no validated energy measurements exist for this device class:
   * `lowest-energy` then resolves through `balanced` with a warning.
   */
  energyPolicyAvailable: boolean
  maxConcurrentJobs: number
  supportsVideo: boolean
}
