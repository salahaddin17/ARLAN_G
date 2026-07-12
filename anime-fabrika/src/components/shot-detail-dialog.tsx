'use client'

import { useState, useTransition } from 'react'
import { useRouter } from 'next/navigation'
import { Loader2, RotateCcw, Sparkles, Star } from 'lucide-react'
import { markBestTake, retryGeneration } from '@/actions/generations'
import { assembleShotPrompt, enqueueGeneration } from '@/actions/shots'
import { Badge } from '@/components/ui/badge'
import { Button } from '@/components/ui/button'
import {
  Dialog,
  DialogContent,
  DialogDescription,
  DialogHeader,
  DialogTitle,
} from '@/components/ui/dialog'
import {
  Select,
  SelectContent,
  SelectItem,
  SelectTrigger,
  SelectValue,
} from '@/components/ui/select'
import { Tabs, TabsContent, TabsList, TabsTrigger } from '@/components/ui/tabs'
import { estimateCost, modelsForKind } from '@/lib/providers/registry'
import { formatUsd } from '@/lib/utils'
import type {
  Asset,
  Generation,
  GenerationKind,
  ProviderName,
  Shot,
} from '@/lib/types'

export type GenerationWithAsset = Generation & { asset: Asset | null }

const GEN_STATUS_BADGE: Record<
  Generation['status'],
  { label: string; variant: 'secondary' | 'info' | 'success' | 'destructive' | 'warning' }
> = {
  queued: { label: 'в очереди', variant: 'secondary' },
  running: { label: 'выполняется', variant: 'info' },
  succeeded: { label: 'готово', variant: 'success' },
  failed: { label: 'ошибка', variant: 'destructive' },
  canceled: { label: 'отменено', variant: 'warning' },
}

interface Props {
  shot: Shot
  generations: GenerationWithAsset[]
  characters: { id: string; name: string }[]
  open: boolean
  onOpenChange: (open: boolean) => void
}

export function ShotDetailDialog({ shot, generations, characters, open, onOpenChange }: Props) {
  const router = useRouter()
  const [pending, startTransition] = useTransition()
  const [message, setMessage] = useState<string | null>(null)
  const [prompts, setPrompts] = useState<{ keyframe?: string; video?: string }>(
    shot.assembled_prompts ?? {}
  )
  const [missing, setMissing] = useState<string[]>([])

  const shotCharacters = characters.filter((c) => shot.character_ids.includes(c.id))

  function assemble() {
    setMessage(null)
    startTransition(async () => {
      const result = await assembleShotPrompt(shot.id)
      if ('error' in result) {
        setMessage(result.error)
        return
      }
      setPrompts({ keyframe: result.preview.keyframe, video: result.preview.video })
      setMissing(result.preview.missing)
      router.refresh()
    })
  }

  return (
    <Dialog open={open} onOpenChange={onOpenChange}>
      <DialogContent className="max-w-3xl">
        <DialogHeader>
          <DialogTitle>
            {shot.code} · {shot.duration_sec}s
          </DialogTitle>
          <DialogDescription>
            {shot.scene_description}
            {shot.camera && <> · камера: {shot.camera}</>}
            {shotCharacters.length > 0 && (
              <> · в кадре: {shotCharacters.map((c) => c.name).join(', ')}</>
            )}
          </DialogDescription>
        </DialogHeader>

        {shot.dialogue && (
          <p className="rounded-md bg-muted p-2 text-sm italic">{shot.dialogue}</p>
        )}

        <div className="flex items-center gap-3">
          <Button onClick={assemble} disabled={pending}>
            {pending ? <Loader2 className="h-4 w-4 animate-spin" /> : <Sparkles className="h-4 w-4" />}
            Собрать промпт
          </Button>
          <span className="text-xs text-muted-foreground">
            Шаблон библии + промпт-блоки персонажей + данные шота. Руками не пишется.
          </span>
        </div>

        {message && <p className="text-sm text-destructive">{message}</p>}
        {missing.length > 0 && (
          <p className="text-xs text-amber-400">
            Не заполнены переменные: {missing.join(', ')} — проверьте библию/персонажей/шот.
          </p>
        )}

        <Tabs defaultValue="keyframe">
          <TabsList>
            <TabsTrigger value="keyframe">Кейфрейм</TabsTrigger>
            <TabsTrigger value="video">Видео</TabsTrigger>
          </TabsList>
          {(['keyframe', 'video'] as const).map((kind) => (
            <TabsContent key={kind} value={kind} className="space-y-3">
              <PromptPanel
                kind={kind}
                prompt={prompts[kind]}
                shot={shot}
                onEnqueued={() => {
                  setMessage(null)
                  router.refresh()
                }}
                onError={setMessage}
              />
              <TakesList kind={kind} shot={shot} generations={generations} />
            </TabsContent>
          ))}
        </Tabs>
      </DialogContent>
    </Dialog>
  )
}

function PromptPanel({
  kind,
  prompt,
  shot,
  onEnqueued,
  onError,
}: {
  kind: GenerationKind
  prompt: string | undefined
  shot: Shot
  onEnqueued: () => void
  onError: (message: string) => void
}) {
  const models = modelsForKind(kind)
  const [selected, setSelected] = useState(
    models[0] ? `${models[0].provider}|${models[0].modelName}` : ''
  )
  const [pending, startTransition] = useTransition()

  const model = models.find((m) => `${m.provider}|${m.modelName}` === selected)
  const cost = model ? estimateCost(model, Number(shot.duration_sec)) : 0

  function enqueue() {
    if (!model) return
    startTransition(async () => {
      const result = await enqueueGeneration(
        shot.id,
        kind,
        model.provider as ProviderName,
        model.modelName
      )
      if (result && 'error' in result && result.error) onError(result.error)
      else onEnqueued()
    })
  }

  return (
    <div className="space-y-2">
      <div className="min-h-16 rounded-md border bg-muted/50 p-3 font-mono text-xs">
        {prompt || (
          <span className="text-muted-foreground">
            Промпт ещё не собран — нажмите «Собрать промпт».
          </span>
        )}
      </div>
      <div className="flex items-center gap-2">
        <Select value={selected} onValueChange={setSelected}>
          <SelectTrigger className="w-56">
            <SelectValue placeholder="Модель" />
          </SelectTrigger>
          <SelectContent>
            {models.map((m) => (
              <SelectItem key={m.modelName} value={`${m.provider}|${m.modelName}`}>
                {m.label}
              </SelectItem>
            ))}
          </SelectContent>
        </Select>
        <span className="text-xs text-muted-foreground">≈ {formatUsd(cost)}</span>
        <Button
          size="sm"
          className="ml-auto"
          disabled={!prompt || !model || pending}
          onClick={enqueue}
        >
          {pending && <Loader2 className="h-4 w-4 animate-spin" />}В очередь
        </Button>
      </div>
    </div>
  )
}

function TakesList({
  kind,
  shot,
  generations,
}: {
  kind: GenerationKind
  shot: Shot
  generations: GenerationWithAsset[]
}) {
  const router = useRouter()
  const [, startTransition] = useTransition()
  const takes = generations.filter((g) => g.kind === kind)
  if (takes.length === 0) {
    return <p className="text-xs text-muted-foreground">Дублей пока нет.</p>
  }

  return (
    <div className="space-y-2">
      <p className="text-xs font-medium text-muted-foreground">Дубли ({takes.length})</p>
      {takes.map((g) => {
        const badge = GEN_STATUS_BADGE[g.status]
        return (
          <div key={g.id} className="flex items-center gap-3 rounded-md border p-2">
            {g.asset?.external_url && g.asset.kind === 'image' ? (
              // eslint-disable-next-line @next/next/no-img-element
              <img
                src={g.asset.external_url}
                alt=""
                className="h-12 w-20 rounded object-cover"
              />
            ) : g.asset?.external_url && g.asset.kind === 'video' ? (
              <video src={g.asset.external_url} className="h-12 w-20 rounded object-cover" muted />
            ) : (
              <div className="h-12 w-20 rounded bg-muted" />
            )}
            <div className="min-w-0 flex-1">
              <p className="text-xs">
                #{g.attempt_no} · {g.provider} · {g.model_name}
              </p>
              <p className="text-[10px] text-muted-foreground">
                {formatUsd(Number(g.cost_usd))}
                {g.error && <span className="text-destructive"> · {g.error}</span>}
              </p>
            </div>
            <Badge variant={badge.variant}>{badge.label}</Badge>
            {g.is_hit && <Star className="h-4 w-4 fill-amber-400 text-amber-400" />}
            {g.status === 'succeeded' && !g.is_hit && (
              <Button
                variant="outline"
                size="sm"
                onClick={() =>
                  startTransition(async () => {
                    await markBestTake(g.id)
                    router.refresh()
                  })
                }
              >
                Лучший
              </Button>
            )}
            {g.status === 'failed' && (
              <Button
                variant="outline"
                size="sm"
                onClick={() =>
                  startTransition(async () => {
                    await retryGeneration(g.id)
                    router.refresh()
                  })
                }
              >
                <RotateCcw className="h-3.5 w-3.5" /> Ретрай
              </Button>
            )}
          </div>
        )
      })}
    </div>
  )
}
