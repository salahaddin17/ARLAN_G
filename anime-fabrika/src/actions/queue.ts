'use server'

import { revalidatePath } from 'next/cache'
import { processJobs, type WorkerSummary } from '@/lib/queue/worker'

/** Ручной прогон воркера с экрана очереди (в проде это делает cron). */
export async function runWorkerOnce(): Promise<WorkerSummary> {
  const summary = await processJobs(10)
  revalidatePath('/queue')
  revalidatePath('/dashboard')
  return summary
}
