import { NextResponse, type NextRequest } from 'next/server'
import { processJobs } from '@/lib/queue/worker'

export const dynamic = 'force-dynamic'

// Точка входа cron-воркера. Вызывать раз в минуту:
//   GET /api/cron/worker  с заголовком  Authorization: Bearer $CRON_SECRET
// (Vercel Cron подставляет заголовок сам через crons в vercel.json)
export async function GET(request: NextRequest) {
  const secret = process.env.CRON_SECRET
  const auth = request.headers.get('authorization')
  if (!secret || auth !== `Bearer ${secret}`) {
    return NextResponse.json({ error: 'unauthorized' }, { status: 401 })
  }

  const summary = await processJobs(10)
  return NextResponse.json(summary)
}
