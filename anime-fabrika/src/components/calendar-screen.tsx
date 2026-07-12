'use client'

import { useState, useTransition } from 'react'
import { useRouter } from 'next/navigation'
import { addDays, format, isSameDay, startOfDay } from 'date-fns'
import { ru } from 'date-fns/locale'
import { ExternalLink, Plus } from 'lucide-react'
import { createDistAccount, createPublication, setPublicationStatus } from '@/actions/publications'
import { Badge } from '@/components/ui/badge'
import { Button } from '@/components/ui/button'
import { Card, CardContent, CardHeader, CardTitle } from '@/components/ui/card'
import {
  Dialog,
  DialogContent,
  DialogHeader,
  DialogTitle,
  DialogTrigger,
} from '@/components/ui/dialog'
import { Input } from '@/components/ui/input'
import { Label } from '@/components/ui/label'
import {
  Select,
  SelectContent,
  SelectItem,
  SelectTrigger,
  SelectValue,
} from '@/components/ui/select'
import { cn } from '@/lib/utils'
import type { DistAccount, Publication, PublicationStatus } from '@/lib/types'

export type PublicationRow = Publication & {
  account: DistAccount | null
  episode: { number: number; title: string } | null
}

const PUB_BADGE: Record<
  PublicationStatus,
  { label: string; variant: 'secondary' | 'info' | 'success' | 'destructive' }
> = {
  planned: { label: 'план', variant: 'secondary' },
  scheduled: { label: 'запланировано', variant: 'info' },
  published: { label: 'опубликовано', variant: 'success' },
  failed: { label: 'ошибка', variant: 'destructive' },
}

const PLATFORM_ICON: Record<string, string> = {
  youtube: '▶',
  tiktok: '♪',
  instagram: '◎',
  vk: 'В',
}

interface Props {
  publications: PublicationRow[]
  accounts: DistAccount[]
  episodes: { id: string; number: number; title: string }[]
}

export function CalendarScreen({ publications, accounts, episodes }: Props) {
  const router = useRouter()
  const [, startTransition] = useTransition()
  const days = Array.from({ length: 14 }, (_, i) => addDays(startOfDay(new Date()), i))
  const backlog = publications.filter(
    (p) => new Date(p.scheduled_at) < startOfDay(new Date()) && p.status !== 'published'
  )

  return (
    <div className="space-y-6">
      <div className="flex items-center justify-between">
        <h1 className="text-2xl font-semibold">Календарь дистрибуции</h1>
        <div className="flex gap-2">
          <NewAccountDialog />
          <NewPublicationDialog accounts={accounts} episodes={episodes} />
        </div>
      </div>

      <div className="flex flex-wrap gap-2 text-sm text-muted-foreground">
        Аккаунты:
        {accounts.map((a) => (
          <Badge key={a.id} variant="outline">
            {PLATFORM_ICON[a.platform]} {a.handle} · {a.default_format}
          </Badge>
        ))}
        {accounts.length === 0 && '— добавьте первый аккаунт'}
      </div>

      {backlog.length > 0 && (
        <p className="rounded-md border border-amber-500/40 bg-amber-500/10 px-3 py-2 text-sm text-amber-400">
          Просрочено публикаций: {backlog.length}
        </p>
      )}

      <div className="grid grid-cols-2 gap-3 md:grid-cols-4 xl:grid-cols-7">
        {days.map((day) => {
          const dayPubs = publications.filter((p) => isSameDay(new Date(p.scheduled_at), day))
          const isToday = isSameDay(day, new Date())
          return (
            <Card key={day.toISOString()} className={cn(isToday && 'border-primary/60')}>
              <CardHeader className="p-3 pb-1">
                <CardTitle className="text-xs font-medium capitalize text-muted-foreground">
                  {format(day, 'EEEEEE, d MMM', { locale: ru })}
                  {isToday && ' · сегодня'}
                </CardTitle>
              </CardHeader>
              <CardContent className="space-y-2 p-3 pt-1">
                {dayPubs.map((p) => {
                  const badge = PUB_BADGE[p.status]
                  return (
                    <div key={p.id} className="rounded-md border p-2">
                      <p className="text-xs font-medium">
                        {p.account && PLATFORM_ICON[p.account.platform]} {p.account?.handle}
                      </p>
                      <p className="line-clamp-2 text-[11px] text-muted-foreground">
                        E{String(p.episode?.number ?? 0).padStart(2, '0')} · {p.title || p.episode?.title}
                      </p>
                      <div className="mt-1 flex items-center gap-1">
                        <Badge variant={badge.variant} className="text-[10px]">
                          {badge.label}
                        </Badge>
                        <span className="text-[10px] text-muted-foreground">{p.format}</span>
                        {p.external_url && (
                          <a href={p.external_url} target="_blank" rel="noreferrer">
                            <ExternalLink className="h-3 w-3 text-muted-foreground" />
                          </a>
                        )}
                      </div>
                      {p.status !== 'published' && (
                        <Button
                          variant="ghost"
                          size="sm"
                          className="mt-1 h-6 w-full text-[10px]"
                          onClick={() =>
                            startTransition(async () => {
                              await setPublicationStatus(p.id, 'published')
                              router.refresh()
                            })
                          }
                        >
                          Отметить опубликованным
                        </Button>
                      )}
                    </div>
                  )
                })}
                {dayPubs.length === 0 && (
                  <p className="text-[11px] text-muted-foreground/50">—</p>
                )}
              </CardContent>
            </Card>
          )
        })}
      </div>
    </div>
  )
}

function NewPublicationDialog({
  accounts,
  episodes,
}: {
  accounts: DistAccount[]
  episodes: { id: string; number: number; title: string }[]
}) {
  const [open, setOpen] = useState(false)
  const [accountId, setAccountId] = useState('')
  const [episodeId, setEpisodeId] = useState('')
  const [pubFormat, setPubFormat] = useState('shorts')

  return (
    <Dialog open={open} onOpenChange={setOpen}>
      <DialogTrigger asChild>
        <Button>
          <Plus className="h-4 w-4" /> Запланировать
        </Button>
      </DialogTrigger>
      <DialogContent>
        <DialogHeader>
          <DialogTitle>Новая публикация</DialogTitle>
        </DialogHeader>
        <form
          action={async (fd) => {
            fd.set('account_id', accountId)
            fd.set('episode_id', episodeId)
            fd.set('format', pubFormat)
            await createPublication(fd)
            setOpen(false)
          }}
          className="space-y-3"
        >
          <div className="space-y-1">
            <Label>Эпизод</Label>
            <Select value={episodeId} onValueChange={setEpisodeId}>
              <SelectTrigger>
                <SelectValue placeholder="Выберите эпизод" />
              </SelectTrigger>
              <SelectContent>
                {episodes.map((e) => (
                  <SelectItem key={e.id} value={e.id}>
                    E{String(e.number).padStart(2, '0')} · {e.title}
                  </SelectItem>
                ))}
              </SelectContent>
            </Select>
          </div>
          <div className="space-y-1">
            <Label>Аккаунт</Label>
            <Select value={accountId} onValueChange={setAccountId}>
              <SelectTrigger>
                <SelectValue placeholder="Выберите аккаунт" />
              </SelectTrigger>
              <SelectContent>
                {accounts.map((a) => (
                  <SelectItem key={a.id} value={a.id}>
                    {a.platform} · {a.handle}
                  </SelectItem>
                ))}
              </SelectContent>
            </Select>
          </div>
          <div className="space-y-1">
            <Label>Формат</Label>
            <Select value={pubFormat} onValueChange={setPubFormat}>
              <SelectTrigger>
                <SelectValue />
              </SelectTrigger>
              <SelectContent>
                <SelectItem value="shorts">shorts</SelectItem>
                <SelectItem value="reel">reel</SelectItem>
                <SelectItem value="full">full</SelectItem>
              </SelectContent>
            </Select>
          </div>
          <Input name="title" placeholder="Заголовок публикации" />
          <div className="space-y-1">
            <Label>Дата и время</Label>
            <Input name="scheduled_at" type="datetime-local" required />
          </div>
          <Button type="submit" disabled={!accountId || !episodeId}>
            Запланировать
          </Button>
        </form>
      </DialogContent>
    </Dialog>
  )
}

function NewAccountDialog() {
  const [open, setOpen] = useState(false)
  const [platform, setPlatform] = useState('youtube')

  return (
    <Dialog open={open} onOpenChange={setOpen}>
      <DialogTrigger asChild>
        <Button variant="secondary">
          <Plus className="h-4 w-4" /> Аккаунт
        </Button>
      </DialogTrigger>
      <DialogContent className="max-w-sm">
        <DialogHeader>
          <DialogTitle>Новый аккаунт дистрибуции</DialogTitle>
        </DialogHeader>
        <form
          action={async (fd) => {
            fd.set('platform', platform)
            await createDistAccount(fd)
            setOpen(false)
          }}
          className="space-y-3"
        >
          <Select value={platform} onValueChange={setPlatform}>
            <SelectTrigger>
              <SelectValue />
            </SelectTrigger>
            <SelectContent>
              <SelectItem value="youtube">YouTube</SelectItem>
              <SelectItem value="tiktok">TikTok</SelectItem>
              <SelectItem value="instagram">Instagram</SelectItem>
              <SelectItem value="vk">VK</SelectItem>
            </SelectContent>
          </Select>
          <Input name="handle" placeholder="@handle" required />
          <input type="hidden" name="default_format" value="shorts" />
          <Button type="submit">Добавить</Button>
        </form>
      </DialogContent>
    </Dialog>
  )
}
