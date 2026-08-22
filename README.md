# react-native-transcoder

Fully on-device media transcoding for React Native, built on one portable C++20 engine and
[Nitro Modules](https://github.com/mrousavy/nitro).

Audio is the first product. Video shares the same lifecycle, I/O, packet and job model.

> **Status: alpha.** Probing, planning, full transcode, packet remux, progress, cancellation and
> atomic output work on device on Android and iOS, backed by a pinned LGPL FFmpeg 7.1.1 audio
> profile. Wave A is partially covered (see the codec table); URL sources, platform codec backends
> and the audio processors beyond resample/channel-map/format-convert are not implemented yet.

## Verified

Not just "the API returns something" — the produced media is checked against reference ffmpeg.

| Check | Result |
|---|---|
| Every advertised codec/container pair, encoded and re-probed on device | **42 / 42** |
| On-device Harness suite (iOS sim, Android emulator, physical Android) | **45 / 45** on each |
| Device-produced files probed and fully decoded by host `ffmpeg` | **108 / 108** |
| Synthetic tone: device round-trip WAV vs host decode | 89088 / 89088 samples identical |
| Android output vs iOS output for the same input | byte-identical |
| Real 8 s 48 kHz stereo MP3 → AAC/M4A | 8.000 s, 384 000 samples, 11.3× realtime |
| …its tone energies at 220/277/330 Hz vs the source | within 0.1%, no off-band artifacts |
| Real AAC/M4A → FLAC on device vs host decode of the source | bit-for-bit identical |
| Metadata `copy` / `replace` / `merge` / `drop`, including through a packet remux | verified per tag |

Manual QA runs through the example app's UI on a physical device — probe, transcode, live
progress, cancel, and an in-app "run the full matrix" button — not only through the test harness.
`scripts/push-test-media.sh` puts real MP3/AAC/ALAC/FLAC/Opus/24-bit-WAV files on the device for it.

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

## Documentation

- [API reference](docs/api/README.md) — every method, option and type, with recipes.
- [Architecture decisions](docs/architecture/DECISIONS.md) — deviations from the plan and why.
- [Licensing](docs/licensing/README.md) — FFmpeg obligations and the release checklist.

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
