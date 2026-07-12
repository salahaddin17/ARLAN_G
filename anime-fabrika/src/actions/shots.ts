'use server'

import { revalidatePath } from 'next/cache'
import { createClient } from '@/lib/supabase/server'
import {
  assemblePrompt,
  buildShotVars,
  templateForKind,
  type CharacterWithVersion,
} from '@/lib/prompt/assemble'
import { estimateCost, findModel } from '@/lib/providers/registry'
import {
  SHOT_STATUS_ORDER,
  type Character,
  type CharacterVersion,
  type GenerationKind,
  type ProviderName,
  type Shot,
  type ShotStatus,
  type StyleBible,
} from '@/lib/types'

function boardPath(episodeId: string) {
  return `/episodes/${episodeId}/board`
}

export async function createShot(episodeId: string, formData: FormData) {
  const supabase = createClient()

  const { data: episode } = await supabase
    .from('episodes')
    .select('id, number')
    .eq('id', episodeId)
    .single()
  if (!episode) return { error: 'Эпизод не найден' }

  const { count } = await supabase
    .from('shots')
    .select('id', { count: 'exact', head: true })
    .eq('episode_id', episodeId)

  const orderIndex = (count ?? 0) + 1
  const code = `E${String(episode.number).padStart(2, '0')}-S${String(orderIndex).padStart(2, '0')}`

  const { error } = await supabase.from('shots').insert({
    episode_id: episodeId,
    order_index: orderIndex,
    code,
    scene_description: String(formData.get('scene_description') ?? ''),
    dialogue: String(formData.get('dialogue') ?? ''),
    camera: String(formData.get('camera') ?? ''),
    duration_sec: Number(formData.get('duration_sec') ?? 5),
    character_ids: formData.getAll('character_ids').map(String),
  })
  if (error) return { error: error.message }

  revalidatePath(boardPath(episodeId))
  return { ok: true }
}

export async function updateShotFields(
  shotId: string,
  fields: Pick<Shot, 'scene_description' | 'dialogue' | 'camera'> & { duration_sec: number }
) {
  const supabase = createClient()
  const { data: shot, error } = await supabase
    .from('shots')
    .update(fields)
    .eq('id', shotId)
    .select('episode_id')
    .single()
  if (error) return { error: error.message }
  revalidatePath(boardPath(shot.episode_id))
  return { ok: true }
}

/** Kanban: вперёд только на соседний статус, назад — на любой (доработка). */
export async function setShotStatus(shotId: string, status: ShotStatus) {
  const supabase = createClient()
  const { data: shot } = await supabase
    .from('shots')
    .select('id, status, episode_id')
    .eq('id', shotId)
    .single()
  if (!shot) return { error: 'Шот не найден' }

  const from = SHOT_STATUS_ORDER.indexOf(shot.status as ShotStatus)
  const to = SHOT_STATUS_ORDER.indexOf(status)
  if (to - from > 1) {
    return { error: 'Нельзя перепрыгивать статусы: только на следующий шаг вперёд' }
  }

  const { error } = await supabase.from('shots').update({ status }).eq('id', shotId)
  if (error) return { error: error.message }
  revalidatePath(boardPath(shot.episode_id))
  return { ok: true }
}

interface AssembledPreview {
  keyframe: string
  video: string
  missing: string[]
  vars: Record<string, string>
}

/**
 * «Собрать промпт»: активная библия вселенной + prompt_block текущих версий
 * персонажей + данные шота → шаблоны keyframe/video. Снапшот сохраняется в шот.
 */
export async function assembleShotPrompt(
  shotId: string
): Promise<{ error: string } | { ok: true; preview: AssembledPreview }> {
  const supabase = createClient()

  const { data: shot } = await supabase
    .from('shots')
    .select('*, episodes(universe_id)')
    .eq('id', shotId)
    .single<Shot & { episodes: { universe_id: string } }>()
  if (!shot) return { error: 'Шот не найден' }

  const { data: bible } = await supabase
    .from('style_bibles')
    .select('*')
    .eq('universe_id', shot.episodes.universe_id)
    .eq('is_active', true)
    .single<StyleBible>()
  if (!bible) return { error: 'У вселенной нет активной библии стиля' }

  let characters: CharacterWithVersion[] = []
  if (shot.character_ids.length > 0) {
    const { data } = await supabase
      .from('characters')
      .select('id, name, current_version:character_versions!characters_current_version_id_fkey(prompt_block)')
      .in('id', shot.character_ids)
    const byId = new Map(
      (data ?? []).map((c) => [
        c.id,
        {
          character: { id: c.id, name: c.name as string },
          version: (c.current_version as unknown as Pick<CharacterVersion, 'prompt_block'> | null) ?? null,
        },
      ])
    )
    // порядок как в шоте
    characters = shot.character_ids
      .map((id) => byId.get(id))
      .filter(Boolean) as CharacterWithVersion[]
  }

  const vars = buildShotVars(bible, characters, shot)
  const keyframe = assemblePrompt(templateForKind(bible, 'keyframe'), vars)
  const video = assemblePrompt(templateForKind(bible, 'video'), vars)

  const { error } = await supabase
    .from('shots')
    .update({
      prompt_vars: vars,
      assembled_prompts: { keyframe: keyframe.prompt, video: video.prompt },
    })
    .eq('id', shotId)
  if (error) return { error: error.message }

  revalidatePath(boardPath(shot.episode_id))
  return {
    ok: true,
    preview: {
      keyframe: keyframe.prompt,
      video: video.prompt,
      missing: Array.from(new Set([...keyframe.missing, ...video.missing])),
      vars,
    },
  }
}

/** «В очередь»: создаёт generation + job. Промпт берётся только из снапшота сборки. */
export async function enqueueGeneration(
  shotId: string,
  kind: GenerationKind,
  provider: ProviderName,
  modelName: string
) {
  const supabase = createClient()

  const { data: shot } = await supabase
    .from('shots')
    .select('id, episode_id, status, duration_sec, assembled_prompts')
    .eq('id', shotId)
    .single<Pick<Shot, 'id' | 'episode_id' | 'status' | 'duration_sec' | 'assembled_prompts'>>()
  if (!shot) return { error: 'Шот не найден' }

  const prompt = shot.assembled_prompts?.[kind]
  if (!prompt) return { error: 'Сначала соберите промпт («Собрать промпт»)' }

  const model = findModel(provider, modelName)
  if (!model) return { error: 'Неизвестная модель' }

  const { count } = await supabase
    .from('generations')
    .select('id', { count: 'exact', head: true })
    .eq('shot_id', shotId)
    .eq('kind', kind)

  const { data: generation, error: genError } = await supabase
    .from('generations')
    .insert({
      shot_id: shotId,
      kind,
      provider,
      model_name: modelName,
      prompt,
      attempt_no: (count ?? 0) + 1,
      params: { estimated_cost_usd: estimateCost(model, Number(shot.duration_sec)) },
    })
    .select('id')
    .single()
  if (genError) return { error: genError.message }

  const { error: jobError } = await supabase
    .from('jobs')
    .insert({ generation_id: generation.id })
  if (jobError) return { error: jobError.message }

  // Видео в очереди → шот переходит в «Рендер»
  if (kind === 'video' && (shot.status === 'keyframe_approved' || shot.status === 'draft')) {
    await supabase.from('shots').update({ status: 'rendering' }).eq('id', shotId)
  }

  revalidatePath(boardPath(shot.episode_id))
  revalidatePath('/queue')
  return { ok: true }
}
