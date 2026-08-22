import type { HybridObject } from 'react-native-nitro-modules'
import type { AudioCodecId, ContainerId } from './MediaTypes.nitro'
import type { PlanWarning } from './ResolvedPlan.nitro'

/** Terminal result of a job. Also produced (partially filled) on cancellation. */
export interface TranscodeReport extends HybridObject<{
  ios: 'c++'
  android: 'c++'
}> {
  readonly outputUri?: string
  readonly outputByteSize: number
  readonly outputDurationSeconds?: number
  readonly outputContainer: ContainerId
  readonly outputCodec?: AudioCodecId
  readonly outputSampleRate?: number
  readonly outputChannelCount?: number
  readonly elapsedSeconds: number
  /** Audio seconds produced per wall second. */
  readonly speedRatio: number
  readonly inputBytesRead: number
  readonly samplesProcessed: number
  readonly warnings: PlanWarning[]
  readonly wasCancelled: boolean
  /** `true` only when the output was fully written and committed. */
  readonly didCommitOutput: boolean
  close(): void
}
