// Мок-адаптер: имитирует генерацию с реалистичной стоимостью и ~15% отказов.
// Результат детерминирован по (generationId, attemptNo) — ретрай может «починить»
// упавшую генерацию, как у реальных провайдеров.

import type { ProviderName } from '@/lib/types'
import type { GenerationAdapter, GenerationRequest, GenerationResult } from './adapter'

function hashString(s: string): number {
  let h = 2166136261
  for (let i = 0; i < s.length; i++) {
    h ^= s.charCodeAt(i)
    h = Math.imul(h, 16777619)
  }
  return h >>> 0
}

const PRICE_PER_SECOND: Record<string, number> = {
  'kling-2.1-pro': 0.09,
  'seedance-1.0-pro': 0.06,
  'veo-3': 0.4,
}

const PRICE_PER_ITEM: Record<string, number> = {
  'flux-pro-1.1': 0.05,
  'flux-dev': 0.025,
  'eleven-v3': 0.03,
}

const MOCK_VIDEO_URL =
  'https://interactive-examples.mdn.mozilla.net/media/cc0-videos/flower.mp4'
const MOCK_AUDIO_URL =
  'https://interactive-examples.mdn.mozilla.net/media/cc0-audio/t-rex-roar.mp3'

export function createMockAdapter(name: ProviderName): GenerationAdapter {
  return {
    name,

    estimateCost({ kind, modelName, durationSec }) {
      if (kind === 'video') {
        const perSec = PRICE_PER_SECOND[modelName] ?? 0.1
        return Math.round(perSec * durationSec * 10000) / 10000
      }
      return PRICE_PER_ITEM[modelName] ?? 0.05
    },

    async run(req: GenerationRequest): Promise<GenerationResult> {
      const roll = hashString(`${req.generationId}:${req.attemptNo}`) % 100
      const costUsd = this.estimateCost(req)

      // ~15% отказов, чтобы очередь и ретраи было на чём проверять
      if (roll < 15) {
        return { ok: false, costUsd: 0, error: `mock ${name}: simulated provider error (roll=${roll})` }
      }

      if (req.kind === 'keyframe') {
        return {
          ok: true,
          costUsd,
          assetUrl: `https://picsum.photos/seed/${req.generationId.slice(0, 8)}-${req.attemptNo}/1024/576`,
          mime: 'image/jpeg',
          width: 1024,
          height: 576,
        }
      }

      if (req.kind === 'video') {
        return {
          ok: true,
          costUsd,
          assetUrl: MOCK_VIDEO_URL,
          mime: 'video/mp4',
          width: 1920,
          height: 1080,
          durationSec: req.durationSec,
        }
      }

      // audio / voice
      return {
        ok: true,
        costUsd,
        assetUrl: MOCK_AUDIO_URL,
        mime: 'audio/mpeg',
        durationSec: Math.min(req.durationSec, 10),
      }
    },
  }
}
