import React from 'react'
import {
  ActivityIndicator,
  StyleSheet,
  Text,
  TouchableOpacity,
} from 'react-native'
import { theme } from '../theme'

interface Props {
  title: string
  onPress: () => void
  variant?: 'primary' | 'secondary' | 'danger'
  disabled?: boolean
  busy?: boolean
  testID?: string
}

export function Button({
  title,
  onPress,
  variant = 'primary',
  disabled = false,
  busy = false,
  testID,
}: Props) {
  const background =
    variant === 'primary'
      ? theme.accent
      : variant === 'danger'
        ? theme.danger
        : theme.surfaceRaised
  return (
    <TouchableOpacity
      testID={testID}
      accessibilityRole="button"
      style={[
        styles.button,
        { backgroundColor: background },
        disabled && styles.disabled,
      ]}
      onPress={onPress}
      disabled={disabled || busy}
    >
      {busy ? (
        <ActivityIndicator color={theme.text} />
      ) : (
        <Text style={styles.title}>{title}</Text>
      )}
    </TouchableOpacity>
  )
}

const styles = StyleSheet.create({
  button: {
    paddingVertical: 12,
    paddingHorizontal: 16,
    borderRadius: 10,
    alignItems: 'center',
    justifyContent: 'center',
    minHeight: 44,
  },
  disabled: { opacity: 0.4 },
  title: { color: '#fff', fontWeight: '600', fontSize: 14 },
})
