// Каталог моделей с ценами и выбор адаптера по провайдеру.
// Пока каждый провайдер обслуживается мок-адаптером; когда появятся ключи,
// достаточно заменить запись в ADAPTERS на реальную реализацию.

import type { GenerationKind, ProviderName } from '@/lib/types'
import type { GenerationAdapter } from './adapter'
import { createMockAdapter } from './mock'

export interface ModelInfo {
  provider: ProviderName
  modelName: string
  kind: GenerationKind
  /** Для image/voice — цена за единицу; для video — за секунду */
  unit: 'item' | 'second'
  priceUsd: number
  label: string
}

export const MODELS: ModelInfo[] = [
  { provider: 'fal', modelName: 'flux-pro-1.1', kind: 'keyframe', unit: 'item', priceUsd: 0.05, label: 'fal.ai · FLUX Pro 1.1' },
  { provider: 'fal', modelName: 'flux-dev', kind: 'keyframe', unit: 'item', priceUsd: 0.025, label: 'fal.ai · FLUX Dev' },
  { provider: 'kling', modelName: 'kling-2.1-pro', kind: 'video', unit: 'second', priceUsd: 0.09, label: 'Kling 2.1 Pro' },
  { provider: 'seedance', modelName: 'seedance-1.0-pro', kind: 'video', unit: 'second', priceUsd: 0.06, label: 'Seedance 1.0 Pro' },
  { provider: 'veo', modelName: 'veo-3', kind: 'video', unit: 'second', priceUsd: 0.4, label: 'Google Veo 3' },
  { provider: 'elevenlabs', modelName: 'eleven-v3', kind: 'voice', unit: 'item', priceUsd: 0.03, label: 'ElevenLabs v3' },
]

export function modelsForKind(kind: GenerationKind): ModelInfo[] {
  return MODELS.filter((m) => m.kind === kind)
}

export function findModel(provider: ProviderName, modelName: string): ModelInfo | undefined {
  return MODELS.find((m) => m.provider === provider && m.modelName === modelName)
}

export function estimateCost(model: ModelInfo, durationSec: number): number {
  return model.unit === 'second'
    ? Math.round(model.priceUsd * durationSec * 10000) / 10000
    : model.priceUsd
}

const ADAPTERS: Record<ProviderName, GenerationAdapter> = {
  mock: createMockAdapter('mock'),
  // TODO: заменить на реальные адаптеры, когда появятся API-ключи
  fal: createMockAdapter('fal'),
  kling: createMockAdapter('kling'),
  seedance: createMockAdapter('seedance'),
  veo: createMockAdapter('veo'),
  elevenlabs: createMockAdapter('elevenlabs'),
}

export function getAdapter(provider: ProviderName): GenerationAdapter {
  return ADAPTERS[provider] ?? ADAPTERS.mock
}
