# Implementation decisions and deviations

Records where the shipped code deviates from the plan, and why. Each entry is reversible
before 1.0.

## D1 — Quality is a tagged struct, not a Nitro variant

The plan asks for a discriminated union for `AudioQualityRequest`. Nitro maps a TypeScript
union of object types onto `std::variant`, discriminated by runtime structural matching, not
by a literal tag. That makes arm selection order-dependent and fragile.

Shipped instead: one struct with a `mode` discriminant and optional payload fields, validated
natively. Invalid combinations still fail during planning, which is the guarantee the plan
actually asks for. Same for `sampleRate` / `channels`, which use `{ policy, value }` structs
rather than a `string | object` variant.

## D2 — `Media.scratchDirectory`

Added to `MediaFactory`. Callers need somewhere to write output, and the engine needs a place
for staged remote sources and temporary muxer files. It resolves natively: `TMPDIR` on Apple
platforms, the app's own cache directory on Android (derived from `/proc/self/cmdline`).

This avoids a `MediaPlatformContext` HybridObject with Swift and Kotlin glue. If an app ever
needs a different location, that glue is the upgrade path.

## D3 — Nitrogen types are used inside the engine

`engine/` includes generated types such as `MediaCapabilitiesSnapshot` and `OutcomePolicy`.
They are backend-neutral, so this costs nothing the plan cares about and removes an entire
enum-mapping layer. The rule that still holds: no FFmpeg, AudioToolbox, VideoToolbox,
MediaCodec, libcurl, Objective-C or Java types in `engine/`.

## D4 — Promises settle on the engine executor

Nitro offers `Promise<T>::async`, which uses Nitro's own pool. `runOnEngine` in
`cpp/hybrid/EngineTask.hpp` submits to the engine's executor instead, so the resource governor
will later see all media work. The executor itself is constructed on the first async call,
which is a thread-pool spawn, not codec enumeration.

## D5 — `jest` is pinned to v30 at the repo root

`@react-native/eslint-config` pulls in `eslint-plugin-jest`, whose optional `jest` peer
resolves to v29. With a fully hoisted install that shadows the v30 tree
`react-native-harness@1.4` needs, and Harness fails with
`this._moduleMocker.clearMocksOnScope is not a function`. The root `overrides` entry forces v30.

## D6 — A minimal `NitroTranscoderPackage.java` exists

The React Native CLI returns `platforms.android: null` for a package with no class implementing
`ReactPackage`, which silently drops the native library from the app. The stub exists only to
make autolinking resolve; it registers no modules.

## D7 — FFmpeg is linked dynamically on both platforms

Android gets four per-ABI sets of `libav*.so` staged into `dependencies/prebuilt/android/jniLibs`
and packaged as ordinary JNI libraries. iOS gets `avcodec`/`avformat`/`avutil`/`swresample`
XCFrameworks built as dylibs, rewritten to `@rpath/<name>.framework/<name>` and embedded by
CocoaPods. Both keep the LGPL components replaceable, which is what §7.2 of the plan prefers.

The 32-bit `x86` ABI is built with `--disable-asm`: FFmpeg's i686 assembly is not PIC-safe in a
shared build, and that ABI is emulator-only.

## D8 — Cancellation is mapped at one place

Cancelling makes FFmpeg fail wherever it happens to be — a read callback, a header write, a mux.
`runPlan` wraps the whole run and, when the token is set, reports `cancelled` instead of the
`probe-failed` / `mux-failed` symptom the specific call produced.

## Not built yet

- URL sources and the `NetworkBackend` platform adapters.
- Platform codec backends (AudioToolbox, MediaCodec) and therefore HE-AAC encode.
- `libmp3lame`, `libopus`, `libvorbis`, so MP3/Opus/Vorbis are decode-only.
- Audio processors beyond format conversion, resampling and channel mapping: trim, concat, gain,
  fade, loudness, limiter, dither.
- Artwork extraction (`MediaAsset.saveArtworkToFile`).
- Gapless trimming on decode paths: the AAC round-trip is ~20 ms longer than the source because
  encoder delay and padding are not yet compensated.
- `tools/media-bench` and `tools/capability-manifest`.
