export function formatBytes(bytes: number | undefined) {
  if (bytes == null) return '—'
  if (bytes < 1024) return `${bytes} B`
  if (bytes < 1024 * 1024) return `${(bytes / 1024).toFixed(1)} KB`
  return `${(bytes / (1024 * 1024)).toFixed(2)} MB`
}

export function formatSeconds(seconds: number | undefined) {
  if (seconds == null) return '—'
  if (seconds < 60) return `${seconds.toFixed(3)} s`
  const minutes = Math.floor(seconds / 60)
  return `${minutes}m ${(seconds - minutes * 60).toFixed(1)}s`
}

export function formatBitRate(bitsPerSecond: number | undefined) {
  if (bitsPerSecond == null) return '—'
  return `${Math.round(bitsPerSecond / 1000)} kbps`
}
