import Link from 'next/link'
import { notFound } from 'next/navigation'
import { ArrowLeft } from 'lucide-react'
import { createClient } from '@/lib/supabase/server'
import { ShotBoard } from '@/components/shot-board'
import { Button } from '@/components/ui/button'
import type { Episode, Shot } from '@/lib/types'
import type { GenerationWithAsset } from '@/components/shot-detail-dialog'

export const dynamic = 'force-dynamic'

export default async function BoardPage({ params }: { params: { id: string } }) {
  const supabase = createClient()

  const { data: episode } = await supabase
    .from('episodes')
    .select('*, universes(name)')
    .eq('id', params.id)
    .single<Episode & { universes: { name: string } }>()
  if (!episode) notFound()

  const [{ data: shots }, { data: characters }] = await Promise.all([
    supabase
      .from('shots')
      .select('*')
      .eq('episode_id', episode.id)
      .order('order_index')
      .returns<Shot[]>(),
    supabase
      .from('characters')
      .select('id, name')
      .eq('universe_id', episode.universe_id)
      .order('name'),
  ])

  const shotIds = (shots ?? []).map((s) => s.id)
  const { data: generations } = shotIds.length
    ? await supabase
        .from('generations')
        .select('*, asset:assets!generations_result_asset_fk(*)')
        .in('shot_id', shotIds)
        .order('created_at')
        .returns<GenerationWithAsset[]>()
    : { data: [] as GenerationWithAsset[] }

  return (
    <div className="space-y-4">
      <div className="flex items-center gap-3">
        <Button asChild variant="ghost" size="icon">
          <Link href={`/universes/${episode.universe_id}`}>
            <ArrowLeft className="h-4 w-4" />
          </Link>
        </Button>
        <div>
          <h1 className="text-2xl font-semibold">
            E{String(episode.number).padStart(2, '0')} · {episode.title}
          </h1>
          <p className="text-sm text-muted-foreground">
            {episode.universes.name} · Shot Board
          </p>
        </div>
      </div>

      <ShotBoard
        episodeId={episode.id}
        shots={shots ?? []}
        generations={generations ?? []}
        characters={(characters ?? []) as { id: string; name: string }[]}
      />
    </div>
  )
}
