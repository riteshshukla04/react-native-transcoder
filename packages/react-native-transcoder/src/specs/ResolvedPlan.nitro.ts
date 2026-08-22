import type { HybridObject } from 'react-native-nitro-modules'
import type { AudioCodecId, ContainerId, ProcessorId } from './MediaTypes.nitro'

/** The graph the planner picked. `packet-remux` never decodes. */
export type PlanPath =
  | 'probe-only'
  | 'metadata-remux'
  | 'packet-remux'
  | 'decode-only'
  | 'encode-only'
  | 'full-transcode'

export type PlanWarningCode =
  | 'preference-ignored'
  | 'sample-rate-converted'
  | 'channel-layout-converted'
  | 'sample-format-converted'
  | 'bit-rate-clamped'
  | 'metadata-not-representable'
  | 'chapters-dropped'
  | 'artwork-dropped'
  | 'gapless-info-lost'
  | 'energy-policy-unavailable'
  | 'source-identity-not-guaranteed'
  | 'source-staged-to-disk'
  | 'overlap-not-possible'

export interface PlanWarning {
  code: PlanWarningCode
  message: string
  streamId?: number
}

/** A conversion the planner inserted to make two adjacent stages agree. */
export interface PlanConversion {
  processor: ProcessorId
  reason: string
  streamId?: number
}

/**
 * Exactly what will happen, produced before any job runs. Contains no backend
 * identities — concrete backend choices live in opt-in support diagnostics.
 */
export interface ResolvedPlan extends HybridObject<{
  ios: 'c++'
  android: 'c++'
}> {
  readonly path: PlanPath
  readonly selectedStreamIds: number[]
  readonly outputContainer: ContainerId
  readonly outputCodec?: AudioCodecId
  readonly outputSampleRate?: number
  readonly outputChannelCount?: number
  readonly outputBitsPerSecond?: number
  readonly estimatedOutputByteSize?: number
  readonly conversions: PlanConversion[]
  readonly warnings: PlanWarning[]
  /** `true` when a non-seekable source must be spooled to a bounded temp file. */
  readonly requiresSourceStaging: boolean
  readonly estimatedStagingByteSize?: number
  readonly isTwoPass: boolean
  /** Stable JSON of the whole plan, for logging and golden tests. */
  toJson(): string
  close(): void
}
