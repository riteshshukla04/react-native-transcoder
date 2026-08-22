# react-native-transcoder

Fully on-device media transcoding for React Native, built on one portable C++20 engine and
[Nitro Modules](https://github.com/mrousavy/nitro).

Audio is the first product. Video shares the same lifecycle, I/O, packet and job model.

> **Status: alpha.** Probing, planning, full transcode, packet remux, progress, cancellation and
> atomic output work on device on Android and iOS, backed by a pinned LGPL FFmpeg 7.1.1 audio
> profile. Wave A is partially covered (see the codec table); URL sources, platform codec backends
> and the audio processors beyond resample/channel-map/format-convert are not implemented yet.

## Verified

A 2-second 44.1 kHz mono WAV generated in JavaScript, transcoded on device to AAC in M4A, then
decoded back:

| Check | Result |
|---|---|
| Reference `ffprobe` on the device output | `aac`, 44100 Hz, mono, 2.000000 s, 129968 bps |
| Dominant frequency after decode | 440.0 Hz (source: 440 Hz) |
| RMS after decode | 18534 vs 18536 in the source |
| On-device round-trip WAV vs host `ffmpeg` decode | 89088 / 89088 samples identical |
| Android output vs iOS output | byte-identical |

## Supported architectures

| Platform | Architectures | Runtime tested |
|---|---|---|
| Android | `arm64-v8a`, `armeabi-v7a`, `x86`, `x86_64` | arm64 device + emulator locally, `x86_64` emulator in CI |
| iOS | device `arm64`, simulator `arm64` + `x86_64` | simulator `arm64` locally; device build verified in CI |

FFmpeg is cross-built per architecture by `scripts/build-ffmpeg-android.sh` and
`scripts/build-ffmpeg-ios.sh`. Android ships four 16 KB-page-aligned `.so` sets; iOS ships
dynamic XCFrameworks embedded in the app bundle.

## Codecs in this build

| Codec | Decode | Encode |
|---|---|---|
| PCM (u8/s16/s24/s32/f32) | yes | yes |
| AAC-LC | yes | yes |
| FLAC | yes | yes |
| ALAC | yes | yes |
| MP3 | yes | not yet (needs `libmp3lame`) |
| Opus, Vorbis | yes | not yet (needs `libopus` / `libvorbis`) |

Containers: WAV, AIFF, CAF, M4A, MP4, ADTS, FLAC, Ogg, Matroska. Ask the engine rather than this
table — `Media.getCapabilities()` reports what the shipped binary can actually do.

## What it is

- One TypeScript contract, one shared C++ implementation, Android and iOS.
- Everything runs on the user's device. There is no transcoding server and no upload path.
- Callers request outcomes (`fastest`, `highest-quality`, …) — never FFmpeg, AudioToolbox or MediaCodec.
- Runtime capability discovery: the engine reports exactly what this build, on this device, can do.

## Install

```sh
bun add react-native-transcoder react-native-nitro-modules
cd ios && pod install
```

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

## Repository layout

```
packages/react-native-transcoder/   the npm package
  src/specs/                        .nitro.ts HybridObject specs (the frozen public API)
  cpp/engine/                       portable engine: errors, executor, byte I/O, paths
  cpp/hybrid/                       HybridObject implementations
  cpp/backends/                     ffmpeg / apple / android backends (not yet populated)
  nitrogen/generated/               nitrogen output — never edited by hand
example/                            React Native example app + on-device Harness suite
docs/                               architecture, API, codec and licensing notes
```

## Development

```sh
bun install                          # hoisted install at the repo root
./scripts/build-ffmpeg-android.sh    # cross-build FFmpeg for all four Android ABIs
./scripts/build-ffmpeg-ios.sh        # cross-build the iOS XCFrameworks
bun run specs                        # regenerate nitrogen bindings from the .nitro.ts specs
bun run typecheck
bun run lint                # eslint + prettier
bun run lint-cpp            # clang-format
```

On-device tests run through `react-native-harness`:

```sh
cd example
bun run test:harness --harnessRunner ios             # booted simulator
bun run test:harness --harnessRunner android         # emulator
bun run test:harness --harnessRunner android-device  # physical device
```

CI is the source of truth for pass/fail. See `.github/workflows/`.

## License

MIT for this package. Codec dependencies carry their own licenses; see `docs/licensing/`.
