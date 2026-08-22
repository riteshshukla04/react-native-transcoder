import { describe, it, expect } from 'react-native-harness'
import { Media, parseTranscoderError } from 'react-native-transcoder'
import type { TranscodeProgress } from 'react-native-transcoder'

import { makeWav as makeWavBuffer, outputPath } from '../src/lib/audio'

const SAMPLE_RATE = 44100
const SECONDS = 2

const makeWav = (seconds = SECONDS, sampleRate = SAMPLE_RATE, channels = 1) =>
  makeWavBuffer({ seconds, sampleRate, channels })

describe('capabilities with the FFmpeg backend linked', () => {
  it('reports real decoders, encoders and containers', async () => {
    const capabilities = await Media.getCapabilities()
    const decoderIds = capabilities.audioDecoders.map((entry) => entry.codec)
    const encoderIds = capabilities.audioEncoders.map((entry) => entry.codec)
    const containerIds = capabilities.containers.map((entry) => entry.container)

    expect(decoderIds).toContain('pcm-s16')
    expect(decoderIds).toContain('aac')
    expect(decoderIds).toContain('flac')
    expect(encoderIds).toContain('aac')
    expect(encoderIds).toContain('flac')
    expect(containerIds).toContain('m4a')
    expect(containerIds).toContain('wav')
  })
})

describe('probing', () => {
  it('reads stream facts out of a generated WAV', async () => {
    const source = Media.openMemorySource(makeWav())
    const asset = await Media.probe(source)

    expect(asset.containerFormatName).toStrictEqual('wav')
    expect(asset.container).toStrictEqual('wav')
    expect(asset.hasVideo).toBe(false)
    expect(asset.audioStreams.length).toStrictEqual(1)

    const stream = asset.audioStreams[0]!
    expect(stream.codec).toStrictEqual('pcm-s16')
    expect(stream.sampleRate).toStrictEqual(SAMPLE_RATE)
    expect(stream.channelCount).toStrictEqual(1)
    expect(Math.abs((asset.durationSeconds ?? 0) - SECONDS)).toBeLessThan(0.05)

    expect(source.isProbed).toBe(true)
    asset.close()
    source.close()
  })

  it('reuses the cached probe record instead of reading the payload twice', async () => {
    const source = Media.openMemorySource(makeWav())
    const first = await Media.probe(source)
    const second = await Media.probe(source)
    expect(second.durationSeconds).toStrictEqual(first.durationSeconds)
    first.close()
    second.close()
    source.close()
  })
})

describe('planning', () => {
  it('resolves a full transcode with the conversions it will insert', async () => {
    const source = Media.openMemorySource(makeWav())
    const destination = await Media.openFileDestination({
      uri: outputPath('plan-only.m4a'),
      overwrite: 'replace-atomically',
    })

    const plan = await Media.resolve(source, destination, {
      audio: {
        mode: 'encode',
        codec: 'aac',
        container: 'm4a',
        quality: { mode: 'bitrate', bitsPerSecond: 128_000 },
      },
    })

    expect(plan.path).toStrictEqual('full-transcode')
    expect(plan.outputCodec).toStrictEqual('aac')
    expect(plan.outputContainer).toStrictEqual('m4a')
    expect(plan.outputSampleRate).toStrictEqual(SAMPLE_RATE)
    expect(plan.outputBitsPerSecond).toStrictEqual(128_000)
    expect(plan.requiresSourceStaging).toBe(false)
    expect(plan.isTwoPass).toBe(false)
    expect(JSON.parse(plan.toJson()).path).toStrictEqual('full-transcode')

    plan.close()
    destination.close()
    source.close()
  })

  it('rejects a lossless-only quality combination before any work starts', async () => {
    const source = Media.openMemorySource(makeWav())
    const destination = await Media.openFileDestination({
      uri: outputPath('plan-invalid.m4a'),
      overwrite: 'replace-atomically',
    })

    let caught: unknown
    try {
      await Media.resolve(source, destination, {
        audio: {
          mode: 'copy',
          container: 'm4a',
          quality: { mode: 'bitrate', bitsPerSecond: 128_000 },
        },
      })
    } catch (error) {
      caught = error
    }
    expect(parseTranscoderError(caught)?.code).toStrictEqual('invalid-request')

    destination.close()
    source.close()
  })
})

describe('transcoding', () => {
  it('encodes WAV to AAC in M4A and reports what it produced', async () => {
    const source = Media.openMemorySource(makeWav())
    const destination = await Media.openFileDestination({
      uri: outputPath('verify-output.m4a'),
      overwrite: 'replace-atomically',
    })

    const job = await Media.createTranscodeJob(source, destination, {
      audio: {
        mode: 'encode',
        codec: 'aac',
        container: 'm4a',
        quality: { mode: 'bitrate', bitsPerSecond: 128_000 },
        sampleRate: { policy: 'preserve' },
        channels: { policy: 'preserve' },
      },
      metadata: 'copy',
      outcome: 'balanced',
    })

    const phases: string[] = []
    const subscription = job.addOnProgressListener(
      (progress: TranscodeProgress) => {
        if (phases[phases.length - 1] !== progress.phase)
          phases.push(progress.phase)
      }
    )

    expect(job.state).toStrictEqual('ready')
    const report = await job.run()

    expect(job.state).toStrictEqual('completed')
    expect(report.didCommitOutput).toBe(true)
    expect(report.wasCancelled).toBe(false)
    expect(report.outputContainer).toStrictEqual('m4a')
    expect(report.outputCodec).toStrictEqual('aac')
    expect(report.outputSampleRate).toStrictEqual(SAMPLE_RATE)
    expect(report.outputChannelCount).toStrictEqual(1)
    expect(report.outputByteSize).toBeGreaterThan(1000)
    expect(
      Math.abs((report.outputDurationSeconds ?? 0) - SECONDS)
    ).toBeLessThan(0.1)
    expect(report.samplesProcessed).toBeGreaterThan(
      SAMPLE_RATE * SECONDS * 0.99
    )
    expect(report.speedRatio).toBeGreaterThan(0)
    expect(phases).toContain('done')

    subscription.remove()
    job.close()
    destination.close()
    source.close()
  })

  it('produces an output that probes back as real AAC', async () => {
    const readback = await Media.openFileSource({
      uri: outputPath('verify-output.m4a'),
    })
    const asset = await Media.probe(readback)

    expect(asset.container).toStrictEqual('m4a')
    expect(asset.audioStreams.length).toStrictEqual(1)
    const stream = asset.audioStreams[0]!
    expect(stream.codec).toStrictEqual('aac')
    expect(stream.sampleRate).toStrictEqual(SAMPLE_RATE)
    expect(stream.channelCount).toStrictEqual(1)
    expect(Math.abs((asset.durationSeconds ?? 0) - SECONDS)).toBeLessThan(0.15)

    asset.close()
    readback.close()
  })

  it('round-trips back to WAV so the decoded output can be compared', async () => {
    const source = await Media.openFileSource({
      uri: outputPath('verify-output.m4a'),
    })
    const destination = await Media.openFileDestination({
      uri: outputPath('verify-roundtrip.wav'),
      overwrite: 'replace-atomically',
    })

    const job = await Media.createTranscodeJob(source, destination, {
      audio: { mode: 'encode', codec: 'pcm-s16', container: 'wav' },
    })
    const report = await job.run()

    expect(report.didCommitOutput).toBe(true)
    expect(report.outputContainer).toStrictEqual('wav')
    expect(
      Math.abs((report.outputDurationSeconds ?? 0) - SECONDS)
    ).toBeLessThan(0.15)

    job.close()
    destination.close()
    source.close()
  })

  it('remuxes compressed packets without decoding', async () => {
    const source = await Media.openFileSource({
      uri: outputPath('verify-output.m4a'),
    })
    const destination = await Media.openFileDestination({
      uri: outputPath('verify-remux.adts'),
      overwrite: 'replace-atomically',
    })

    const plan = await Media.resolve(source, destination, {
      audio: { mode: 'copy', container: 'adts' },
    })
    expect(plan.path).toStrictEqual('packet-remux')

    const job = await Media.createTranscodeJobFromPlan(
      source,
      destination,
      plan
    )
    const report = await job.run()
    expect(report.didCommitOutput).toBe(true)
    expect(report.outputByteSize).toBeGreaterThan(1000)

    job.close()
    plan.close()
    destination.close()
    source.close()
  })

  it('cancels cooperatively and leaves no output behind', async () => {
    const source = Media.openMemorySource(makeWav(20))
    const destination = await Media.openFileDestination({
      uri: outputPath('verify-cancelled.m4a'),
      overwrite: 'replace-atomically',
    })

    const job = await Media.createTranscodeJob(source, destination, {
      audio: { mode: 'encode', codec: 'aac', container: 'm4a' },
    })

    const running = job.run()
    job.cancel()

    let caught: unknown
    try {
      await running
    } catch (error) {
      caught = error
    }
    expect(parseTranscoderError(caught)?.code).toStrictEqual('cancelled')
    expect(job.state).toStrictEqual('cancelled')

    job.close()
    destination.close()
    source.close()
  })

  it('refuses a second run on the same job', async () => {
    const source = Media.openMemorySource(makeWav(1))
    const destination = await Media.openFileDestination({
      uri: outputPath('verify-single-use.m4a'),
      overwrite: 'replace-atomically',
    })
    const job = await Media.createTranscodeJob(source, destination, {
      audio: { mode: 'encode', codec: 'aac', container: 'm4a' },
    })

    await job.run()
    expect(() => job.run()).toThrow()

    job.close()
    destination.close()
    source.close()
  })
})
