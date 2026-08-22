import type { HybridObject } from 'react-native-nitro-modules'
import type { TranscodeReport } from './TranscodeReport.nitro'

/**
 * A job is single-use. `run()` moves `ready` -> `running` -> one terminal state.
 */
export type JobState =
  'ready' | 'running' | 'completed' | 'failed' | 'cancelled'

/** Coarse phase, for UI. Always delivered on transition, never coalesced away. */
export type JobPhase =
  | 'starting'
  | 'staging-source'
  | 'reading'
  | 'transcoding'
  | 'flushing'
  | 'finalizing'
  | 'done'

/**
 * Latest-value coalesced before crossing JSI. Unknown totals are absent,
 * never reported as zero.
 */
export interface TranscodeProgress {
  phase: JobPhase
  inputSecondsProcessed: number
  /** Absent for a source with unknown duration. */
  totalInputSeconds?: number
  inputBytesRead: number
  outputBytesWritten: number
  /** Absent until enough observations exist. */
  speedRatio?: number
  estimatedSecondsRemaining?: number
}

/** Flat, so removing a listener never keeps a HybridObject alive. */
export interface ProgressSubscription {
  remove: () => void
}

export interface TranscodeJob extends HybridObject<{
  ios: 'c++'
  android: 'c++'
}> {
  readonly state: JobState
  /** Runs on an engine-owned worker. Rejects on a second call. */
  run(): Promise<TranscodeReport>
  /** Cooperative and idempotent. Observed within 100ms at engine boundaries. */
  cancel(): void
  addOnProgressListener(
    listener: (progress: TranscodeProgress) => void
  ): ProgressSubscription
  /** Clamped to [1, 60]. @default 10 */
  setProgressUpdatesPerSecond(updatesPerSecond: number): void
  /** Bounded diagnostic ring buffer, redacted, for support bundles. */
  exportDiagnostics(): Promise<string>
  close(): void
}
