import type { HybridObject } from 'react-native-nitro-modules'

export type MediaDestinationKind =
  'file' | 'memory' | 'file-descriptor' | 'callback'

/**
 * - `replace-atomically`: write a same-volume temp file, then atomically replace.
 *   Failure or cancellation leaves the previous destination untouched.
 * - `fail-if-exists`: refuse to write over an existing file.
 * - `not-guaranteed`: the platform cannot promise atomicity (never mapped
 *   automatically — the caller must ask for it).
 */
export type OverwritePolicy =
  'replace-atomically' | 'fail-if-exists' | 'not-guaranteed'

export interface FileDestinationOptions {
  /** Local filesystem path or `file://` URL. */
  uri: string
  /** @default 'fail-if-exists' */
  overwrite?: OverwritePolicy
}

/** A native-owned output resource. */
export interface MediaDestination extends HybridObject<{
  ios: 'c++'
  android: 'c++'
}> {
  readonly kind: MediaDestinationKind
  readonly uri?: string
  readonly atomicity: OverwritePolicy
  /** Idempotent. Removes an engine-created temporary file if the job never committed. */
  close(): void
}
