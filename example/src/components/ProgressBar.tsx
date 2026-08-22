import React from 'react'
import { StyleSheet, Text, View } from 'react-native'
import { theme } from '../theme'
import type { TranscodeProgress } from 'react-native-transcoder'

export function ProgressBar({
  progress,
}: {
  progress: TranscodeProgress | undefined
}) {
  if (progress == null) return null
  const total = progress.totalInputSeconds
  const fraction =
    total != null && total > 0
      ? Math.max(0, Math.min(1, progress.inputSecondsProcessed / total))
      : undefined

  return (
    <View style={styles.container}>
      <View style={styles.track}>
        <View
          style={[
            styles.fill,
            fraction != null
              ? { width: `${Math.round(fraction * 100)}%` }
              : styles.indeterminate,
          ]}
        />
      </View>
      <Text style={styles.caption} testID="progress-caption">
        {progress.phase}
        {fraction != null ? ` · ${Math.round(fraction * 100)}%` : ''}
        {progress.speedRatio != null
          ? ` · ${progress.speedRatio.toFixed(1)}×`
          : ''}
        {progress.estimatedSecondsRemaining != null
          ? ` · ${progress.estimatedSecondsRemaining.toFixed(1)}s left`
          : ''}
      </Text>
    </View>
  )
}

const styles = StyleSheet.create({
  container: { gap: 6 },
  track: {
    height: 8,
    borderRadius: 999,
    backgroundColor: theme.surfaceRaised,
    overflow: 'hidden',
  },
  fill: { height: 8, backgroundColor: theme.accent, borderRadius: 999 },
  indeterminate: { width: '35%' },
  caption: { color: theme.textMuted, fontSize: 12 },
})
