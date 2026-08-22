import { Media } from 'react-native-transcoder'

export const outputPath = (name: string) => `${Media.scratchDirectory}/${name}`

function writeAscii(view: DataView, offset: number, text: string) {
  for (let i = 0; i < text.length; i++)
    view.setUint8(offset + i, text.charCodeAt(i))
}

export interface WavOptions {
  seconds?: number
  sampleRate?: number
  channels?: number
  frequency?: number
}

/** A deterministic 16-bit PCM sine, so decoded output can be checked numerically. */
export function makeWav({
  seconds = 2,
  sampleRate = 44100,
  channels = 1,
  frequency = 440,
}: WavOptions = {}) {
  const frames = Math.round(seconds * sampleRate)
  const dataSize = frames * channels * 2
  const buffer = new ArrayBuffer(44 + dataSize)
  const view = new DataView(buffer)

  writeAscii(view, 0, 'RIFF')
  view.setUint32(4, 36 + dataSize, true)
  writeAscii(view, 8, 'WAVE')
  writeAscii(view, 12, 'fmt ')
  view.setUint32(16, 16, true)
  view.setUint16(20, 1, true)
  view.setUint16(22, channels, true)
  view.setUint32(24, sampleRate, true)
  view.setUint32(28, sampleRate * channels * 2, true)
  view.setUint16(32, channels * 2, true)
  view.setUint16(34, 16, true)
  writeAscii(view, 36, 'data')
  view.setUint32(40, dataSize, true)

  for (let i = 0; i < frames; i++) {
    const sample = Math.round(
      Math.sin((2 * Math.PI * frequency * i) / sampleRate) * 0.8 * 32767
    )
    for (let c = 0; c < channels; c++) {
      view.setInt16(44 + (i * channels + c) * 2, sample, true)
    }
  }
  return buffer
}

const BASE64_ALPHABET =
  'ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/'

/** Hermes has no Buffer, and the fixtures are small enough for a plain decoder. */
/* eslint-disable no-bitwise */
export function base64ToArrayBuffer(input: string) {
  const clean = input.replace(/[^A-Za-z0-9+/]/g, '')
  const bytes = new Uint8Array(Math.floor((clean.length * 3) / 4))
  let byteIndex = 0
  let accumulator = 0
  let bits = 0

  for (let i = 0; i < clean.length; i++) {
    accumulator = (accumulator << 6) | BASE64_ALPHABET.indexOf(clean[i]!)
    bits += 6
    if (bits >= 8) {
      bits -= 8
      bytes[byteIndex++] = (accumulator >> bits) & 0xff
    }
  }
  return bytes.buffer.slice(0, byteIndex)
}
/* eslint-enable no-bitwise */
