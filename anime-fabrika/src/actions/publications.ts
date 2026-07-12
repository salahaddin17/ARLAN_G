'use server'

import { revalidatePath } from 'next/cache'
import { createClient } from '@/lib/supabase/server'
import type { PubFormat, PublicationStatus } from '@/lib/types'

export async function createPublication(formData: FormData) {
  const supabase = createClient()

  const scheduledAt = String(formData.get('scheduled_at') ?? '')
  if (!scheduledAt) return { error: 'Дата публикации обязательна' }

  const { error } = await supabase.from('publications').insert({
    episode_id: String(formData.get('episode_id')),
    account_id: String(formData.get('account_id')),
    format: String(formData.get('format') ?? 'shorts') as PubFormat,
    title: String(formData.get('title') ?? ''),
    scheduled_at: new Date(scheduledAt).toISOString(),
    status: 'planned',
  })
  if (error) return { error: error.message }

  revalidatePath('/calendar')
  return { ok: true }
}

export async function setPublicationStatus(
  publicationId: string,
  status: PublicationStatus,
  externalUrl?: string
) {
  const supabase = createClient()
  const { error } = await supabase
    .from('publications')
    .update({
      status,
      external_url: externalUrl ?? null,
      published_at: status === 'published' ? new Date().toISOString() : null,
    })
    .eq('id', publicationId)
  if (error) return { error: error.message }
  revalidatePath('/calendar')
  return { ok: true }
}

export async function createDistAccount(formData: FormData) {
  const supabase = createClient()
  const { error } = await supabase.from('dist_accounts').insert({
    platform: String(formData.get('platform')),
    handle: String(formData.get('handle')),
    default_format: String(formData.get('default_format') ?? 'shorts'),
  })
  if (error) return { error: error.message }
  revalidatePath('/calendar')
  return { ok: true }
}
