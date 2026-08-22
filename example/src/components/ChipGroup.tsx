import React from 'react'
import { StyleSheet, Text, TouchableOpacity, View } from 'react-native'
import { theme } from '../theme'

interface Props<T extends string> {
  label: string
  options: readonly T[]
  value: T | undefined
  onChange: (value: T) => void
}

export function ChipGroup<T extends string>({
  label,
  options,
  value,
  onChange,
}: Props<T>) {
  return (
    <View style={styles.group}>
      <Text style={styles.label}>{label}</Text>
      <View style={styles.chips}>
        {options.map((option) => {
          const selected = option === value
          return (
            <TouchableOpacity
              key={option}
              testID={`chip-${label}-${option}`}
              accessibilityRole="button"
              style={[styles.chip, selected && styles.chipSelected]}
              onPress={() => onChange(option)}
            >
              <Text
                style={[styles.chipText, selected && styles.chipTextSelected]}
              >
                {option}
              </Text>
            </TouchableOpacity>
          )
        })}
      </View>
    </View>
  )
}

const styles = StyleSheet.create({
  group: { gap: 6 },
  label: {
    color: theme.textMuted,
    fontSize: 12,
    textTransform: 'uppercase',
    letterSpacing: 0.6,
  },
  chips: { flexDirection: 'row', flexWrap: 'wrap', gap: 6 },
  chip: {
    paddingVertical: 7,
    paddingHorizontal: 12,
    borderRadius: 999,
    backgroundColor: theme.surfaceRaised,
    borderWidth: 1,
    borderColor: theme.border,
  },
  chipSelected: {
    backgroundColor: theme.accentMuted,
    borderColor: theme.accent,
  },
  chipText: { color: theme.textMuted, fontSize: 13 },
  chipTextSelected: { color: theme.text, fontWeight: '600' },
})
