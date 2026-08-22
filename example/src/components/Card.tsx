import React from 'react'
import { StyleSheet, Text, View } from 'react-native'
import { theme } from '../theme'

interface Props {
  title: string
  subtitle?: string
  children: React.ReactNode
}

export function Card({ title, subtitle, children }: Props) {
  return (
    <View style={styles.card}>
      <Text style={styles.title}>{title}</Text>
      {subtitle != null && <Text style={styles.subtitle}>{subtitle}</Text>}
      <View style={styles.body}>{children}</View>
    </View>
  )
}

const styles = StyleSheet.create({
  card: {
    backgroundColor: theme.surface,
    borderRadius: theme.radius,
    borderWidth: 1,
    borderColor: theme.border,
    padding: 16,
    marginBottom: 14,
  },
  title: { color: theme.text, fontSize: 16, fontWeight: '700' },
  subtitle: { color: theme.textMuted, fontSize: 12, marginTop: 2 },
  body: { marginTop: 12, gap: 8 },
})
