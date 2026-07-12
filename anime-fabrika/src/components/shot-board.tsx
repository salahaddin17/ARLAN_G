'use client'

import { useState, useTransition } from 'react'
import { useRouter } from 'next/navigation'
import {
  DndContext,
  PointerSensor,
  useDraggable,
  useDroppable,
  useSensor,
  useSensors,
  type DragEndEvent,
} from '@dnd-kit/core'
import { GripVertical, ImageOff, Plus } from 'lucide-react'
import { createShot, setShotStatus } from '@/actions/shots'
import { ShotDetailDialog, type GenerationWithAsset } from '@/components/shot-detail-dialog'
import { Badge } from '@/components/ui/badge'
import { Button } from '@/components/ui/button'
import {
  Dialog,
  DialogContent,
  DialogHeader,
  DialogTitle,
  DialogTrigger,
} from '@/components/ui/dialog'
import { Input } from '@/components/ui/input'
import { Textarea } from '@/components/ui/textarea'
import { cn } from '@/lib/utils'
import {
  SHOT_STATUS_LABELS,
  SHOT_STATUS_ORDER,
  type Shot,
  type ShotStatus,
} from '@/lib/types'

interface Props {
  episodeId: string
  shots: Shot[]
  generations: GenerationWithAsset[]
  characters: { id: string; name: string }[]
}

export function ShotBoard({ episodeId, shots, generations, characters }: Props) {
  const router = useRouter()
  const [, startTransition] = useTransition()
  const [optimistic, setOptimistic] = useState<Record<string, ShotStatus>>({})
  const [openShotId, setOpenShotId] = useState<string | null>(null)
  const [error, setError] = useState<string | null>(null)

  const sensors = useSensors(useSensor(PointerSensor, { activationConstraint: { distance: 4 } }))

  function statusOf(shot: Shot): ShotStatus {
    return optimistic[shot.id] ?? shot.status
  }

  function onDragEnd(event: DragEndEvent) {
    const shotId = String(event.active.id)
    const target = event.over?.id as ShotStatus | undefined
    const shot = shots.find((s) => s.id === shotId)
    if (!shot || !target || statusOf(shot) === target) return

    setOptimistic((prev) => ({ ...prev, [shotId]: target }))
    setError(null)
    startTransition(async () => {
      const result = await setShotStatus(shotId, target)
      if (result && 'error' in result && result.error) {
        setOptimistic((prev) => {
          const next = { ...prev }
          delete next[shotId]
          return next
        })
        setError(result.error)
      } else {
        router.refresh()
      }
    })
  }

  const openShot = shots.find((s) => s.id === openShotId) ?? null

  return (
    <div className="space-y-3">
      {error && (
        <p className="rounded-md border border-destructive/50 bg-destructive/10 px-3 py-2 text-sm text-destructive">
          {error}
        </p>
      )}

      <DndContext sensors={sensors} onDragEnd={onDragEnd}>
        <div className="grid grid-cols-2 gap-3 md:grid-cols-3 xl:grid-cols-6">
          {SHOT_STATUS_ORDER.map((status) => (
            <BoardColumn
              key={status}
              status={status}
              shots={shots.filter((s) => statusOf(s) === status)}
              generations={generations}
              onOpen={setOpenShotId}
            />
          ))}
        </div>
      </DndContext>

      <NewShotDialog episodeId={episodeId} characters={characters} />

      {openShot && (
        <ShotDetailDialog
          shot={openShot}
          generations={generations.filter((g) => g.shot_id === openShot.id)}
          characters={characters}
          open
          onOpenChange={(open) => !open && setOpenShotId(null)}
        />
      )}
    </div>
  )
}

function BoardColumn({
  status,
  shots,
  generations,
  onOpen,
}: {
  status: ShotStatus
  shots: Shot[]
  generations: GenerationWithAsset[]
  onOpen: (id: string) => void
}) {
  const { setNodeRef, isOver } = useDroppable({ id: status })

  return (
    <div
      ref={setNodeRef}
      className={cn(
        'flex min-h-[50vh] flex-col gap-2 rounded-lg border bg-card/50 p-2',
        isOver && 'border-primary/60 bg-primary/5'
      )}
    >
      <div className="flex items-center justify-between px-1 py-0.5">
        <span className="text-xs font-medium text-muted-foreground">
          {SHOT_STATUS_LABELS[status]}
        </span>
        <Badge variant="secondary">{shots.length}</Badge>
      </div>
      {shots.map((shot) => (
        <ShotCard key={shot.id} shot={shot} generations={generations} onOpen={onOpen} />
      ))}
    </div>
  )
}

function ShotCard({
  shot,
  generations,
  onOpen,
}: {
  shot: Shot
  generations: GenerationWithAsset[]
  onOpen: (id: string) => void
}) {
  const { attributes, listeners, setNodeRef, transform, isDragging } = useDraggable({
    id: shot.id,
  })

  const shotGens = generations.filter((g) => g.shot_id === shot.id)
  const keyframeUrl =
    shotGens.find((g) => g.result_asset_id === shot.keyframe_asset_id)?.asset?.external_url ??
    shotGens.find((g) => g.kind === 'keyframe' && g.status === 'succeeded')?.asset?.external_url

  return (
    <div
      ref={setNodeRef}
      style={
        transform ? { transform: `translate(${transform.x}px, ${transform.y}px)` } : undefined
      }
      className={cn(
        'rounded-md border bg-card shadow-sm transition-colors hover:border-primary/40',
        isDragging && 'z-10 opacity-80'
      )}
    >
      <button className="block w-full text-left" onClick={() => onOpen(shot.id)}>
        {keyframeUrl ? (
          // eslint-disable-next-line @next/next/no-img-element
          <img
            src={keyframeUrl}
            alt={shot.code}
            className="aspect-video w-full rounded-t-md object-cover"
          />
        ) : (
          <div className="flex aspect-video w-full items-center justify-center rounded-t-md bg-muted">
            <ImageOff className="h-5 w-5 text-muted-foreground" />
          </div>
        )}
        <div className="p-2">
          <p className="text-xs font-semibold">{shot.code}</p>
          <p className="line-clamp-2 text-xs text-muted-foreground">{shot.scene_description}</p>
          <p className="mt-1 text-[10px] text-muted-foreground">
            {shot.duration_sec}s · дублей: {shotGens.length}
          </p>
        </div>
      </button>
      <div
        {...attributes}
        {...listeners}
        className="flex cursor-grab justify-center border-t py-0.5 text-muted-foreground active:cursor-grabbing"
        title="Перетащить в другой статус"
      >
        <GripVertical className="h-3.5 w-3.5 rotate-90" />
      </div>
    </div>
  )
}

function NewShotDialog({
  episodeId,
  characters,
}: {
  episodeId: string
  characters: { id: string; name: string }[]
}) {
  const [open, setOpen] = useState(false)

  return (
    <Dialog open={open} onOpenChange={setOpen}>
      <DialogTrigger asChild>
        <Button variant="secondary">
          <Plus className="h-4 w-4" /> Новый шот
        </Button>
      </DialogTrigger>
      <DialogContent>
        <DialogHeader>
          <DialogTitle>Новый шот</DialogTitle>
        </DialogHeader>
        <form
          action={async (fd) => {
            await createShot(episodeId, fd)
            setOpen(false)
          }}
          className="space-y-3"
        >
          <Textarea name="scene_description" placeholder="Описание сцены" rows={3} required />
          <Textarea name="dialogue" placeholder="Реплики (если есть)" rows={2} />
          <Input name="camera" placeholder="Камера: wide shot, slow push-in…" />
          <div className="flex items-center gap-2">
            <Input name="duration_sec" type="number" defaultValue={5} min={1} className="w-24" />
            <span className="text-xs text-muted-foreground">сек.</span>
          </div>
          <div className="space-y-1">
            <p className="text-xs text-muted-foreground">Персонажи в кадре:</p>
            <div className="flex flex-wrap gap-3">
              {characters.map((c) => (
                <label key={c.id} className="flex items-center gap-1.5 text-sm">
                  <input type="checkbox" name="character_ids" value={c.id} />
                  {c.name}
                </label>
              ))}
            </div>
          </div>
          <Button type="submit">Создать</Button>
        </form>
      </DialogContent>
    </Dialog>
  )
}
