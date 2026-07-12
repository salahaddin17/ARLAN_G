import Link from 'next/link'
import { notFound } from 'next/navigation'
import { Users } from 'lucide-react'
import { createClient } from '@/lib/supabase/server'
import { createEpisode, updateStyleBible } from '@/actions/universes'
import { Badge } from '@/components/ui/badge'
import { Button } from '@/components/ui/button'
import { Card, CardContent, CardDescription, CardHeader, CardTitle } from '@/components/ui/card'
import { Input } from '@/components/ui/input'
import { Label } from '@/components/ui/label'
import { Textarea } from '@/components/ui/textarea'
import type { Episode, PaletteEntry, StyleBible, Universe } from '@/lib/types'

export const dynamic = 'force-dynamic'

async function updateBibleAction(bibleId: string, formData: FormData) {
  'use server'
  await updateStyleBible(bibleId, formData)
}

const EPISODE_STATUS_LABELS: Record<string, string> = {
  writing: 'Сценарий',
  storyboard: 'Раскадровка',
  production: 'Производство',
  done: 'Готов',
}

export default async function UniversePage({ params }: { params: { id: string } }) {
  async function createEpisodeAction(formData: FormData) {
    'use server'
    await createEpisode(params.id, formData)
  }

  const supabase = createClient()

  const [{ data: universe }, { data: bible }, { data: episodes }] = await Promise.all([
    supabase.from('universes').select('*').eq('id', params.id).single<Universe>(),
    supabase
      .from('style_bibles')
      .select('*')
      .eq('universe_id', params.id)
      .eq('is_active', true)
      .maybeSingle<StyleBible>(),
    supabase
      .from('episodes')
      .select('*')
      .eq('universe_id', params.id)
      .order('number')
      .returns<Episode[]>(),
  ])

  if (!universe) notFound()

  return (
    <div className="space-y-6">
      <div className="flex items-start justify-between">
        <div>
          <h1 className="text-2xl font-semibold">{universe.name}</h1>
          <p className="mt-1 max-w-2xl text-sm text-muted-foreground">{universe.description}</p>
        </div>
        <Button asChild variant="secondary">
          <Link href={`/universes/${universe.id}/characters`}>
            <Users className="h-4 w-4" /> Персонажи
          </Link>
        </Button>
      </div>

      <div className="grid gap-6 lg:grid-cols-2">
        <Card>
          <CardHeader>
            <CardTitle className="text-base">Библия стиля</CardTitle>
            <CardDescription>
              Шаблоны промптов. Плейсхолдеры: {'{{style_lock}} {{characters}} {{scene}} {{camera}} {{palette}} {{negative}} {{duration}} {{dialogue}}'}
            </CardDescription>
          </CardHeader>
          <CardContent>
            {bible ? (
              <form action={updateBibleAction.bind(null, bible.id)} className="space-y-3">
                <div className="space-y-1">
                  <Label>Style lock (арт-направление)</Label>
                  <Textarea name="art_direction" defaultValue={bible.art_direction} rows={2} />
                </div>
                <div className="space-y-1">
                  <Label>Негативный промпт</Label>
                  <Textarea name="negative_prompt" defaultValue={bible.negative_prompt} rows={2} />
                </div>
                <div className="space-y-1">
                  <Label>Шаблон кейфрейма</Label>
                  <Textarea
                    name="prompt_template_keyframe"
                    defaultValue={bible.prompt_template_keyframe}
                    rows={3}
                    className="font-mono text-xs"
                  />
                </div>
                <div className="space-y-1">
                  <Label>Шаблон видео</Label>
                  <Textarea
                    name="prompt_template_video"
                    defaultValue={bible.prompt_template_video}
                    rows={3}
                    className="font-mono text-xs"
                  />
                </div>
                <div className="space-y-1">
                  <Label>Палитра (JSON)</Label>
                  <Textarea
                    name="palette"
                    defaultValue={JSON.stringify(bible.palette)}
                    rows={2}
                    className="font-mono text-xs"
                  />
                  <div className="flex gap-1 pt-1">
                    {(bible.palette as PaletteEntry[]).map((p) => (
                      <span
                        key={p.hex}
                        title={`${p.name} ${p.hex}`}
                        className="h-5 w-5 rounded border"
                        style={{ backgroundColor: p.hex }}
                      />
                    ))}
                  </div>
                </div>
                <Button type="submit">Сохранить библию (v{bible.version})</Button>
              </form>
            ) : (
              <p className="text-sm text-muted-foreground">Активная библия не найдена.</p>
            )}
          </CardContent>
        </Card>

        <Card>
          <CardHeader>
            <CardTitle className="text-base">Серии</CardTitle>
          </CardHeader>
          <CardContent className="space-y-3">
            {(episodes ?? []).map((e) => (
              <Link
                key={e.id}
                href={`/episodes/${e.id}/board`}
                className="flex items-center justify-between rounded-md border p-3 transition-colors hover:border-primary/50"
              >
                <div>
                  <p className="text-sm font-medium">
                    E{String(e.number).padStart(2, '0')} · {e.title}
                  </p>
                  <p className="line-clamp-1 text-xs text-muted-foreground">{e.synopsis}</p>
                </div>
                <Badge variant="secondary">{EPISODE_STATUS_LABELS[e.status] ?? e.status}</Badge>
              </Link>
            ))}
            {(episodes ?? []).length === 0 && (
              <p className="text-sm text-muted-foreground">Серий пока нет.</p>
            )}

            <form action={createEpisodeAction} className="space-y-2 border-t pt-3">
              <Input name="title" placeholder="Название новой серии" required />
              <Textarea name="synopsis" placeholder="Синопсис" rows={2} />
              <div className="flex items-center gap-2">
                <Input
                  name="target_duration_sec"
                  type="number"
                  defaultValue={60}
                  className="w-28"
                />
                <span className="text-xs text-muted-foreground">сек. хронометраж</span>
                <Button type="submit" size="sm" className="ml-auto">
                  Добавить серию
                </Button>
              </div>
            </form>
          </CardContent>
        </Card>
      </div>
    </div>
  )
}
