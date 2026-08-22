import React, { useCallback, useState } from 'react'
import {
  SafeAreaView,
  ScrollView,
  StyleSheet,
  Text,
  TouchableOpacity,
  View,
} from 'react-native'
import { Media, parseTranscoderError } from 'react-native-transcoder'

type Row = { label: string; value: string }

export default function App() {
  const [rows, setRows] = useState<Row[]>([])

  const push = useCallback((label: string, value: string) => {
    setRows((previous) => [...previous, { label, value }])
  }, [])

  const runChecks = useCallback(async () => {
    setRows([])
    try {
      push('version', Media.version)
      push('scratchDirectory', Media.scratchDirectory)

      await Media.prewarm()
      const capabilities = await Media.getCapabilities()
      push('profile', capabilities.profile)
      push('decoders', String(capabilities.audioDecoders.length))
      push('encoders', String(capabilities.audioEncoders.length))
      push('energy policy', String(capabilities.energyPolicyAvailable))

      const outputUri = `${Media.scratchDirectory}/demo-output.m4a`
      const destination = await Media.openFileDestination({
        uri: outputUri,
        overwrite: 'replace-atomically',
      })
      push('destination', destination.atomicity)
      destination.close()

      const source = Media.openMemorySource(new ArrayBuffer(1024))
      push('memory source', `${source.kind} / ${source.byteLength ?? 0} bytes`)
      try {
        await Media.probe(source)
      } catch (error) {
        const details = parseTranscoderError(error)
        push(
          'probe',
          `${details?.code ?? 'unknown'} @ ${details?.stage ?? '?'}`
        )
      }
      source.close()
    } catch (error) {
      push('error', String(error))
    }
  }, [push])

  return (
    <SafeAreaView style={styles.container}>
      <ScrollView contentContainerStyle={styles.content}>
        <Text style={styles.title}>react-native-transcoder</Text>
        <TouchableOpacity style={styles.button} onPress={runChecks}>
          <Text style={styles.buttonText}>Run engine checks</Text>
        </TouchableOpacity>
        {rows.map((row, index) => (
          <View style={styles.row} key={`${row.label}-${index}`}>
            <Text style={styles.rowLabel}>{row.label}</Text>
            <Text style={styles.rowValue}>{row.value}</Text>
          </View>
        ))}
      </ScrollView>
    </SafeAreaView>
  )
}

const styles = StyleSheet.create({
  container: { flex: 1, backgroundColor: '#0b0b0f' },
  content: { padding: 20, gap: 12 },
  title: { color: 'white', fontSize: 22, fontWeight: '700' },
  button: {
    backgroundColor: '#4c6ef5',
    paddingVertical: 12,
    borderRadius: 10,
    alignItems: 'center',
  },
  buttonText: { color: 'white', fontWeight: '600' },
  row: {
    flexDirection: 'row',
    justifyContent: 'space-between',
    borderBottomColor: '#23232b',
    borderBottomWidth: 1,
    paddingVertical: 8,
  },
  rowLabel: { color: '#9aa0aa' },
  rowValue: { color: 'white', flexShrink: 1, textAlign: 'right' },
})
