// Типы строк БД (зеркалят supabase/migrations/0001_init.sql)

export type ShotStatus =
  | 'draft'
  | 'keyframe_approved'
  | 'rendering'
  | 'qc'
  | 'approved'
  | 'published'

export const SHOT_STATUS_ORDER: ShotStatus[] = [
  'draft',
  'keyframe_approved',
  'rendering',
  'qc',
  'approved',
  'published',
]

export const SHOT_STATUS_LABELS: Record<ShotStatus, string> = {
  draft: 'Драфт',
  keyframe_approved: 'Кейфрейм OK',
  rendering: 'Рендер',
  qc: 'QC',
  approved: 'Принят',
  published: 'Опубликован',
}

export type EpisodeStatus = 'writing' | 'storyboard' | 'production' | 'done'
export type GenerationKind = 'keyframe' | 'video' | 'audio' | 'voice'
export type GenerationStatus = 'queued' | 'running' | 'succeeded' | 'failed' | 'canceled'
export type JobStatus = 'pending' | 'processing' | 'done' | 'failed'
export type PublicationStatus = 'planned' | 'scheduled' | 'published' | 'failed'
export type ProviderName = 'mock' | 'fal' | 'kling' | 'seedance' | 'veo' | 'elevenlabs'
export type AssetKind = 'image' | 'video' | 'audio'
export type PubFormat = 'shorts' | 'reel' | 'full'
export type PlatformName = 'youtube' | 'tiktok' | 'instagram' | 'vk'

export interface PaletteEntry {
  name: string
  hex: string
}

export interface Universe {
  id: string
  name: string
  slug: string
  description: string
  cover_url: string | null
  created_at: string
  updated_at: string
}

export interface StyleBible {
  id: string
  universe_id: string
  version: number
  is_active: boolean
  art_direction: string
  palette: PaletteEntry[]
  camera_rules: string[]
  negative_prompt: string
  prompt_template_keyframe: string
  prompt_template_video: string
  created_at: string
  updated_at: string
}

export interface Character {
  id: string
  universe_id: string
  name: string
  slug: string
  role: string
  description: string
  voice_provider: ProviderName
  voice_id: string
  current_version_id: string | null
  created_at: string
  updated_at: string
}

export interface CharacterVersion {
  id: string
  character_id: string
  version_no: number
  model_sheet_url: string | null
  palette: PaletteEntry[]
  prompt_block: string
  voice_settings: Record<string, unknown>
  changelog: string
  created_at: string
}

export interface Episode {
  id: string
  universe_id: string
  number: number
  title: string
  synopsis: string
  script: string
  status: EpisodeStatus
  target_duration_sec: number
  created_at: string
  updated_at: string
}

export interface Shot {
  id: string
  episode_id: string
  order_index: number
  code: string
  status: ShotStatus
  scene_description: string
  dialogue: string
  camera: string
  duration_sec: number
  character_ids: string[]
  prompt_vars: Record<string, string> | null
  assembled_prompts: Partial<Record<GenerationKind, string>>
  keyframe_asset_id: string | null
  selected_generation_id: string | null
  created_at: string
  updated_at: string
}

export interface Generation {
  id: string
  shot_id: string
  kind: GenerationKind
  provider: ProviderName
  model_name: string
  status: GenerationStatus
  prompt: string
  params: Record<string, unknown>
  seed: number | null
  attempt_no: number
  cost_usd: number
  is_hit: boolean
  result_asset_id: string | null
  error: string | null
  started_at: string | null
  finished_at: string | null
  created_at: string
  updated_at: string
}

export interface Asset {
  id: string
  universe_id: string
  kind: AssetKind
  storage_path: string | null
  external_url: string | null
  mime: string | null
  width: number | null
  height: number | null
  duration_sec: number | null
  meta: Record<string, unknown>
  source_generation_id: string | null
  created_at: string
}

export interface Job {
  id: string
  generation_id: string
  status: JobStatus
  run_after: string
  attempts: number
  max_attempts: number
  locked_at: string | null
  last_error: string | null
  created_at: string
  updated_at: string
}

export interface DistAccount {
  id: string
  platform: PlatformName
  handle: string
  default_format: PubFormat
  created_at: string
}

export interface Publication {
  id: string
  episode_id: string
  shot_id: string | null
  account_id: string
  format: PubFormat
  title: string
  scheduled_at: string
  published_at: string | null
  status: PublicationStatus
  external_url: string | null
  created_at: string
  updated_at: string
}

export interface Metric {
  id: string
  publication_id: string
  collected_at: string
  views: number
  likes: number
  comments: number
  shares: number
  watch_time_sec: number
}
