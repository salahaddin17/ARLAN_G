'use server'

import { revalidatePath } from 'next/cache'
import { createClient } from '@/lib/supabase/server'
import type { Generation } from '@/lib/types'

/** Ретрай упавшей генерации: её джоб снова становится pending. */
export async function retryGeneration(generationId: string) {
  const supabase = createClient()

  const { data: job } = await supabase
    .from('jobs')
    .select('id')
    .eq('generation_id', generationId)
    .order('created_at', { ascending: false })
    .limit(1)
    .maybeSingle()

  if (job) {
    await supabase
      .from('jobs')
      .update({
        status: 'pending',
        attempts: 0,
        run_after: new Date().toISOString(),
        locked_at: null,
      })
      .eq('id', job.id)
  } else {
    await supabase.from('jobs').insert({ generation_id: generationId })
  }

  const { error } = await supabase
    .from('generations')
    .update({ status: 'queued', error: null })
    .eq('id', generationId)
  if (error) return { error: error.message }

  revalidatePath('/queue')
  return { ok: true }
}

/** Выбор лучшего дубля: hit у этой генерации, miss у остальных того же вида. */
export async function markBestTake(generationId: string) {
  const supabase = createClient()

  const { data: generation } = await supabase
    .from('generations')
    .select('id, shot_id, kind, status, result_asset_id, shots(episode_id)')
    .eq('id', generationId)
    .single<Generation & { shots: { episode_id: string } }>()
  if (!generation) return { error: 'Генерация не найдена' }
  if (generation.status !== 'succeeded') return { error: 'Выбрать можно только успешный дубль' }

  await supabase
    .from('generations')
    .update({ is_hit: false })
    .eq('shot_id', generation.shot_id)
    .eq('kind', generation.kind)

  await supabase.from('generations').update({ is_hit: true }).eq('id', generationId)

  if (generation.kind === 'keyframe') {
    await supabase
      .from('shots')
      .update({ keyframe_asset_id: generation.result_asset_id })
      .eq('id', generation.shot_id)
  } else if (generation.kind === 'video') {
    await supabase
      .from('shots')
      .update({ selected_generation_id: generationId })
      .eq('id', generation.shot_id)
  }

  revalidatePath(`/episodes/${generation.shots.episode_id}/board`)
  return { ok: true }
}
