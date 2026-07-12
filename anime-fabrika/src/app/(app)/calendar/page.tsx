import { createClient } from '@/lib/supabase/server'
import { CalendarScreen, type PublicationRow } from '@/components/calendar-screen'
import type { DistAccount, Episode } from '@/lib/types'

export const dynamic = 'force-dynamic'

export default async function CalendarPage() {
  const supabase = createClient()

  const [{ data: publications }, { data: accounts }, { data: episodes }] = await Promise.all([
    supabase
      .from('publications')
      .select('*, account:dist_accounts(*), episode:episodes(number, title)')
      .order('scheduled_at')
      .returns<PublicationRow[]>(),
    supabase.from('dist_accounts').select('*').order('created_at').returns<DistAccount[]>(),
    supabase
      .from('episodes')
      .select('id, number, title, universe_id')
      .order('number')
      .returns<Pick<Episode, 'id' | 'number' | 'title' | 'universe_id'>[]>(),
  ])

  return (
    <CalendarScreen
      publications={publications ?? []}
      accounts={accounts ?? []}
      episodes={episodes ?? []}
    />
  )
}
