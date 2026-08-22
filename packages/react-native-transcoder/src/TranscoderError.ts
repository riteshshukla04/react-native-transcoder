/** Stable, public error categories. Backend causes stay in support diagnostics. */
export type TranscoderErrorCode =
  | 'invalid-request'
  | 'unsupported-combination'
  | 'capability-not-met'
  | 'probe-failed'
  | 'decode-failed'
  | 'process-failed'
  | 'encode-failed'
  | 'mux-failed'
  | 'source-read-failed'
  | 'destination-write-failed'
  | 'permission-denied'
  | 'resource-limit-exceeded'
  | 'resource-unavailable'
  | 'cancelled'
  | 'internal-error'

export type TranscoderErrorStage =
  | 'open'
  | 'probe'
  | 'plan'
  | 'demux'
  | 'decode'
  | 'process'
  | 'encode'
  | 'mux'
  | 'write'
  | 'engine'

export interface TranscoderErrorDetails {
  code: TranscoderErrorCode
  stage: TranscoderErrorStage
  /** Safe to show a user. */
  message: string
  /** Redacted technical detail for logs. */
  detail?: string
  streamId?: number
  retryable: boolean
}

const PREFIX = /^\[([a-z-]+):([a-z-]+):(retryable|final)]\s*/

/**
 * Native throws a plain `Error` whose message starts with
 * `[<code>:<stage>:<retryable|final>] <message>`. This parses it back into a
 * typed shape without needing a custom JSI error class.
 */
export function parseTranscoderError(
  error: unknown
): TranscoderErrorDetails | undefined {
  if (!(error instanceof Error)) return undefined
  const match = PREFIX.exec(error.message)
  if (match == null) return undefined
  return {
    code: match[1] as TranscoderErrorCode,
    stage: match[2] as TranscoderErrorStage,
    retryable: match[3] === 'retryable',
    message: error.message.replace(PREFIX, ''),
  }
}

export function isTranscoderError(
  error: unknown,
  code?: TranscoderErrorCode
): boolean {
  const parsed = parseTranscoderError(error)
  if (parsed == null) return false
  return code == null || parsed.code === code
}
