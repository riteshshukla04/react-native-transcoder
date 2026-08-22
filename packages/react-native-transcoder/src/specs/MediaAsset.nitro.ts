import type { HybridObject } from 'react-native-nitro-modules'
import type {
  AudioStreamDescriptor,
  ChapterDescriptor,
  ContainerId,
} from './MediaTypes.nitro'

/** Immutable probe result. Metadata and artwork are read lazily. */
export interface MediaAsset extends HybridObject<{
  ios: 'c++'
  android: 'c++'
}> {
  readonly durationSeconds?: number
  /** Engine container id, absent when the container has no stable engine identifier. */
  readonly container?: ContainerId
  /** Always present, for diagnostics and unrecognised containers. */
  readonly containerFormatName: string
  readonly byteSize?: number
  readonly audioStreams: AudioStreamDescriptor[]
  /** `true` when the container carries video the audio-only engine will not touch. */
  readonly hasVideo: boolean
  readonly chapters: ChapterDescriptor[]
  readonly artworkCount: number

  /** Normalised common tags plus namespaced raw tags that survived probing. */
  getMetadata(): Record<string, string>
  /** Writes artwork `index` to `path`. Large bytes never cross JSI eagerly. */
  saveArtworkToFile(index: number, path: string): Promise<string>
  close(): void
}
