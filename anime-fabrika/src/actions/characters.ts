'use server'

import { revalidatePath } from 'next/cache'
import { createClient } from '@/lib/supabase/server'

function slugify(name: string): string {
  const map: Record<string, string> = {
    а: 'a', б: 'b', в: 'v', г: 'g', д: 'd', е: 'e', ё: 'e', ж: 'zh', з: 'z',
    и: 'i', й: 'i', к: 'k', л: 'l', м: 'm', н: 'n', о: 'o', п: 'p', р: 'r',
    с: 's', т: 't', у: 'u', ф: 'f', х: 'h', ц: 'ts', ч: 'ch', ш: 'sh',
    щ: 'sch', ъ: '', ы: 'y', ь: '', э: 'e', ю: 'yu', я: 'ya',
  }
  return name
    .toLowerCase()
    .split('')
    .map((ch) => map[ch] ?? ch)
    .join('')
    .replace(/[^a-z0-9]+/g, '-')
    .replace(/^-|-$/g, '')
}

export async function createCharacter(universeId: string, formData: FormData) {
  const supabase = createClient()
  const name = String(formData.get('name') ?? '').trim()
  if (!name) return { error: 'Имя обязательно' }

  const { data: character, error } = await supabase
    .from('characters')
    .insert({
      universe_id: universeId,
      name,
      slug: slugify(name) || `char-${Date.now()}`,
      role: String(formData.get('role') ?? ''),
      description: String(formData.get('description') ?? ''),
      voice_id: String(formData.get('voice_id') ?? ''),
    })
    .select('id')
    .single()
  if (error) return { error: error.message }

  // Первая версия сразу, чтобы у персонажа был prompt_block
  const promptBlock = String(formData.get('prompt_block') ?? '')
  if (promptBlock) {
    const { data: version } = await supabase
      .from('character_versions')
      .insert({
        character_id: character.id,
        version_no: 1,
        prompt_block: promptBlock,
        changelog: 'Первая версия',
      })
      .select('id')
      .single()
    if (version) {
      await supabase
        .from('characters')
        .update({ current_version_id: version.id })
        .eq('id', character.id)
    }
  }

  revalidatePath(`/universes/${universeId}/characters`)
  return { ok: true }
}

export async function addCharacterVersion(characterId: string, formData: FormData) {
  const supabase = createClient()

  const { data: character } = await supabase
    .from('characters')
    .select('id, universe_id')
    .eq('id', characterId)
    .single()
  if (!character) return { error: 'Персонаж не найден' }

  const { data: last } = await supabase
    .from('character_versions')
    .select('version_no')
    .eq('character_id', characterId)
    .order('version_no', { ascending: false })
    .limit(1)
    .maybeSingle()

  const { data: version, error } = await supabase
    .from('character_versions')
    .insert({
      character_id: characterId,
      version_no: (last?.version_no ?? 0) + 1,
      prompt_block: String(formData.get('prompt_block') ?? ''),
      model_sheet_url: String(formData.get('model_sheet_url') ?? '') || null,
      changelog: String(formData.get('changelog') ?? ''),
    })
    .select('id')
    .single()
  if (error) return { error: error.message }

  await supabase
    .from('characters')
    .update({ current_version_id: version.id })
    .eq('id', characterId)

  revalidatePath(`/universes/${character.universe_id}/characters`)
  return { ok: true }
}

/** Откат на конкретную версию (текущей становится выбранная). */
export async function setCurrentVersion(characterId: string, versionId: string) {
  const supabase = createClient()
  const { data: character, error } = await supabase
    .from('characters')
    .update({ current_version_id: versionId })
    .eq('id', characterId)
    .select('universe_id')
    .single()
  if (error) return { error: error.message }
  revalidatePath(`/universes/${character.universe_id}/characters`)
  return { ok: true }
}
