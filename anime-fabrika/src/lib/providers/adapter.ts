// Единый интерфейс адаптера генеративного провайдера.
// Реальные реализации (fal.ai / Kling / Seedance / Veo / ElevenLabs)
// подключаются позже — сейчас всё обслуживает mock (см. mock.ts, registry.ts).

import type { GenerationKind, ProviderName } from '@/lib/types'

export interface GenerationRequest {
  generationId: string
  kind: GenerationKind
  modelName: string
  prompt: string
  params: Record<string, unknown>
  durationSec: number
  attemptNo: number
}

export interface GenerationSuccess {
  ok: true
  costUsd: number
  assetUrl: string
  mime: string
  width?: number
  height?: number
  durationSec?: number
}

export interface GenerationFailure {
  ok: false
  costUsd: number
  error: string
}

export type GenerationResult = GenerationSuccess | GenerationFailure

export interface GenerationAdapter {
  name: ProviderName
  /** Оценка стоимости до постановки в очередь (для UI). */
  estimateCost(req: Pick<GenerationRequest, 'kind' | 'modelName' | 'durationSec'>): number
  /** Синхронный (в рамках тика воркера) запуск генерации. */
  run(req: GenerationRequest): Promise<GenerationResult>
}
