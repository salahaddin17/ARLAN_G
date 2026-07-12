import { format, startOfDay, subDays } from 'date-fns'
import { ru } from 'date-fns/locale'
import { createClient } from '@/lib/supabase/server'
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
import {
  SHOT_STATUS_LABELS,
  SHOT_STATUS_ORDER,
  type ShotStatus,
} from '@/lib/types'

export const dynamic = 'force-dynamic'

export default async function DashboardPage() {
  const supabase = createClient()
  const weekAgo = subDays(startOfDay(new Date()), 6).toISOString()

  const [{ data: shots }, { data: generations }] = await Promise.all([
    supabase.from('shots').select('status'),
    supabase
      .from('generations')
      .select('created_at, cost_usd, status, provider, model_name, is_hit'),
  ])

  // Шоты по статусам
  const statusCounts = new Map<ShotStatus, number>()
  for (const s of shots ?? []) {
    const st = s.status as ShotStatus
    statusCounts.set(st, (statusCounts.get(st) ?? 0) + 1)
  }

  // Себестоимость по дням за неделю
  const weekGens = (generations ?? []).filter((g) => g.created_at >= weekAgo)
  const weekCost = weekGens.reduce((sum, g) => sum + Number(g.cost_usd), 0)
  const days = Array.from({ length: 7 }, (_, i) => {
    const day = subDays(startOfDay(new Date()), 6 - i)
    const next = subDays(startOfDay(new Date()), 5 - i)
    const total = weekGens
      .filter((g) => g.created_at >= day.toISOString() && g.created_at < next.toISOString())
      .reduce((sum, g) => sum + Number(g.cost_usd), 0)
    return { label: format(day, 'EEEEEE dd.MM', { locale: ru }), total }
  })
  const maxDay = Math.max(...days.map((d) => d.total), 0.0001)

  // Hit-rate моделей: hit / succeeded за всё время
  const byModel = new Map<
    string,
    { provider: string; total: number; succeeded: number; hits: number; cost: number }
  >()
  for (const g of generations ?? []) {
    const key = g.model_name as string
    const row = byModel.get(key) ?? {
      provider: g.provider as string,
      total: 0,
      succeeded: 0,
      hits: 0,
      cost: 0,
    }
    row.total++
    if (g.status === 'succeeded') row.succeeded++
    if (g.is_hit) row.hits++
    row.cost += Number(g.cost_usd)
    byModel.set(key, row)
  }

  return (
    <div className="space-y-6">
      <h1 className="text-2xl font-semibold">Производственная сводка</h1>

      <div className="grid grid-cols-2 gap-3 md:grid-cols-3 lg:grid-cols-6">
        {SHOT_STATUS_ORDER.map((status) => (
          <Card key={status}>
            <CardHeader className="p-4 pb-1">
              <CardTitle className="text-xs font-normal text-muted-foreground">
                {SHOT_STATUS_LABELS[status]}
              </CardTitle>
            </CardHeader>
            <CardContent className="p-4 pt-0">
              <span className="text-2xl font-semibold">{statusCounts.get(status) ?? 0}</span>
            </CardContent>
          </Card>
        ))}
      </div>

      <div className="grid gap-6 lg:grid-cols-2">
        <Card>
          <CardHeader>
            <CardTitle className="text-base">
              Себестоимость за неделю: {formatUsd(weekCost)}
            </CardTitle>
          </CardHeader>
          <CardContent>
            <div className="flex h-40 items-end gap-2">
              {days.map((d) => (
                <div key={d.label} className="flex flex-1 flex-col items-center gap-1">
                  <span className="text-[10px] text-muted-foreground">
                    {d.total > 0 ? formatUsd(d.total) : ''}
                  </span>
                  <div
                    className="w-full rounded-t bg-primary/70"
                    style={{ height: `${Math.max((d.total / maxDay) * 100, 2)}%` }}
                  />
                  <span className="text-[10px] text-muted-foreground">{d.label}</span>
                </div>
              ))}
            </div>
          </CardContent>
        </Card>

        <Card>
          <CardHeader>
            <CardTitle className="text-base">Hit-rate моделей</CardTitle>
          </CardHeader>
          <CardContent>
            <Table>
              <TableHeader>
                <TableRow>
                  <TableHead>Модель</TableHead>
                  <TableHead className="text-right">Генераций</TableHead>
                  <TableHead className="text-right">Успешных</TableHead>
                  <TableHead className="text-right">Hit-rate</TableHead>
                  <TableHead className="text-right">Стоимость</TableHead>
                </TableRow>
              </TableHeader>
              <TableBody>
                {Array.from(byModel.entries()).map(([model, row]) => (
                  <TableRow key={model}>
                    <TableCell>
                      <span className="text-muted-foreground">{row.provider}</span> · {model}
                    </TableCell>
                    <TableCell className="text-right">{row.total}</TableCell>
                    <TableCell className="text-right">{row.succeeded}</TableCell>
                    <TableCell className="text-right">
                      {row.succeeded > 0
                        ? `${Math.round((row.hits / row.succeeded) * 100)}%`
                        : '—'}
                    </TableCell>
                    <TableCell className="text-right">{formatUsd(row.cost)}</TableCell>
                  </TableRow>
                ))}
                {byModel.size === 0 && (
                  <TableRow>
                    <TableCell colSpan={5} className="text-center text-muted-foreground">
                      Генераций пока нет
                    </TableCell>
                  </TableRow>
                )}
              </TableBody>
            </Table>
          </CardContent>
        </Card>
      </div>
    </div>
  )
}
