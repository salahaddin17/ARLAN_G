'use client'

import { useEffect, useState, useTransition } from 'react'
import { useRouter } from 'next/navigation'
import { formatDistanceToNow } from 'date-fns'
import { ru } from 'date-fns/locale'
import { Loader2, Play, RotateCcw } from 'lucide-react'
import { retryGeneration } from '@/actions/generations'
import { runWorkerOnce } from '@/actions/queue'
import { Badge } from '@/components/ui/badge'
import { Button } from '@/components/ui/button'
import { Card, CardContent, CardHeader, CardTitle } from '@/components/ui/card'
import {
  Table,
  TableBody,
  TableCell,
  TableHead,
  TableHeader,
  TableRow,
} from '@/components/ui/table'
import { formatUsd } from '@/lib/utils'
import type { Generation, Job, JobStatus } from '@/lib/types'

export type JobRow = Job & {
  generation: (Generation & { shot: { code: string; episode_id: string } | null }) | null
}

const JOB_BADGE: Record<JobStatus, { label: string; variant: 'secondary' | 'info' | 'success' | 'destructive' }> = {
  pending: { label: 'ожидает', variant: 'secondary' },
  processing: { label: 'в работе', variant: 'info' },
  done: { label: 'готово', variant: 'success' },
  failed: { label: 'провал', variant: 'destructive' },
}

export function QueueScreen({ jobs, costToday }: { jobs: JobRow[]; costToday: number }) {
  const router = useRouter()
  const [pending, startTransition] = useTransition()
  const [lastRun, setLastRun] = useState<string | null>(null)

  // Экран очереди живёт поллингом: раз в 5 секунд перечитываем данные
  useEffect(() => {
    const timer = setInterval(() => router.refresh(), 5000)
    return () => clearInterval(timer)
  }, [router])

  const active = jobs.filter((j) => j.status === 'pending' || j.status === 'processing')
  const estimatedPending = active.reduce(
    (sum, j) => sum + Number((j.generation?.params as { estimated_cost_usd?: number })?.estimated_cost_usd ?? 0),
    0
  )

  function runWorker() {
    startTransition(async () => {
      const summary = await runWorkerOnce()
      setLastRun(
        `Взято ${summary.claimed}, успешно ${summary.succeeded}, ретраев ${summary.retried}, провалов ${summary.failed}`
      )
      router.refresh()
    })
  }

  return (
    <div className="space-y-6">
      <div className="flex items-center justify-between">
        <h1 className="text-2xl font-semibold">Очередь генераций</h1>
        <Button onClick={runWorker} disabled={pending}>
          {pending ? <Loader2 className="h-4 w-4 animate-spin" /> : <Play className="h-4 w-4" />}
          Прогнать воркер
        </Button>
      </div>

      {lastRun && <p className="text-sm text-muted-foreground">Последний прогон: {lastRun}</p>}

      <div className="grid grid-cols-3 gap-3">
        <Card>
          <CardHeader className="p-4 pb-1">
            <CardTitle className="text-xs font-normal text-muted-foreground">В очереди</CardTitle>
          </CardHeader>
          <CardContent className="p-4 pt-0 text-2xl font-semibold">{active.length}</CardContent>
        </Card>
        <Card>
          <CardHeader className="p-4 pb-1">
            <CardTitle className="text-xs font-normal text-muted-foreground">
              Ожидаемая стоимость очереди
            </CardTitle>
          </CardHeader>
          <CardContent className="p-4 pt-0 text-2xl font-semibold">
            {formatUsd(estimatedPending)}
          </CardContent>
        </Card>
        <Card>
          <CardHeader className="p-4 pb-1">
            <CardTitle className="text-xs font-normal text-muted-foreground">
              Потрачено сегодня
            </CardTitle>
          </CardHeader>
          <CardContent className="p-4 pt-0 text-2xl font-semibold">
            {formatUsd(costToday)}
          </CardContent>
        </Card>
      </div>

      <Table>
        <TableHeader>
          <TableRow>
            <TableHead>Шот</TableHead>
            <TableHead>Тип</TableHead>
            <TableHead>Модель</TableHead>
            <TableHead>Статус</TableHead>
            <TableHead className="text-right">Попытки</TableHead>
            <TableHead className="text-right">Стоимость</TableHead>
            <TableHead>Создано</TableHead>
            <TableHead />
          </TableRow>
        </TableHeader>
        <TableBody>
          {jobs.map((job) => {
            const badge = JOB_BADGE[job.status]
            const g = job.generation
            return (
              <TableRow key={job.id}>
                <TableCell className="font-medium">{g?.shot?.code ?? '—'}</TableCell>
                <TableCell>{g?.kind}</TableCell>
                <TableCell className="text-muted-foreground">
                  {g?.provider} · {g?.model_name}
                </TableCell>
                <TableCell>
                  <Badge variant={badge.variant}>{badge.label}</Badge>
                  {job.last_error && (
                    <p className="mt-0.5 max-w-56 truncate text-[10px] text-destructive">
                      {job.last_error}
                    </p>
                  )}
                </TableCell>
                <TableCell className="text-right">
                  {job.attempts}/{job.max_attempts}
                </TableCell>
                <TableCell className="text-right">{formatUsd(Number(g?.cost_usd ?? 0))}</TableCell>
                <TableCell className="text-xs text-muted-foreground">
                  {formatDistanceToNow(new Date(job.created_at), { addSuffix: true, locale: ru })}
                </TableCell>
                <TableCell>
                  {job.status === 'failed' && g && (
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
                </TableCell>
              </TableRow>
            )
          })}
          {jobs.length === 0 && (
            <TableRow>
              <TableCell colSpan={8} className="text-center text-muted-foreground">
                Очередь пуста
              </TableCell>
            </TableRow>
          )}
        </TableBody>
      </Table>
    </div>
  )
}
