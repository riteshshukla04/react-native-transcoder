import { NitroModules } from 'react-native-nitro-modules'
import type { MediaFactory } from './specs/MediaFactory.nitro'

/**
 * The single autolinked root of the engine.
 *
 * Creating it is trivial — no workers, no codec registration, no platform codec
 * enumeration. The engine initializes once on the first async call, or on
 * {@linkcode MediaFactory.prewarm}.
 */
export const Media =
  NitroModules.createHybridObject<MediaFactory>('MediaFactory')
