# API reference

`react-native-transcoder` exposes one runtime object, `Media`, plus a set of native-owned
resources it hands back. Everything runs on the device; nothing is uploaded anywhere.

```ts
import { Media } from 'react-native-transcoder'
```

- [Mental model](#mental-model)
- [`Media`](#media)
- [Sources](#sources)
- [Destinations](#destinations)
- [Probing — `MediaAsset`](#probing--mediaasset)
- [Planning — `ResolvedPlan`](#planning--resolvedplan)
- [Running — `TranscodeJob`](#running--transcodejob)
- [Result — `TranscodeReport`](#result--transcodereport)
- [Capabilities](#capabilities)
- [`TranscodeRequest`](#transcoderequest)
- [Errors](#errors)
- [Type reference](#type-reference)
- [Recipes](#recipes)

## Mental model

```
openFileSource / openUrlSource / openMemorySource ─┐
                                                   ├─► probe   → MediaAsset
openFileDestination ───────────────────────────────┤
                                                   ├─► resolve → ResolvedPlan
                                                   └─► createTranscodeJob → TranscodeJob
                                                                              │
                                                                    run() ────┴─► TranscodeReport
```

Four rules that the whole API follows:

1. **Every resource is native-owned and has an idempotent `close()`.** Close on every terminal
   path, including failures. Use `try/finally`.
2. **A job is single-use.** `run()` moves it `ready → running → completed | failed | cancelled`.
   A second `run()` throws.
3. **Requirements fail, preferences degrade.** A `require` policy that cannot be met fails during
   planning. A `prefer` policy that cannot be met still runs and records a warning on the plan.
4. **No work happens on the JS thread.** Every async method hands off to an engine worker.

## `Media`

The single autolinked root. Creating it starts no workers and enumerates no codecs.

### `Media.version: string`

Package version. Cheap, synchronous, safe to read at import time.

### `Media.scratchDirectory: string`

An app-private writable directory the engine also uses for temporary output files. `TMPDIR` on
Apple platforms, the app's own cache directory on Android.

```ts
const outputUri = `${Media.scratchDirectory}/converted.m4a`
```

### `Media.prewarm(): Promise<void>`

Optional. Initializes the engine once, off the JS thread, so a later user gesture does not pay
that cost. Every other async method initializes it transparently, so calling this is never
required.

### `Media.getCapabilities(): Promise<MediaCapabilitiesSnapshot>`

What this build, on this device, can actually do — see [Capabilities](#capabilities). The first
call performs lazy discovery on a worker; later calls return the cached snapshot. The whole
nested structure crosses JSI once, so codec-picker UI reads plain JavaScript data.

### Source and destination factories

| Method | Returns |
|---|---|
| `openFileSource(options: FileSourceOptions)` | `Promise<MediaSource>` |
| `openUrlSource(options: UrlSourceOptions)` | `Promise<MediaSource>` |
| `openMemorySource(buffer: ArrayBuffer)` | `MediaSource` (synchronous) |
| `openFileDestination(options: FileDestinationOptions)` | `Promise<MediaDestination>` |

### Pipeline entry points

| Method | Returns |
|---|---|
| `probe(source)` | `Promise<MediaAsset>` |
| `resolve(source, destination, request)` | `Promise<ResolvedPlan>` |
| `createTranscodeJob(source, destination, request)` | `Promise<TranscodeJob>` |
| `createTranscodeJobFromPlan(source, destination, plan)` | `Promise<TranscodeJob>` |

`createTranscodeJob` runs the *same* planner as `resolve` and then the same validated construction
path as `createTranscodeJobFromPlan`. There is one planning implementation; the two-step form only
exists so you can show the user what will happen before committing.

## Sources

A `MediaSource` is a native-owned input. The concrete implementation (file, memory, HTTP) stays
native; JavaScript sees one type.

```ts
interface MediaSource {
  readonly kind: 'file' | 'url' | 'memory' | 'file-descriptor' | 'callback'
  readonly isSeekable: boolean
  readonly byteLength?: number   // absent when the length is not known up front
  readonly isProbed: boolean     // true once a probe record is cached on this source
  close(): void
}
```

### `openFileSource`

```ts
const source = await Media.openFileSource({ uri: '/path/to/song.mp3' })
```

`uri` accepts a plain filesystem path or a `file://` URL (percent-decoded). Access is validated
before the promise resolves, so a missing file, a directory, or a permission problem fails here
with a typed error rather than mid-transcode.

### `openMemorySource`

```ts
const source = Media.openMemorySource(arrayBuffer)
```

Synchronous. The buffer is copied into native-owned immutable memory before returning, so later
JavaScript mutations cannot affect the source. Default ceiling 64 MiB; opening temporarily retains
roughly twice the input bytes.

### `openUrlSource`

```ts
const source = await Media.openUrlSource({
  url: 'https://media.example.com/source.flac',
  headers: { Authorization: `Bearer ${token}` },
  connectTimeoutMs: 10_000,
  readTimeoutMs: 30_000,
  maxRedirects: 3,
  cache: { mode: 'bounded', maxByteSize: 128 * 1024 * 1024 },
  networkAccess: 'public-only',
  sourceValidation: 'best-effort',
})
```

The device downloads the bytes and transcodes locally — a URL never means "upload it somewhere".

`networkAccess` defaults to `public-only`, which rejects loopback, link-local, private,
carrier-grade NAT and other reserved destinations after every DNS resolution and redirect. LAN/NAS
callers opt in with `private-and-public`.

`sourceValidation` controls how strictly a remote source's identity must be proven before a job
runs: `best-effort` (default) uses a strong validator when the server offers one and otherwise
falls back to a bounded range fingerprint plus a warning; `strict` rejects a source whose identity
cannot be proven.

> **Not implemented yet.** `openUrlSource` currently rejects with `capability-not-met`. The option
> shape is frozen; the network backends land with the URL milestone.

## Destinations

```ts
interface MediaDestination {
  readonly kind: 'file' | 'memory' | 'file-descriptor' | 'callback'
  readonly uri?: string
  readonly atomicity: OverwritePolicy
  close(): void
}
```

```ts
const destination = await Media.openFileDestination({
  uri: `${Media.scratchDirectory}/out.m4a`,
  overwrite: 'replace-atomically',
})
```

| `overwrite` | Behaviour |
|---|---|
| `fail-if-exists` (default) | Refuses to write over an existing file |
| `replace-atomically` | Writes a same-volume temporary file, then atomically replaces the target. Failure or cancellation removes only the temporary file and leaves the previous destination intact |
| `not-guaranteed` | Reserved for platform destinations that cannot promise atomicity. Never selected automatically; currently rejected |

Closing a destination that never committed removes the engine-created temporary file, so an
abandoned job leaves nothing behind.

## Probing — `MediaAsset`

```ts
const asset = await Media.probe(source)
```

Reads the minimum needed to identify streams, metadata and chapters, then caches the record on the
`MediaSource`. `resolve` and both job-creation paths reuse that record, so no media payload byte is
read twice.

```ts
interface MediaAsset {
  readonly durationSeconds?: number
  readonly container?: ContainerId       // absent for containers with no stable engine id
  readonly containerFormatName: string   // always present, for diagnostics
  readonly byteSize?: number
  readonly audioStreams: AudioStreamDescriptor[]
  readonly hasVideo: boolean
  readonly chapters: ChapterDescriptor[]
  readonly artworkCount: number

  getMetadata(): Record<string, string>
  saveArtworkToFile(index: number, path: string): Promise<string>
  close(): void
}
```

```ts
interface AudioStreamDescriptor {
  streamId: number
  codec?: AudioCodecId
  codecName: string
  sampleRate: number
  channelCount: number
  channelLayoutName: string
  sampleFormat?: SampleFormat
  bitsPerSecond?: number
  durationSeconds?: number
  frameSize?: number
  isDefault: boolean
  language?: string
  encoderDelaySamples?: number
  encoderPaddingSamples?: number
}
```

Artwork and large metadata are read lazily, so probing a file with embedded cover art does not
push the image across JSI.

## Planning — `ResolvedPlan`

```ts
const plan = await Media.resolve(source, destination, request)
```

A plan states exactly what will happen before anything runs. It never contains backend identities.

```ts
interface ResolvedPlan {
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
  readonly requiresSourceStaging: boolean
  readonly estimatedStagingByteSize?: number
  readonly isTwoPass: boolean
  toJson(): string
  close(): void
}
```

| `path` | Meaning |
|---|---|
| `packet-remux` | Compressed packets are copied into a new container. Never decodes |
| `metadata-remux` | Packet copy plus rewritten container metadata |
| `full-transcode` | Decode → process → encode → mux |
| `decode-only` | Produce PCM |
| `encode-only` | Accept caller-provided PCM |
| `probe-only` | Identify only |

`conversions` lists what the planner inserted to make adjacent stages agree (`resample`,
`channel-map`, `sample-format-convert`). `warnings` lists preferences that were degraded.

## Running — `TranscodeJob`

```ts
const job = await Media.createTranscodeJob(source, destination, request)
```

```ts
interface TranscodeJob {
  readonly state: 'ready' | 'running' | 'completed' | 'failed' | 'cancelled'
  run(): Promise<TranscodeReport>
  cancel(): void
  addOnProgressListener(listener: (progress: TranscodeProgress) => void): ProgressSubscription
  setProgressUpdatesPerSecond(updatesPerSecond: number): void
  exportDiagnostics(): Promise<string>
  close(): void
}
```

### Progress

```ts
const subscription = job.addOnProgressListener((progress) => {
  setPhase(progress.phase)
  if (progress.totalInputSeconds != null) {
    setFraction(progress.inputSecondsProcessed / progress.totalInputSeconds)
  }
})
```

```ts
interface TranscodeProgress {
  phase: 'starting' | 'staging-source' | 'reading' | 'transcoding' | 'flushing' | 'finalizing' | 'done'
  inputSecondsProcessed: number
  totalInputSeconds?: number          // absent when the source duration is unknown
  inputBytesRead: number
  outputBytesWritten: number
  speedRatio?: number                 // absent until enough observations exist
  estimatedSecondsRemaining?: number
}
```

Unknown values are **absent**, never reported as `0`. Updates are latest-value coalesced before
crossing JSI: 10 per second by default, clamped to `[1, 60]` by
`setProgressUpdatesPerSecond`. Phase transitions and the terminal `done` event are always
delivered, even when an intermediate sample is dropped.

Always `subscription.remove()` when you stop caring — the subscription is a flat
`{ remove(): void }`, so it cannot keep a finished job alive.

### Cancellation

`cancel()` is cooperative and idempotent, observed at every I/O and packet boundary. The pending
`run()` promise rejects with a `cancelled` error, `state` becomes `cancelled`, and the destination
is aborted so no partial output is published.

### Diagnostics

`exportDiagnostics()` returns a bounded, redacted ring buffer for support bundles. Paths, URLs and
credentials never appear in it.

## Result — `TranscodeReport`

```ts
interface TranscodeReport {
  readonly outputUri?: string
  readonly outputByteSize: number
  readonly outputDurationSeconds?: number
  readonly outputContainer: ContainerId
  readonly outputCodec?: AudioCodecId
  readonly outputSampleRate?: number
  readonly outputChannelCount?: number
  readonly elapsedSeconds: number
  readonly speedRatio: number      // audio seconds produced per wall second
  readonly inputBytesRead: number
  readonly samplesProcessed: number
  readonly warnings: PlanWarning[]
  readonly wasCancelled: boolean
  readonly didCommitOutput: boolean
  close(): void
}
```

`didCommitOutput` is the one to branch on: it is `true` only when the output was fully written and
atomically published.

## Capabilities

```ts
const capabilities = await Media.getCapabilities()
```

```ts
interface MediaCapabilitiesSnapshot {
  engineVersion: string
  profile: string                  // 'mobile-core' | 'mobile-extended' | 'gpl' | custom
  capabilityManifestHash: string
  audioDecoders: CodecCapability[]
  audioEncoders: CodecCapability[]
  containers: ContainerCapability[]
  processors: ProcessorId[]
  outcomes: OutcomePolicy[]
  energyPolicyAvailable: boolean
  maxConcurrentJobs: number
  supportsVideo: boolean
}

interface ContainerCapability {
  container: ContainerId
  codecs: AudioCodecId[]           // codecs this build can actually mux into it
  supportsMetadata: boolean
  supportsChapters: boolean
  supportsArtwork: boolean
  requiresSeekableOutput: boolean
}
```

Drive your format pickers from this, never from a hard-coded list — availability varies by build
profile, OS and device. Every advertised `container.codecs` pair is covered by an on-device test.

`energyPolicyAvailable` is `false` when no validated energy measurements exist for the current
device class; `outcome: 'lowest-energy'` then resolves through `balanced` and adds an
`energy-policy-unavailable` warning to the plan.

## `TranscodeRequest`

```ts
interface TranscodeRequest {
  audio: AudioTranscodeRequest
  metadata?: 'copy' | 'drop' | 'replace' | 'merge'   // default 'copy'
  metadataValues?: Record<string, string>            // required for 'replace' and 'merge'
  outcome?: OutcomePolicy                            // default 'balanced'
  gapless?: 'preserve' | 'ignore' | 'require'        // default 'preserve'
  memoryLimitByteSize?: number
}

interface AudioTranscodeRequest {
  mode: 'encode' | 'copy' | 'decode'
  container: ContainerId
  codec?: AudioCodecId              // required when mode is 'encode'
  quality?: AudioQualityRequest
  sampleRate?: AudioSampleRateRequest
  channels?: AudioChannelRequest
  sampleFormat?: SampleFormat
  tracks?: { audioStreamIds: number[] }   // empty selects the default audio stream
}
```

### `mode`

| Value | What happens |
|---|---|
| `encode` | Decode, convert as needed, encode into `codec`. Requires `codec` |
| `copy` | Compressed passthrough into `container`. Never decodes. Fails if the container cannot carry the source codec |
| `decode` | Produce PCM |

### `quality`

`mode` selects which payload fields apply. Invalid combinations fail during planning, before any
byte is read.

```ts
{ mode: 'bitrate', bitsPerSecond: 192_000, bitRateMode: 'vbr' }  // bitRateMode: cbr|vbr|abr|constrained-vbr
{ mode: 'quality', level: 6 }                                    // encoder-relative, 0..10
{ mode: 'lossless', compressionLevel: 8 }                        // encoder-relative effort
{ mode: 'copy' }                                                 // only valid with audio.mode 'copy'
```

A target bit rate on a lossless codec is ignored with a `preference-ignored` warning rather than
silently changing the output.

### `sampleRate` and `channels`

Both take a policy plus a value:

```ts
sampleRate: { policy: 'preserve' }
sampleRate: { policy: 'prefer', hertz: 48_000 }   // nearest supported rate, warns if it differs
sampleRate: { policy: 'require', hertz: 48_000 }  // planning fails if not reachable exactly
channels:   { policy: 'require', channelCount: 1 }
```

### `outcome`

`balanced` (default), `fastest`, `highest-quality`, `lowest-energy`, `deterministic`. These are
*outcomes*, not backends — you never name FFmpeg, AudioToolbox or MediaCodec.

## Errors

Native throws a plain `Error` whose message begins with `[<code>:<stage>:<retryable|final>]`.
Parse it into a typed shape:

```ts
import { parseTranscoderError, isTranscoderError } from 'react-native-transcoder'

try {
  await job.run()
} catch (error) {
  if (isTranscoderError(error, 'cancelled')) return
  const details = parseTranscoderError(error)
  console.warn(details?.code, details?.stage, details?.message, details?.retryable)
}
```

| Code | Meaning |
|---|---|
| `invalid-request` | The request is malformed or contradicts itself |
| `unsupported-combination` | Codec/container/option combination this build cannot produce |
| `capability-not-met` | A `require` policy or a feature that is not available here |
| `probe-failed` | The source could not be identified, or is corrupt |
| `decode-failed` / `process-failed` / `encode-failed` / `mux-failed` | Stage failures |
| `source-read-failed` / `destination-write-failed` | I/O failures |
| `permission-denied` | No access to the path |
| `resource-limit-exceeded` | A configured memory or size ceiling was hit |
| `resource-unavailable` | An execution resource was unavailable or lost |
| `cancelled` | `cancel()` was observed |
| `internal-error` | Invariant failure — please report it |

`stage` is one of `open`, `probe`, `plan`, `demux`, `decode`, `process`, `encode`, `mux`, `write`,
`engine`. Concrete backend causes stay in `exportDiagnostics()`, never in the public error.

## Type reference

**`AudioCodecId`** — `pcm-u8`, `pcm-s16`, `pcm-s24`, `pcm-s32`, `pcm-f32`, `aac`, `he-aac`,
`he-aac-v2`, `mp3`, `flac`, `alac`, `opus`, `vorbis`, `amr-nb`, `amr-wb`, `ac3`, `eac3`,
`wavpack`, `ape`, `musepack`, `speex`, `wma`, `dts`

**`ContainerId`** — `wav`, `aiff`, `caf`, `raw-pcm`, `m4a`, `mp4`, `adts`, `mp3`, `flac`, `ogg`,
`webm`, `matroska`, `amr`, `ac3`, `wavpack`, `ape`, `musepack`, `asf`, `rf64`, `mpegts`

**`SampleFormat`** — `u8`, `s16`, `s32`, `f32`, `f64` and their `-planar` variants

**`ProcessorId`** — `sample-format-convert`, `resample`, `channel-map`, `trim`, `concat`, `gain`,
`fade`, `loudness-measure`, `loudness-normalize`, `limiter`, `dither`

The union types are the full engine vocabulary. What a given build can run is whatever
`getCapabilities()` reports.

## Not implemented yet

The shapes below are frozen and type-checked, but the current build does not act on them. Each
either fails with a typed error or is accepted and ignored — none of them silently change output.

| API | Status |
|---|---|
| `Media.openUrlSource` | Rejects with `capability-not-met`. Option shape is final |
| `MediaAsset.saveArtworkToFile` | Rejects with `capability-not-met`; `artworkCount` is reported correctly |
| `TranscodeRequest.gapless` | Accepted and ignored. Decode paths do not yet trim encoder delay/padding, so an AAC round-trip runs ~20 ms long |
| `TranscodeRequest.memoryLimitByteSize` | Accepted and ignored; pipeline buffers are bounded but not caller-configurable |
| `AudioTranscodeRequest.sampleFormat` | Accepted and ignored; the encoder's own format is chosen |
| `outcome` | Recorded on the plan, but there is a single software backend to choose from, so it does not change execution. `lowest-energy` adds an `energy-policy-unavailable` warning |
| `quality: { mode: 'quality' }` | Falls back to the encoder default bit rate with a `preference-ignored` warning |
| `overwrite: 'not-guaranteed'` | Rejected with `unsupported-combination` until a destination type needs it |
| Processors beyond `resample` / `channel-map` / `sample-format-convert` | Declared in `ProcessorId`, not yet runnable |

## Recipes

### Transcode a local file with progress

```ts
const source = await Media.openFileSource({ uri: inputUri })
const destination = await Media.openFileDestination({
  uri: `${Media.scratchDirectory}/out.m4a`,
  overwrite: 'replace-atomically',
})

const job = await Media.createTranscodeJob(source, destination, {
  audio: {
    mode: 'encode',
    codec: 'aac',
    container: 'm4a',
    quality: { mode: 'bitrate', bitsPerSecond: 192_000, bitRateMode: 'vbr' },
    sampleRate: { policy: 'preserve' },
    channels: { policy: 'preserve' },
  },
  metadata: 'copy',
  outcome: 'balanced',
})

const subscription = job.addOnProgressListener(setProgress)
try {
  const report = await job.run()
  console.log(report.outputUri, report.outputDurationSeconds, report.speedRatio)
} finally {
  subscription.remove()
  job.close()
  destination.close()
  source.close()
}
```

### Remux without re-encoding

```ts
const plan = await Media.resolve(source, destination, {
  audio: { mode: 'copy', container: 'adts' },
})
console.log(plan.path) // 'packet-remux' — no decode happens
const job = await Media.createTranscodeJobFromPlan(source, destination, plan)
```

### Show the user what will happen, then commit

```ts
const plan = await Media.resolve(source, destination, request)
const summary = {
  path: plan.path,
  output: `${plan.outputCodec} in ${plan.outputContainer}`,
  conversions: plan.conversions.map((c) => c.processor),
  warnings: plan.warnings.map((w) => w.message),
}
if (await confirmWithUser(summary)) {
  const job = await Media.createTranscodeJobFromPlan(source, destination, plan)
  await job.run()
}
```

### Decode anything to WAV

```ts
const job = await Media.createTranscodeJob(source, destination, {
  audio: { mode: 'encode', codec: 'pcm-s16', container: 'wav' },
})
```

### Normalise to 48 kHz mono

```ts
const job = await Media.createTranscodeJob(source, destination, {
  audio: {
    mode: 'encode',
    codec: 'aac',
    container: 'm4a',
    sampleRate: { policy: 'require', hertz: 48_000 },
    channels: { policy: 'require', channelCount: 1 },
  },
})
```

### Build a format picker from real capabilities

```ts
const capabilities = await Media.getCapabilities()
const containers = capabilities.containers.map((entry) => entry.container)
const codecsFor = (container: ContainerId) =>
  capabilities.containers.find((entry) => entry.container === container)?.codecs ?? []
```
