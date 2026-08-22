# react-native-transcoder

Fully on-device media transcoding for React Native, built on one portable C++20 engine and
[Nitro Modules](https://github.com/mrousavy/nitro).
Decodes and encodes PCM, AAC-LC, FLAC, ALAC, MP3, Opus and Vorbis across WAV, AIFF, CAF, M4A, MP4,
ADTS, MP3, FLAC, Ogg and Matroska.
Nothing leaves the device — there is no transcoding server and no upload path — and the engine
reports at runtime exactly what the shipped binary can do.

## Install

```sh
bun add react-native-transcoder react-native-nitro-modules
cd ios && pod install
```

## API

`Media` is the single autolinked entry point.

| Member | Returns | Description |
|---|---|---|
| `Media.version` | `string` | Engine version. |
| `Media.scratchDirectory` | `string` | Where the engine stages temporary files. |
| `Media.prewarm()` | `Promise<void>` | Initialize the engine up front instead of on first use. |
| `Media.getCapabilities()` | `Promise<MediaCapabilitiesSnapshot>` | Codecs, containers and processors this build supports. |
| `Media.openFileSource(options)` | `Promise<MediaSource>` | Open a local file by `uri`. |
| `Media.openMemorySource(buffer)` | `MediaSource` | Open an `ArrayBuffer`. Synchronous. |
| `Media.openUrlSource(options)` | `Promise<MediaSource>` | **Not implemented yet** — throws `capability-not-met`. |
| `Media.openFileDestination(options)` | `Promise<MediaDestination>` | Output file, with an `overwrite` policy. |
| `Media.probe(source)` | `Promise<MediaAsset>` | Read container, streams, duration and metadata. |
| `Media.resolve(source, destination, request)` | `Promise<ResolvedPlan>` | Resolve a request into a plan without running it. |
| `Media.createTranscodeJob(source, destination, request)` | `Promise<TranscodeJob>` | Plan and build a runnable job. |
| `Media.createTranscodeJobFromPlan(source, destination, plan)` | `Promise<TranscodeJob>` | Build a job from an already-resolved plan. |

Each call returns a native-owned handle:

| Type | Key members |
|---|---|
| `MediaSource` | `kind`, `isSeekable`, `isProbed`, `byteLength?`, `close()` |
| `MediaDestination` | `kind`, `atomicity`, `close()` |
| `MediaAsset` | `container?`, `containerFormatName`, `audioStreams`, `durationSeconds?`, `byteSize?`, `hasVideo`, `chapters`, `artworkCount`, `getMetadata()`, `saveArtworkToFile()` (**not implemented yet**), `close()` |
| `ResolvedPlan` | `path`, `outputContainer`, `outputCodec?`, `outputSampleRate?`, `outputChannelCount?`, `outputBitsPerSecond?`, `estimatedOutputByteSize?`, `conversions`, `warnings`, `isTwoPass`, `toJson()`, `close()` |
| `TranscodeJob` | `state`, `run()`, `cancel()`, `addOnProgressListener(listener)`, `setProgressUpdatesPerSecond(n)`, `exportDiagnostics()`, `close()` |
| `TranscodeReport` | `outputUri?`, `outputByteSize`, `outputContainer`, `outputCodec?`, `outputDurationSeconds?`, `outputSampleRate?`, `outputChannelCount?`, `elapsedSeconds`, `speedRatio`, `samplesProcessed`, `warnings`, `wasCancelled`, `didCommitOutput`, `close()` |

## Usage

```ts
import { Media } from 'react-native-transcoder'

const source = await Media.openFileSource({ uri: inputUri })
const destination = await Media.openFileDestination({
  uri: outputUri,
  overwrite: 'replace-atomically',
})

const job = await Media.createTranscodeJob(source, destination, {
  audio: {
    mode: 'encode',
    codec: 'opus',
    container: 'ogg',
    quality: { mode: 'bitrate', bitsPerSecond: 160_000, bitRateMode: 'vbr' },
    sampleRate: { policy: 'prefer', hertz: 48_000 },
    channels: { policy: 'preserve' },
  },
  metadata: 'copy',
  outcome: 'fastest',
})

const progress = job.addOnProgressListener(console.log)
try {
  const report = await job.run()
  console.log(report.outputDurationSeconds)
} finally {
  progress.remove()
  job.close()
  destination.close()
  source.close()
}
```

Every resource is native-owned and has an idempotent `close()`. Close on every terminal path.

## License

MIT. Codec dependencies carry their own licenses; see [docs/licensing](docs/licensing/README.md).
