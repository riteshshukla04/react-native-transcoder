import type { HybridObject } from 'react-native-nitro-modules'
import type { MediaAsset } from './MediaAsset.nitro'
import type {
  MediaDestination,
  FileDestinationOptions,
} from './MediaDestination.nitro'
import type {
  MediaSource,
  FileSourceOptions,
  UrlSourceOptions,
} from './MediaSource.nitro'
import type {
  AudioCodecId,
  ContainerId,
  MediaCapabilitiesSnapshot,
  MetadataPolicy,
  OutcomePolicy,
  SampleFormat,
  ValuePolicy,
  BitRateMode,
} from './MediaTypes.nitro'
import type { ResolvedPlan } from './ResolvedPlan.nitro'
import type { TranscodeJob } from './TranscodeJob.nitro'

/** What the audio side of a request should do. */
export type AudioRequestMode =
  /** Decode + encode into `codec`. */
  | 'encode'
  /** Compressed passthrough into a new container. Never decodes. */
  | 'copy'
  /** Produce PCM only. */
  | 'decode'

/**
 * Quality is a tagged shape: `mode` selects which of the payload fields apply.
 * Conflicting combinations (e.g. `copy` plus loudness normalization, or
 * `bitrate` on a lossless-only encoder) fail during planning, not at runtime.
 */
export type QualityMode = 'bitrate' | 'quality' | 'lossless' | 'copy'

export interface AudioQualityRequest {
  mode: QualityMode
  /** `mode: 'bitrate'` only. */
  bitsPerSecond?: number
  /** `mode: 'bitrate'` only. @default 'vbr' */
  bitRateMode?: BitRateMode
  /** `mode: 'quality'` only. Encoder-relative, 0..10. */
  level?: number
  /** `mode: 'lossless'` only. Encoder-relative compression effort. */
  compressionLevel?: number
}

export interface AudioSampleRateRequest {
  policy: ValuePolicy
  /** Required for `prefer` and `require`. */
  hertz?: number
}

export interface AudioChannelRequest {
  policy: ValuePolicy
  /** Required for `prefer` and `require`. */
  channelCount?: number
}

/** How gapless timing facts are treated. */
export type GaplessPolicy = 'preserve' | 'ignore' | 'require'

/** Which source streams to take. */
export interface TrackSelection {
  /** Empty selects the default audio stream. */
  audioStreamIds: number[]
}

export interface AudioTranscodeRequest {
  mode: AudioRequestMode
  container: ContainerId
  /** Required when `mode` is `'encode'`. */
  codec?: AudioCodecId
  quality?: AudioQualityRequest
  sampleRate?: AudioSampleRateRequest
  channels?: AudioChannelRequest
  sampleFormat?: SampleFormat
  tracks?: TrackSelection
}

export interface TranscodeRequest {
  audio: AudioTranscodeRequest
  /** @default 'copy' */
  metadata?: MetadataPolicy
  /** Applied for `replace` and `merge`. */
  metadataValues?: Record<string, string>
  /** @default 'balanced' */
  outcome?: OutcomePolicy
  /** @default 'preserve' */
  gapless?: GaplessPolicy
  /** Hard ceiling on pipeline-owned buffers. Planning fails if unreachable. */
  memoryLimitByteSize?: number
}

/**
 * The one autolinked root. Its constructor is trivial: importing this package
 * starts no workers, initializes no codecs and enumerates no platform codecs.
 * The first async call (or `prewarm`) initializes the engine once, off the JS thread.
 */
export interface MediaFactory extends HybridObject<{
  ios: 'c++'
  android: 'c++'
}> {
  /** Cheap static fact. Safe to read synchronously. */
  readonly version: string

  /**
   * An app-private directory the engine can write to: output files, staged
   * remote sources and temporary muxer files. Resolved once, natively.
   */
  readonly scratchDirectory: string

  /** Optional eager initialization, so a user gesture doesn't pay for it. */
  prewarm(): Promise<void>

  /** First call performs lazy discovery off the JS thread; later calls are cached. */
  getCapabilities(): Promise<MediaCapabilitiesSnapshot>

  openFileSource(options: FileSourceOptions): Promise<MediaSource>
  openUrlSource(options: UrlSourceOptions): Promise<MediaSource>
  /** Copies the buffer into native-owned immutable memory before returning. */
  openMemorySource(buffer: ArrayBuffer): MediaSource
  openFileDestination(
    options: FileDestinationOptions
  ): Promise<MediaDestination>

  /** Reads the minimum bytes needed. The result is cached on the source. */
  probe(source: MediaSource): Promise<MediaAsset>

  /** Runs the planner without creating a job. Reuses the cached probe record. */
  resolve(
    source: MediaSource,
    destination: MediaDestination,
    request: TranscodeRequest
  ): Promise<ResolvedPlan>

  /** Same planner as `resolve`, then the same validated construction path. */
  createTranscodeJob(
    source: MediaSource,
    destination: MediaDestination,
    request: TranscodeRequest
  ): Promise<TranscodeJob>

  createTranscodeJobFromPlan(
    source: MediaSource,
    destination: MediaDestination,
    plan: ResolvedPlan
  ): Promise<TranscodeJob>
}
