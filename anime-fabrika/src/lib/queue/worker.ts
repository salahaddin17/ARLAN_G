// Воркер очереди генераций: забирает pending-джобы (claim_jobs, SKIP LOCKED),
// зовёт адаптер провайдера, пишет ассет и стоимость, ретраит с backoff.
// Вызывается из /api/cron/worker и кнопкой «Прогнать воркер» на экране очереди.

import { createAdminClient } from '@/lib/supabase/admin'
import type { Generation, Job, Shot } from '@/lib/types'
import { getAdapter } from '@/lib/providers/registry'

export interface WorkerSummary {
  claimed: number
  succeeded: number
  retried: number
  failed: number
  errors: string[]
}

const BACKOFF_BASE_SEC = 10

export async function processJobs(limit = 5): Promise<WorkerSummary> {
  const supabase = createAdminClient()
  const summary: WorkerSummary = { claimed: 0, succeeded: 0, retried: 0, failed: 0, errors: [] }

  const { data: jobs, error: claimError } = await supabase.rpc('claim_jobs', { batch: limit })
  if (claimError) {
    summary.errors.push(`claim_jobs: ${claimError.message}`)
    return summary
  }

  for (const job of (jobs ?? []) as Job[]) {
    summary.claimed++
    try {
      await processJob(supabase, job, summary)
    } catch (e) {
      summary.errors.push(`job ${job.id}: ${e instanceof Error ? e.message : String(e)}`)
      await failOrRetry(supabase, job, e instanceof Error ? e.message : String(e), summary)
    }
  }

  return summary
}

type Admin = ReturnType<typeof createAdminClient>

async function processJob(supabase: Admin, job: Job, summary: WorkerSummary) {
  const { data: generation } = await supabase
    .from('generations')
    .select('*')
    .eq('id', job.generation_id)
    .single<Generation>()
  if (!generation) throw new Error('generation not found')

  const { data: shot } = await supabase
    .from('shots')
    .select('*, episodes(universe_id)')
    .eq('id', generation.shot_id)
    .single<Shot & { episodes: { universe_id: string } }>()
  if (!shot) throw new Error('shot not found')

  await supabase
    .from('generations')
    .update({ status: 'running', started_at: new Date().toISOString(), error: null })
    .eq('id', generation.id)

  const adapter = getAdapter(generation.provider)
  const result = await adapter.run({
    generationId: generation.id,
    kind: generation.kind,
    modelName: generation.model_name,
    prompt: generation.prompt,
    params: generation.params,
    durationSec: Number(shot.duration_sec),
    attemptNo: job.attempts, // attempts уже инкрементирован в claim_jobs
  })

  if (!result.ok) {
    await failOrRetry(supabase, job, result.error, summary)
    return
  }

  const assetKind =
    generation.kind === 'keyframe' ? 'image' : generation.kind === 'video' ? 'video' : 'audio'

  const { data: asset, error: assetError } = await supabase
    .from('assets')
    .insert({
      universe_id: shot.episodes.universe_id,
      kind: assetKind,
      external_url: result.assetUrl,
      mime: result.mime,
      width: result.width ?? null,
      height: result.height ?? null,
      duration_sec: result.durationSec ?? null,
      source_generation_id: generation.id,
    })
    .select('id')
    .single()
  if (assetError) throw new Error(`asset insert: ${assetError.message}`)

  await supabase
    .from('generations')
    .update({
      status: 'succeeded',
      cost_usd: result.costUsd,
      result_asset_id: asset.id,
      finished_at: new Date().toISOString(),
    })
    .eq('id', generation.id)

  await supabase.from('jobs').update({ status: 'done', last_error: null }).eq('id', job.id)

  // Видео готово → шот уходит из «Рендер» в «QC»
  if (generation.kind === 'video' && shot.status === 'rendering') {
    await supabase.from('shots').update({ status: 'qc' }).eq('id', shot.id)
  }

  summary.succeeded++
}

async function failOrRetry(supabase: Admin, job: Job, error: string, summary: WorkerSummary) {
  const attemptsUsed = job.attempts // после claim_jobs
  if (attemptsUsed >= job.max_attempts) {
    await supabase.from('jobs').update({ status: 'failed', last_error: error }).eq('id', job.id)
    await supabase
      .from('generations')
      .update({ status: 'failed', error, finished_at: new Date().toISOString() })
      .eq('id', job.generation_id)
    summary.failed++
    return
  }

  const delaySec = BACKOFF_BASE_SEC * 2 ** (attemptsUsed - 1)
  await supabase
    .from('jobs')
    .update({
      status: 'pending',
      run_after: new Date(Date.now() + delaySec * 1000).toISOString(),
      last_error: error,
      locked_at: null,
    })
    .eq('id', job.id)
  await supabase
    .from('generations')
    .update({ status: 'queued', error })
    .eq('id', job.generation_id)
  summary.retried++
}
