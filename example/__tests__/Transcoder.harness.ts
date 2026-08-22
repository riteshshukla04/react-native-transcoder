import { describe, it, expect } from 'react-native-harness'
import { Media, parseTranscoderError } from 'react-native-transcoder'
import type { MediaDestination, MediaSource } from 'react-native-transcoder'
import { outputPath } from '../src/lib/audio'

describe('MediaFactory lifecycle', () => {
  it('exposes a version without touching the engine', () => {
    expect(typeof Media.version).toBe('string')
    expect(Media.version.length).toBeGreaterThan(0)
  })

  it('resolves a writable scratch directory', () => {
    expect(Media.scratchDirectory.startsWith('/')).toBe(true)
  })

  it('prewarms exactly once, even when called concurrently', async () => {
    await Promise.all([Media.prewarm(), Media.prewarm(), Media.prewarm()])
    const capabilities = await Media.getCapabilities()
    expect(capabilities.engineVersion).toStrictEqual(Media.version)
  })
})

describe('capabilities snapshot', () => {
  it('crosses JSI once as plain data', async () => {
    const capabilities = await Media.getCapabilities()
    expect(capabilities.profile).toStrictEqual('mobile-core')
    expect(Array.isArray(capabilities.audioDecoders)).toBe(true)
    expect(Array.isArray(capabilities.audioEncoders)).toBe(true)
    expect(Array.isArray(capabilities.containers)).toBe(true)
    expect(capabilities.outcomes).toContain('balanced')
    expect(capabilities.supportsVideo).toBe(false)
    expect(capabilities.maxConcurrentJobs).toBeGreaterThan(0)
  })

  it('reports that no energy ranking is validated yet', async () => {
    const capabilities = await Media.getCapabilities()
    expect(capabilities.energyPolicyAvailable).toBe(false)
  })
})

describe('memory sources', () => {
  let source: MediaSource | undefined

  it('copies the buffer into native-owned memory', () => {
    const buffer = new ArrayBuffer(2048)
    source = Media.openMemorySource(buffer)
    expect(source.kind).toStrictEqual('memory')
    expect(source.isSeekable).toBe(true)
    expect(source.byteLength).toStrictEqual(2048)
  })

  it('rejects a buffer above the input ceiling', () => {
    expect(() =>
      Media.openMemorySource(new ArrayBuffer(65 * 1024 * 1024))
    ).toThrow()
  })

  it('closes idempotently', () => {
    source?.close()
    source?.close()
  })
})

describe('file sources', () => {
  it('fails with a typed error for a missing file', async () => {
    let caught: unknown
    try {
      await Media.openFileSource({ uri: outputPath('does-not-exist.wav') })
    } catch (error) {
      caught = error
    }
    const details = parseTranscoderError(caught)
    expect(details?.code).toStrictEqual('source-read-failed')
    expect(details?.stage).toStrictEqual('open')
  })

  it('rejects a directory as a source', async () => {
    let caught: unknown
    try {
      await Media.openFileSource({ uri: Media.scratchDirectory })
    } catch (error) {
      caught = error
    }
    expect(parseTranscoderError(caught)?.code).toStrictEqual('invalid-request')
  })
})

describe('file destinations', () => {
  let destination: MediaDestination | undefined

  it('opens an atomic destination', async () => {
    destination = await Media.openFileDestination({
      uri: outputPath('harness-output.m4a'),
      overwrite: 'replace-atomically',
    })
    expect(destination.kind).toStrictEqual('file')
    expect(destination.atomicity).toStrictEqual('replace-atomically')
    expect(destination.uri?.endsWith('harness-output.m4a')).toBe(true)
  })

  it('closes without publishing an output file', async () => {
    destination?.close()
    destination?.close()
    // The destination never committed, so re-opening with fail-if-exists works.
    const reopened = await Media.openFileDestination({
      uri: outputPath('harness-output.m4a'),
    })
    reopened.close()
  })

  it('refuses a not-guaranteed atomicity contract it cannot honour', async () => {
    let caught: unknown
    try {
      await Media.openFileDestination({
        uri: outputPath('harness-unsafe.m4a'),
        overwrite: 'not-guaranteed',
      })
    } catch (error) {
      caught = error
    }
    expect(parseTranscoderError(caught)?.code).toStrictEqual(
      'unsupported-combination'
    )
  })
})

describe('media pipeline entry points', () => {
  it('fails probing with a typed error when the bytes are not media', async () => {
    const source = Media.openMemorySource(new ArrayBuffer(64))
    let caught: unknown
    try {
      await Media.probe(source)
    } catch (error) {
      caught = error
    }
    const details = parseTranscoderError(caught)
    expect(details?.code).toStrictEqual('probe-failed')
    expect(details?.stage).toStrictEqual('probe')
    expect(details?.retryable).toBe(false)
    source.close()
  })

  it('rejects a non-http URL source before any network work', async () => {
    let caught: unknown
    try {
      await Media.openUrlSource({ url: 'ftp://example.com/audio.flac' })
    } catch (error) {
      caught = error
    }
    expect(parseTranscoderError(caught)?.code).toStrictEqual('invalid-request')
  })
})
