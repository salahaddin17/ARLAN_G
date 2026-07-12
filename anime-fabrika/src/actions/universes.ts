'use server'

import { revalidatePath } from 'next/cache'
import { createClient } from '@/lib/supabase/server'

export async function createUniverse(formData: FormData) {
  const supabase = createClient()
  const name = String(formData.get('name') ?? '').trim()
  if (!name) return { error: 'Название обязательно' }

  const slug =
    name
      .toLowerCase()
      .replace(/[^a-z0-9а-яё]+/gi, '-')
      .replace(/^-|-$/g, '') || `universe-${Date.now()}`

  const { data: universe, error } = await supabase
    .from('universes')
    .insert({ name, slug, description: String(formData.get('description') ?? '') })
    .select('id')
    .single()
  if (error) return { error: error.message }

  // Пустая библия стиля сразу — прод-механика промптов требует её наличия
  await supabase.from('style_bibles').insert({
    universe_id: universe.id,
    prompt_template_keyframe:
      '{{style_lock}}, anime key visual, {{characters}}, scene: {{scene}}, camera: {{camera}}, color palette: {{palette}} --no {{negative}}',
    prompt_template_video:
      '{{style_lock}}, anime sequence, {{characters}}, action: {{scene}}, camera: {{camera}}, duration {{duration}}s, color palette: {{palette}} --no {{negative}}',
  })

  revalidatePath('/universes')
  return { ok: true }
}

export async function updateStyleBible(bibleId: string, formData: FormData) {
  const supabase = createClient()

  let palette: unknown = []
  try {
    palette = JSON.parse(String(formData.get('palette') ?? '[]'))
  } catch {
    return { error: 'Палитра должна быть валидным JSON-массивом [{"name","hex"}]' }
  }

  const { data: bible, error } = await supabase
    .from('style_bibles')
    .update({
      art_direction: String(formData.get('art_direction') ?? ''),
      negative_prompt: String(formData.get('negative_prompt') ?? ''),
      prompt_template_keyframe: String(formData.get('prompt_template_keyframe') ?? ''),
      prompt_template_video: String(formData.get('prompt_template_video') ?? ''),
      palette,
    })
    .eq('id', bibleId)
    .select('universe_id')
    .single()
  if (error) return { error: error.message }

  revalidatePath(`/universes/${bible.universe_id}`)
  return { ok: true }
}

export async function createEpisode(universeId: string, formData: FormData) {
  const supabase = createClient()

  const { data: last } = await supabase
    .from('episodes')
    .select('number')
    .eq('universe_id', universeId)
    .order('number', { ascending: false })
    .limit(1)
    .maybeSingle()

  const { error } = await supabase.from('episodes').insert({
    universe_id: universeId,
    number: (last?.number ?? 0) + 1,
    title: String(formData.get('title') ?? 'Без названия'),
    synopsis: String(formData.get('synopsis') ?? ''),
    target_duration_sec: Number(formData.get('target_duration_sec') ?? 60),
  })
  if (error) return { error: error.message }

  revalidatePath(`/universes/${universeId}`)
  return { ok: true }
}
