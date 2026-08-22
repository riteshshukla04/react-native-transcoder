export { Media } from './Media'
export * from './TranscoderError'

export type { MediaFactory } from './specs/MediaFactory.nitro'
export type {
  AudioChannelRequest,
  AudioQualityRequest,
  AudioRequestMode,
  AudioSampleRateRequest,
  AudioTranscodeRequest,
  GaplessPolicy,
  QualityMode,
  TrackSelection,
  TranscodeRequest,
} from './specs/MediaFactory.nitro'

export type { MediaSource } from './specs/MediaSource.nitro'
export type {
  CacheMode,
  FileSourceOptions,
  MediaSourceKind,
  NetworkAccess,
  SourceValidation,
  UrlCachePolicy,
  UrlSourceOptions,
} from './specs/MediaSource.nitro'

export type { MediaDestination } from './specs/MediaDestination.nitro'
export type {
  FileDestinationOptions,
  MediaDestinationKind,
  OverwritePolicy,
} from './specs/MediaDestination.nitro'

export type { MediaAsset } from './specs/MediaAsset.nitro'
export type { ResolvedPlan } from './specs/ResolvedPlan.nitro'
export type {
  PlanConversion,
  PlanPath,
  PlanWarning,
  PlanWarningCode,
} from './specs/ResolvedPlan.nitro'

export type { TranscodeJob } from './specs/TranscodeJob.nitro'
export type {
  JobPhase,
  JobState,
  ProgressSubscription,
  TranscodeProgress,
} from './specs/TranscodeJob.nitro'

export type { TranscodeReport } from './specs/TranscodeReport.nitro'

export type {
  AudioCodecId,
  AudioStreamDescriptor,
  BitRateMode,
  ChapterDescriptor,
  CodecCapability,
  CodecDirection,
  ContainerCapability,
  ContainerId,
  MediaCapabilitiesSnapshot,
  MetadataPolicy,
  OutcomePolicy,
  ProcessorId,
  SampleFormat,
  ValuePolicy,
} from './specs/MediaTypes.nitro'
