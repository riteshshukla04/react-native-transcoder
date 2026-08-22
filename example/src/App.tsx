import React, { useCallback, useEffect, useMemo, useRef, useState } from 'react'
import {
  Keyboard,
  SafeAreaView,
  ScrollView,
  StatusBar,
  StyleSheet,
  Text,
  TextInput,
  View,
} from 'react-native'
import { Media, parseTranscoderError } from 'react-native-transcoder'
import type {
  AudioCodecId,
  ContainerId,
  MediaCapabilitiesSnapshot,
  MediaSource,
  TranscodeProgress,
} from 'react-native-transcoder'

import { Button } from './components/Button'
import { Card } from './components/Card'
import { ChipGroup } from './components/ChipGroup'
import { ProgressBar } from './components/ProgressBar'
import { Row } from './components/Row'
import {
  MP3_BASE64,
  OGG_OPUS_BASE64,
  OGG_VORBIS_BASE64,
  WEBM_OPUS_BASE64,
} from './fixtures/media'
import { base64ToArrayBuffer, makeWav, outputPath } from './lib/audio'
import { formatBitRate, formatBytes, formatSeconds } from './lib/format'
import { theme } from './theme'

type SourceKind =
  'tone' | 'mp3' | 'ogg-vorbis' | 'ogg-opus' | 'webm-opus' | 'file'

const SOURCE_KINDS: readonly SourceKind[] = [
  'tone',
  'mp3',
  'ogg-vorbis',
  'ogg-opus',
  'webm-opus',
  'file',
]

const FIXTURES: Partial<Record<SourceKind, string>> = {
  'mp3': MP3_BASE64,
  'ogg-vorbis': OGG_VORBIS_BASE64,
  'ogg-opus': OGG_OPUS_BASE64,
  'webm-opus': WEBM_OPUS_BASE64,
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

const SAMPLE_RATES = ['preserve', '22050', '44100', '48000'] as const
const CHANNELS = ['preserve', '1', '2'] as const

interface AssetSummary {
  container: string
  codec: string
  sampleRate: number
  channels: number
  duration?: number
  bitRate?: number
  metadataKeys: number
  streams: number
}

interface ReportSummary {
  uri: string
  bytes: number
  duration?: number
  elapsed: number
  speed: number
  samples: number
  warnings: string[]
}

export default function App() {
  const [capabilities, setCapabilities] = useState<MediaCapabilitiesSnapshot>()
  const [error, setError] = useState<string>()
  const [busy, setBusy] = useState(false)

  const [sourceKind, setSourceKind] = useState<SourceKind>('tone')
  const [filePath, setFilePath] = useState('')
  const [asset, setAsset] = useState<AssetSummary>()

  const [container, setContainer] = useState<ContainerId>('m4a')
  const [codec, setCodec] = useState<AudioCodecId>('aac')
  const [sampleRate, setSampleRate] =
    useState<(typeof SAMPLE_RATES)[number]>('preserve')
  const [channels, setChannels] =
    useState<(typeof CHANNELS)[number]>('preserve')
  const [bitRateKbps, setBitRateKbps] = useState('128')

  const [progress, setProgress] = useState<TranscodeProgress>()
  const [report, setReport] = useState<ReportSummary>()
  const [matrix, setMatrix] = useState<{ pass: string[]; fail: string[] }>()
  const cancelRef = useRef<(() => void) | undefined>(undefined)

  useEffect(() => {
    Media.getCapabilities()
      .then(setCapabilities)
      .catch((e) => setError(String(e)))
  }, [])

  const containers = useMemo(
    () => (capabilities?.containers ?? []).map((entry) => entry.container),
    [capabilities]
  )
  const codecsForContainer = useMemo(
    () =>
      capabilities?.containers.find((entry) => entry.container === container)
        ?.codecs ?? [],
    [capabilities, container]
  )

  useEffect(() => {
    if (codecsForContainer.length > 0 && !codecsForContainer.includes(codec)) {
      setCodec(codecsForContainer[0]!)
    }
  }, [codecsForContainer, codec])

  const openSource = useCallback(async (): Promise<MediaSource> => {
    if (sourceKind === 'file') {
      if (filePath.trim().length === 0)
        throw new Error('Enter a file path first.')
      return Media.openFileSource({ uri: filePath.trim() })
    }
    if (sourceKind === 'tone') {
      return Media.openMemorySource(makeWav({ seconds: 5 }))
    }
    return Media.openMemorySource(base64ToArrayBuffer(FIXTURES[sourceKind]!))
  }, [sourceKind, filePath])

  const runWithSource = useCallback(
    async (work: (source: MediaSource) => Promise<void>) => {
      setError(undefined)
      setBusy(true)
      let source: MediaSource | undefined
      try {
        source = await openSource()
        await work(source)
      } catch (caught) {
        const details = parseTranscoderError(caught)
        setError(
          details != null
            ? `${details.code} @ ${details.stage}: ${details.message}`
            : String(caught)
        )
      } finally {
        source?.close()
        setBusy(false)
      }
    },
    [openSource]
  )

  const onProbe = useCallback(
    () =>
      runWithSource(async (source) => {
        const probed = await Media.probe(source)
        const stream = probed.audioStreams[0]
        setAsset({
          container: probed.container ?? probed.containerFormatName,
          codec: stream?.codec ?? stream?.codecName ?? 'unknown',
          sampleRate: stream?.sampleRate ?? 0,
          channels: stream?.channelCount ?? 0,
          duration: probed.durationSeconds,
          bitRate: stream?.bitsPerSecond,
          metadataKeys: Object.keys(probed.getMetadata()).length,
          streams: probed.audioStreams.length,
        })
        probed.close()
      }),
    [runWithSource]
  )

  const onTranscode = useCallback(
    () =>
      runWithSource(async (source) => {
        setReport(undefined)
        setProgress(undefined)

        const name = `output-${codec}.${EXTENSION[container] ?? 'bin'}`
        const destination = await Media.openFileDestination({
          uri: outputPath(name),
          overwrite: 'replace-atomically',
        })
        const parsedBitRate = Number(bitRateKbps)
        const job = await Media.createTranscodeJob(source, destination, {
          audio: {
            mode: 'encode',
            codec,
            container,
            quality:
              Number.isFinite(parsedBitRate) && parsedBitRate > 0
                ? {
                    mode: 'bitrate',
                    bitsPerSecond: parsedBitRate * 1000,
                    bitRateMode: 'vbr',
                  }
                : undefined,
            sampleRate:
              sampleRate === 'preserve'
                ? { policy: 'preserve' }
                : { policy: 'prefer', hertz: Number(sampleRate) },
            channels:
              channels === 'preserve'
                ? { policy: 'preserve' }
                : { policy: 'prefer', channelCount: Number(channels) },
          },
          metadata: 'copy',
          outcome: 'balanced',
        })
        const subscription = job.addOnProgressListener(setProgress)
        cancelRef.current = () => job.cancel()

        try {
          const result = await job.run()
          setReport({
            uri: result.outputUri ?? name,
            bytes: result.outputByteSize,
            duration: result.outputDurationSeconds,
            elapsed: result.elapsedSeconds,
            speed: result.speedRatio,
            samples: result.samplesProcessed,
            warnings: result.warnings.map(
              (warning) => `${warning.code}: ${warning.message}`
            ),
          })
        } finally {
          cancelRef.current = undefined
          subscription.remove()
          job.close()
          destination.close()
        }
      }),
    [runWithSource, codec, container, sampleRate, channels, bitRateKbps]
  )

  const onReprobeOutput = useCallback(async () => {
    if (report == null) return
    setError(undefined)
    setBusy(true)
    try {
      const source = await Media.openFileSource({ uri: report.uri })
      const probed = await Media.probe(source)
      const stream = probed.audioStreams[0]
      setAsset({
        container: probed.container ?? probed.containerFormatName,
        codec: stream?.codec ?? stream?.codecName ?? 'unknown',
        sampleRate: stream?.sampleRate ?? 0,
        channels: stream?.channelCount ?? 0,
        duration: probed.durationSeconds,
        bitRate: stream?.bitsPerSecond,
        metadataKeys: Object.keys(probed.getMetadata()).length,
        streams: probed.audioStreams.length,
      })
      probed.close()
      source.close()
    } catch (caught) {
      setError(String(caught))
    } finally {
      setBusy(false)
    }
  }, [report])

  const onRunMatrix = useCallback(async () => {
    if (capabilities == null) return
    setError(undefined)
    setBusy(true)
    setMatrix(undefined)
    const pass: string[] = []
    const fail: string[] = []

    try {
      for (const entry of capabilities.containers) {
        for (const targetCodec of entry.codecs) {
          const label = `${targetCodec}→${entry.container}`
          const source = Media.openMemorySource(makeWav({ seconds: 1 }))
          try {
            const name = `matrix-${targetCodec}-${entry.container}.${EXTENSION[entry.container] ?? 'bin'}`
            const destination = await Media.openFileDestination({
              uri: outputPath(name),
              overwrite: 'replace-atomically',
            })
            const job = await Media.createTranscodeJob(source, destination, {
              audio: {
                mode: 'encode',
                codec: targetCodec,
                container: entry.container,
              },
            })
            const result = await job.run()
            job.close()
            destination.close()

            const readback = await Media.openFileSource({
              uri: outputPath(name),
            })
            const probed = await Media.probe(readback)
            const ok =
              result.didCommitOutput &&
              probed.audioStreams[0]?.codec === targetCodec
            probed.close()
            readback.close()
            ;(ok ? pass : fail).push(label)
          } catch (caught) {
            fail.push(
              `${label} (${parseTranscoderError(caught)?.code ?? 'error'})`
            )
          } finally {
            source.close()
          }
          setMatrix({ pass: [...pass], fail: [...fail] })
        }
      }
    } finally {
      setBusy(false)
    }
  }, [capabilities])

  return (
    <SafeAreaView style={styles.screen}>
      <StatusBar barStyle="light-content" backgroundColor={theme.background} />
      <ScrollView contentContainerStyle={styles.content}>
        <Text style={styles.heading}>react-native-transcoder</Text>
        <Text style={styles.subheading}>Fully on-device audio transcoding</Text>

        <Card title="Engine" subtitle={Media.scratchDirectory}>
          <Row label="version" value={Media.version} />
          <Row label="profile" value={capabilities?.profile ?? '…'} />
          <Row
            label="codecs"
            value={
              capabilities != null
                ? `${capabilities.audioDecoders.length} decode · ${capabilities.audioEncoders.length} encode`
                : '…'
            }
          />
          <Row
            label="containers"
            value={String(capabilities?.containers.length ?? '…')}
          />
          <Row
            label="energy policy"
            value={
              capabilities?.energyPolicyAvailable ? 'measured' : 'unavailable'
            }
            tone={capabilities?.energyPolicyAvailable ? 'success' : 'warning'}
          />
        </Card>

        <Card title="Source" subtitle="Pick what to feed the engine">
          <ChipGroup
            label="source"
            options={SOURCE_KINDS}
            value={sourceKind}
            onChange={setSourceKind}
          />
          {sourceKind === 'file' && (
            <TextInput
              testID="file-path-input"
              style={styles.input}
              value={filePath}
              onChangeText={setFilePath}
              placeholder={`${Media.scratchDirectory}/song.mp3`}
              placeholderTextColor={theme.textMuted}
              autoCapitalize="none"
              autoCorrect={false}
              spellCheck={false}
              returnKeyType="done"
              clearButtonMode="while-editing"
              onSubmitEditing={Keyboard.dismiss}
            />
          )}
          <Button
            testID="probe-button"
            title="Probe source"
            onPress={onProbe}
            busy={busy}
          />
          {asset != null && (
            <View style={styles.assetBlock}>
              <Row label="container" value={asset.container} />
              <Row label="codec" value={asset.codec} />
              <Row label="sample rate" value={`${asset.sampleRate} Hz`} />
              <Row label="channels" value={String(asset.channels)} />
              <Row label="duration" value={formatSeconds(asset.duration)} />
              <Row label="bit rate" value={formatBitRate(asset.bitRate)} />
              <Row label="audio streams" value={String(asset.streams)} />
              <Row label="metadata keys" value={String(asset.metadataKeys)} />
            </View>
          )}
        </Card>

        <Card
          title="Output"
          subtitle="Only combinations this build can actually write"
        >
          <ChipGroup
            label="container"
            options={containers}
            value={container}
            onChange={setContainer}
          />
          <ChipGroup
            label="codec"
            options={codecsForContainer}
            value={codec}
            onChange={setCodec}
          />
          <ChipGroup
            label="sample rate"
            options={SAMPLE_RATES}
            value={sampleRate}
            onChange={setSampleRate}
          />
          <ChipGroup
            label="channels"
            options={CHANNELS}
            value={channels}
            onChange={setChannels}
          />
          <View style={styles.inlineField}>
            <Text style={styles.inlineLabel}>bit rate (kbps)</Text>
            <TextInput
              testID="bitrate-input"
              style={[styles.input, styles.inputSmall]}
              value={bitRateKbps}
              onChangeText={setBitRateKbps}
              keyboardType="number-pad"
              placeholderTextColor={theme.textMuted}
            />
          </View>
        </Card>

        <Card
          title="Transcode"
          subtitle="Runs on an engine worker, never on the JS thread"
        >
          <Button
            testID="transcode-button"
            title="Transcode"
            onPress={onTranscode}
            busy={busy}
          />
          <Button
            testID="cancel-button"
            title="Cancel"
            variant="danger"
            onPress={() => cancelRef.current?.()}
            disabled={cancelRef.current == null && !busy}
          />
          <ProgressBar progress={progress} />
          {report != null && (
            <View style={styles.assetBlock}>
              <Row
                label="output"
                value={report.uri.split('/').pop() ?? report.uri}
              />
              <Row label="size" value={formatBytes(report.bytes)} />
              <Row label="duration" value={formatSeconds(report.duration)} />
              <Row label="elapsed" value={formatSeconds(report.elapsed)} />
              <Row
                label="speed"
                value={`${report.speed.toFixed(1)}× realtime`}
                tone="success"
              />
              <Row label="samples" value={report.samples.toLocaleString()} />
              {report.warnings.map((warning) => (
                <Row
                  key={warning}
                  label="warning"
                  value={warning}
                  tone="warning"
                />
              ))}
              <Button
                testID="reprobe-button"
                title="Probe the output"
                variant="secondary"
                onPress={onReprobeOutput}
              />
            </View>
          )}
        </Card>

        <Card
          title="Format matrix"
          subtitle="Transcode every advertised codec/container pair"
        >
          <Button
            testID="matrix-button"
            title="Run the full matrix"
            variant="secondary"
            onPress={onRunMatrix}
            busy={busy}
          />
          {matrix != null && (
            <View style={styles.assetBlock}>
              <Row
                label="passed"
                value={String(matrix.pass.length)}
                tone={matrix.fail.length === 0 ? 'success' : 'default'}
              />
              <Row
                label="failed"
                value={String(matrix.fail.length)}
                tone={matrix.fail.length === 0 ? 'success' : 'danger'}
              />
              <Text testID="matrix-detail" style={styles.matrixDetail}>
                {matrix.pass.join('  ·  ')}
              </Text>
              {matrix.fail.length > 0 && (
                <Text style={[styles.matrixDetail, { color: theme.danger }]}>
                  {matrix.fail.join('  ·  ')}
                </Text>
              )}
            </View>
          )}
        </Card>

        {error != null && (
          <Card title="Error">
            <Text testID="error-text" style={styles.errorText}>
              {error}
            </Text>
          </Card>
        )}
      </ScrollView>
    </SafeAreaView>
  )
}

const styles = StyleSheet.create({
  screen: { flex: 1, backgroundColor: theme.background },
  content: { padding: 16, paddingBottom: 48 },
  heading: { color: theme.text, fontSize: 24, fontWeight: '800' },
  subheading: { color: theme.textMuted, fontSize: 13, marginBottom: 18 },
  input: {
    backgroundColor: theme.surfaceRaised,
    borderRadius: 10,
    borderWidth: 1,
    borderColor: theme.border,
    color: theme.text,
    paddingHorizontal: 12,
    paddingVertical: 10,
    fontSize: 13,
  },
  inputSmall: { width: 100, textAlign: 'right' },
  inlineField: {
    flexDirection: 'row',
    alignItems: 'center',
    justifyContent: 'space-between',
  },
  inlineLabel: {
    color: theme.textMuted,
    fontSize: 12,
    textTransform: 'uppercase',
    letterSpacing: 0.6,
  },
  assetBlock: {
    gap: 8,
    marginTop: 4,
    paddingTop: 12,
    borderTopWidth: 1,
    borderTopColor: theme.border,
  },
  matrixDetail: { color: theme.textMuted, fontSize: 11, lineHeight: 16 },
  errorText: { color: theme.danger, fontSize: 13 },
})
