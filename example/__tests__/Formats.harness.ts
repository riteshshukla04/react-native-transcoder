import { describe, it, expect } from 'react-native-harness'
import { Media } from 'react-native-transcoder'
import type {
  AudioCodecId,
  ContainerId,
  MediaSource,
  TranscodeReport,
} from 'react-native-transcoder'
import {
  MP3_BASE64,
  OGG_OPUS_BASE64,
  OGG_VORBIS_BASE64,
  WEBM_OPUS_BASE64,
} from '../src/fixtures/media'
import { base64ToArrayBuffer, makeWav, outputPath } from '../src/lib/audio'

const RATE = 44100
const SECONDS = 1

/** A probed output reports the demuxer family, not the muxer that wrote it. */
const PROBED_AS: Partial<Record<ContainerId, ContainerId>> = {
  mp4: 'm4a',
  webm: 'matroska',
  rf64: 'rf64',
}

const EXTENSION: Record<string, string> = {
  wav: 'wav',
  aiff: 'aiff',
  caf: 'caf',
  m4a: 'm4a',
  mp4: 'mp4',
  adts: 'aac',
  mp3: 'mp3',
  flac: 'flac',
  ogg: 'ogg',
  webm: 'webm',
  matroska: 'mka',
  rf64: 'w64',
}

async function transcode(
  source: MediaSource,
  name: string,
  codec: AudioCodecId,
  container: ContainerId
): Promise<TranscodeReport> {
  const destination = await Media.openFileDestination({
    uri: outputPath(name),
    overwrite: 'replace-atomically',
  })
  const job = await Media.createTranscodeJob(source, destination, {
    audio: { mode: 'encode', codec, container },
  })
  try {
    return await job.run()
  } finally {
    job.close()
    destination.close()
  }
}

describe('every advertised codec/container pair', () => {
  it('encodes, muxes and probes back for each combination the build reports', async () => {
    const capabilities = await Media.getCapabilities()
    const pairs = capabilities.containers.flatMap((container) =>
      container.codecs.map((codec) => ({
        container: container.container,
        codec,
      }))
    )
    expect(pairs.length).toBeGreaterThan(10)

    const covered: string[] = []
    const failed: string[] = []
    for (const pair of pairs) {
      const label = `${pair.codec}-in-${pair.container}`
      const source = Media.openMemorySource(makeWav({ seconds: SECONDS }))
      try {
        const name = `matrix-${label}.${EXTENSION[pair.container] ?? 'bin'}`
        const report = await transcode(source, name, pair.codec, pair.container)
        expect(report.didCommitOutput).toBe(true)
        expect(report.outputByteSize).toBeGreaterThan(100)

        // Raw ADTS and Matroska carry no exact duration, so only re-probe the
        // stream facts there.
        const readback = await Media.openFileSource({ uri: outputPath(name) })
        const asset = await Media.probe(readback)
        expect(asset.audioStreams.length).toStrictEqual(1)
        expect(asset.audioStreams[0]!.codec).toStrictEqual(pair.codec)
        // Opus only encodes at its own rates, so the muxed file is checked
        // against the rate the engine reported rather than the source rate.
        expect(asset.audioStreams[0]!.sampleRate).toStrictEqual(
          report.outputSampleRate ?? RATE
        )
        expect(asset.container).toStrictEqual(
          PROBED_AS[pair.container] ?? pair.container
        )
        asset.close()
        readback.close()
        covered.push(label)
      } catch (error) {
        failed.push(`${label}: ${String(error)}`)
      } finally {
        source.close()
      }
    }

    // Advertising a combination the build cannot actually produce is a bug.
    expect(failed.join(' | ')).toStrictEqual('')
    expect(covered.length).toStrictEqual(pairs.length)
    console.log(`covered ${covered.length} pairs: ${covered.join(', ')}`)
  })
})

describe('decode-only source formats', () => {
  const fixtures: Array<{
    name: string
    base64: string
    codec: AudioCodecId
    container: ContainerId
    channels: number
  }> = [
    {
      name: 'mp3',
      base64: MP3_BASE64,
      codec: 'mp3',
      container: 'mp3',
      channels: 1,
    },
    {
      name: 'ogg-vorbis',
      base64: OGG_VORBIS_BASE64,
      codec: 'vorbis',
      container: 'ogg',
      channels: 2,
    },
    {
      name: 'ogg-opus',
      base64: OGG_OPUS_BASE64,
      codec: 'opus',
      container: 'ogg',
      channels: 1,
    },
    {
      name: 'webm-opus',
      base64: WEBM_OPUS_BASE64,
      codec: 'opus',
      container: 'matroska',
      channels: 1,
    },
  ]

  for (const fixture of fixtures) {
    it(`probes and decodes ${fixture.name}`, async () => {
      const source = Media.openMemorySource(base64ToArrayBuffer(fixture.base64))
      const asset = await Media.probe(source)

      expect(asset.container).toStrictEqual(fixture.container)
      expect(asset.audioStreams.length).toStrictEqual(1)
      expect(asset.audioStreams[0]!.codec).toStrictEqual(fixture.codec)
      expect(asset.audioStreams[0]!.channelCount).toStrictEqual(
        fixture.channels
      )
      asset.close()

      const report = await transcode(
        source,
        `decoded-${fixture.name}.wav`,
        'pcm-s16',
        'wav'
      )
      expect(report.didCommitOutput).toBe(true)
      expect(
        Math.abs((report.outputDurationSeconds ?? 0) - SECONDS)
      ).toBeLessThan(0.2)

      const readback = await Media.openFileSource({
        uri: outputPath(`decoded-${fixture.name}.wav`),
      })
      const decoded = await Media.probe(readback)
      expect(decoded.audioStreams[0]!.codec).toStrictEqual('pcm-s16')
      decoded.close()
      readback.close()
      source.close()
    })
  }
})

describe('format conversions', () => {
  it('resamples 44100 to 48000', async () => {
    const source = Media.openMemorySource(makeWav({ seconds: SECONDS }))
    const destination = await Media.openFileDestination({
      uri: outputPath('convert-48k.wav'),
      overwrite: 'replace-atomically',
    })
    const plan = await Media.resolve(source, destination, {
      audio: {
        mode: 'encode',
        codec: 'pcm-s16',
        container: 'wav',
        sampleRate: { policy: 'require', hertz: 48000 },
      },
    })
    expect(plan.outputSampleRate).toStrictEqual(48000)
    expect(plan.conversions.map((c) => c.processor)).toContain('resample')

    const job = await Media.createTranscodeJobFromPlan(
      source,
      destination,
      plan
    )
    const report = await job.run()
    expect(report.outputSampleRate).toStrictEqual(48000)
    expect(
      Math.abs((report.outputDurationSeconds ?? 0) - SECONDS)
    ).toBeLessThan(0.05)

    job.close()
    plan.close()
    destination.close()
    source.close()
  })

  it('downmixes stereo to mono', async () => {
    const source = Media.openMemorySource(
      makeWav({ seconds: SECONDS, channels: 2 })
    )
    const destination = await Media.openFileDestination({
      uri: outputPath('convert-mono.wav'),
      overwrite: 'replace-atomically',
    })
    const plan = await Media.resolve(source, destination, {
      audio: {
        mode: 'encode',
        codec: 'pcm-s16',
        container: 'wav',
        channels: { policy: 'require', channelCount: 1 },
      },
    })
    expect(plan.conversions.map((c) => c.processor)).toContain('channel-map')

    const job = await Media.createTranscodeJobFromPlan(
      source,
      destination,
      plan
    )
    const report = await job.run()
    expect(report.outputChannelCount).toStrictEqual(1)

    job.close()
    plan.close()
    destination.close()
    source.close()
  })

  it('converts between every PCM sample format', async () => {
    const formats: AudioCodecId[] = [
      'pcm-u8',
      'pcm-s16',
      'pcm-s24',
      'pcm-s32',
      'pcm-f32',
    ]
    for (const codec of formats) {
      const source = Media.openMemorySource(makeWav({ seconds: 0.5 }))
      const name = `convert-${codec}.wav`
      const report = await transcode(source, name, codec, 'wav')
      expect(report.didCommitOutput).toBe(true)

      const readback = await Media.openFileSource({ uri: outputPath(name) })
      const asset = await Media.probe(readback)
      expect(asset.audioStreams[0]!.codec).toStrictEqual(codec)
      asset.close()
      readback.close()
      source.close()
    }
  })
})

describe('packet remux across containers', () => {
  const cases: Array<{ from: string; codec: AudioCodecId; to: ContainerId }> = [
    { from: 'm4a', codec: 'aac', to: 'adts' },
    { from: 'm4a', codec: 'aac', to: 'mp4' },
    { from: 'm4a', codec: 'aac', to: 'matroska' },
    { from: 'caf', codec: 'alac', to: 'm4a' },
    { from: 'flac', codec: 'flac', to: 'matroska' },
  ]

  for (const testCase of cases) {
    it(`copies ${testCase.codec} from ${testCase.from} into ${testCase.to}`, async () => {
      const seed = Media.openMemorySource(makeWav({ seconds: SECONDS }))
      const sourceName = `remux-src-${testCase.codec}.${EXTENSION[testCase.from]}`
      await transcode(
        seed,
        sourceName,
        testCase.codec,
        testCase.from as ContainerId
      )
      seed.close()

      const source = await Media.openFileSource({ uri: outputPath(sourceName) })
      const destination = await Media.openFileDestination({
        uri: outputPath(
          `remux-out-${testCase.codec}.${EXTENSION[testCase.to]}`
        ),
        overwrite: 'replace-atomically',
      })
      const plan = await Media.resolve(source, destination, {
        audio: { mode: 'copy', container: testCase.to },
      })
      expect(plan.path).toStrictEqual('packet-remux')
      expect(plan.outputCodec).toStrictEqual(testCase.codec)

      const job = await Media.createTranscodeJobFromPlan(
        source,
        destination,
        plan
      )
      const report = await job.run()
      expect(report.didCommitOutput).toBe(true)
      expect(report.outputByteSize).toBeGreaterThan(100)

      job.close()
      plan.close()
      destination.close()
      source.close()
    })
  }
})
