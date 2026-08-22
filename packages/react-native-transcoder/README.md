# react-native-transcoder

Fully on-device audio transcoding for React Native, built on one portable C++20 engine and
[Nitro Modules](https://github.com/mrousavy/nitro).

```sh
bun add react-native-transcoder react-native-nitro-modules
cd ios && pod install
```

```ts
import { Media } from 'react-native-transcoder'

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
  },
})

const progress = job.addOnProgressListener(console.log)
try {
  const report = await job.run()
  console.log(report.outputUri, report.outputDurationSeconds)
} finally {
  progress.remove()
  job.close()
  destination.close()
  source.close()
}
```

- Everything runs on the device. There is no transcoding server and no upload path.
- You request outcomes (`fastest`, `highest-quality`, …), never a backend.
- `Media.getCapabilities()` reports exactly what this build, on this device, can do.

**[Full API reference →](https://github.com/riteshshukla04/react-native-transcoder/blob/main/docs/api/README.md)**

MIT. FFmpeg is LGPL-2.1-or-later and is linked dynamically; see the repository's licensing notes.
