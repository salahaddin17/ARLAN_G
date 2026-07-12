import { startOfDay } from 'date-fns'
import { createClient } from '@/lib/supabase/server'
import { QueueScreen, type JobRow } from '@/components/queue-screen'

export const dynamic = 'force-dynamic'

export default async function QueuePage() {
  const supabase = createClient()

  const [{ data: jobs }, { data: todayGens }] = await Promise.all([
    supabase
      .from('jobs')
      .select('*, generation:generations(*, shot:shots(code, episode_id))')
      .order('created_at', { ascending: false })
      .limit(50)
      .returns<JobRow[]>(),
    supabase
      .from('generations')
      .select('cost_usd')
      .gte('created_at', startOfDay(new Date()).toISOString()),
  ])

  const costToday = (todayGens ?? []).reduce((sum, g) => sum + Number(g.cost_usd), 0)

  return <QueueScreen jobs={jobs ?? []} costToday={costToday} />
}
