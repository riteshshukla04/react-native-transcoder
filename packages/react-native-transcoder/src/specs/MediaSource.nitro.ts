import type { HybridObject } from 'react-native-nitro-modules'

/** Where a source's bytes come from. Concrete implementations stay native-only. */
export type MediaSourceKind =
  'file' | 'url' | 'memory' | 'file-descriptor' | 'callback'

/**
 * How strictly a remote source's identity must be proven before a job runs.
 * - `best-effort`: strong validator when the server offers one, otherwise a
 *   bounded length + first/last range fingerprint, with a warning.
 * - `strict`: a remote source whose identity cannot be proven is rejected.
 */
export type SourceValidation = 'best-effort' | 'strict'

/**
 * Which network destinations an ordinary URL source may reach.
 * `public-only` is the default: it stops an untrusted media URL from becoming
 * an internal-network probe. LAN/NAS callers opt in explicitly.
 */
export type NetworkAccess = 'public-only' | 'private-and-public'

/** Bounded on-device caching policy for a URL source. */
export type CacheMode = 'none' | 'bounded'

export interface UrlCachePolicy {
  mode: CacheMode
  maxByteSize?: number
}

export interface FileSourceOptions {
  /** Local filesystem path, `file://` URL, or (Android) a `content://` URI. */
  uri: string
}

export interface UrlSourceOptions {
  /** `http://` or `https://`. The device downloads and transcodes locally. */
  url: string
  headers?: Record<string, string>
  connectTimeoutMs?: number
  readTimeoutMs?: number
  maxRedirects?: number
  cache?: UrlCachePolicy
  /** @default 'public-only' */
  networkAccess?: NetworkAccess
  /** @default 'best-effort' */
  sourceValidation?: SourceValidation
}

/**
 * A native-owned input resource. One active reader at a time; the first
 * successful probe is cached and reused by `resolve` and job creation.
 */
export interface MediaSource extends HybridObject<{
  ios: 'c++'
  android: 'c++'
}> {
  readonly kind: MediaSourceKind
  /** `false` for a forward-only HTTP source whose server has no range support. */
  readonly isSeekable: boolean
  /** Absent when the length is not known up front. */
  readonly byteLength?: number
  /** `true` once a probe record has been cached for this source. */
  readonly isProbed: boolean
  /** Idempotent. Releases file descriptors, sockets and cached bytes. */
  close(): void
}
