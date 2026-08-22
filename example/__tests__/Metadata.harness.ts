import { describe, it, expect } from 'react-native-harness'
import { Media, parseTranscoderError } from 'react-native-transcoder'
import type { MediaSource, TranscodeRequest } from 'react-native-transcoder'
import { makeWav, outputPath } from '../src/lib/audio'

const TAGS = {
  title: 'Chord Test',
  artist: 'react-native-transcoder',
  album: 'Metadata QA',
}

async function transcode(
  source: MediaSource,
  name: string,
  request: TranscodeRequest
) {
  const destination = await Media.openFileDestination({
    uri: outputPath(name),
    overwrite: 'replace-atomically',
  })
  const job = await Media.createTranscodeJob(source, destination, request)
  try {
    return await job.run()
  } finally {
    job.close()
    destination.close()
  }
}

async function readMetadata(name: string) {
  const source = await Media.openFileSource({ uri: outputPath(name) })
  const asset = await Media.probe(source)
  const metadata = asset.getMetadata()
  asset.close()
  source.close()
  return metadata
}

describe('metadata policies', () => {
  it('writes the tags a replace policy asks for', async () => {
    const source = Media.openMemorySource(makeWav({ seconds: 1 }))
    const report = await transcode(source, 'meta-replace.m4a', {
      audio: { mode: 'encode', codec: 'aac', container: 'm4a' },
      metadata: 'replace',
      metadataValues: TAGS,
    })
    expect(report.didCommitOutput).toBe(true)
    source.close()

    const metadata = await readMetadata('meta-replace.m4a')
    expect(metadata.title).toStrictEqual(TAGS.title)
    expect(metadata.artist).toStrictEqual(TAGS.artist)
    expect(metadata.album).toStrictEqual(TAGS.album)
  })

  it('carries tags through a full transcode with the default copy policy', async () => {
    const tagged = await Media.openFileSource({
      uri: outputPath('meta-replace.m4a'),
    })
    const report = await transcode(tagged, 'meta-copied.m4a', {
      audio: { mode: 'encode', codec: 'aac', container: 'm4a' },
    })
    expect(report.didCommitOutput).toBe(true)
    tagged.close()

    const metadata = await readMetadata('meta-copied.m4a')
    expect(metadata.title).toStrictEqual(TAGS.title)
    expect(metadata.artist).toStrictEqual(TAGS.artist)
  })

  it('carries tags through a packet remux', async () => {
    const tagged = await Media.openFileSource({
      uri: outputPath('meta-replace.m4a'),
    })
    const report = await transcode(tagged, 'meta-remuxed.mp4', {
      audio: { mode: 'copy', container: 'mp4' },
    })
    expect(report.didCommitOutput).toBe(true)
    tagged.close()

    const metadata = await readMetadata('meta-remuxed.mp4')
    expect(metadata.title).toStrictEqual(TAGS.title)
  })

  it('drops every tag when asked to', async () => {
    const tagged = await Media.openFileSource({
      uri: outputPath('meta-replace.m4a'),
    })
    await transcode(tagged, 'meta-dropped.m4a', {
      audio: { mode: 'encode', codec: 'aac', container: 'm4a' },
      metadata: 'drop',
    })
    tagged.close()

    const metadata = await readMetadata('meta-dropped.m4a')
    expect(metadata.title).toBeUndefined()
    expect(metadata.artist).toBeUndefined()
  })

  it('merges new tags over the ones the source carried', async () => {
    const tagged = await Media.openFileSource({
      uri: outputPath('meta-replace.m4a'),
    })
    await transcode(tagged, 'meta-merged.m4a', {
      audio: { mode: 'encode', codec: 'aac', container: 'm4a' },
      metadata: 'merge',
      metadataValues: { artist: 'Someone Else', comment: 'added by merge' },
    })
    tagged.close()

    const metadata = await readMetadata('meta-merged.m4a')
    expect(metadata.title).toStrictEqual(TAGS.title)
    expect(metadata.artist).toStrictEqual('Someone Else')
    expect(metadata.comment).toStrictEqual('added by merge')
  })

  it('refuses replace and merge without values', async () => {
    const source = Media.openMemorySource(makeWav({ seconds: 0.5 }))
    const destination = await Media.openFileDestination({
      uri: outputPath('meta-invalid.m4a'),
      overwrite: 'replace-atomically',
    })
    let caught: unknown
    try {
      await Media.resolve(source, destination, {
        audio: { mode: 'encode', codec: 'aac', container: 'm4a' },
        metadata: 'replace',
      })
    } catch (error) {
      caught = error
    }
    expect(parseTranscoderError(caught)?.code).toStrictEqual('invalid-request')
    destination.close()
    source.close()
  })
})
