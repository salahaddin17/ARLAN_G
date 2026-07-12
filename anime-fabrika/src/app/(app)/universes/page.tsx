import Link from 'next/link'
import { createClient } from '@/lib/supabase/server'
import { createUniverse } from '@/actions/universes'
import { Button } from '@/components/ui/button'
import { Card, CardContent, CardDescription, CardHeader, CardTitle } from '@/components/ui/card'
import { Input } from '@/components/ui/input'
import { Textarea } from '@/components/ui/textarea'
import type { Universe } from '@/lib/types'

export const dynamic = 'force-dynamic'

export default async function UniversesPage() {
  async function createUniverseAction(formData: FormData) {
    'use server'
    await createUniverse(formData)
  }

  const supabase = createClient()
  const { data: universes } = await supabase
    .from('universes')
    .select('*')
    .order('created_at')
    .returns<Universe[]>()

  return (
    <div className="space-y-6">
      <h1 className="text-2xl font-semibold">Вселенные</h1>

      <div className="grid gap-4 md:grid-cols-2 lg:grid-cols-3">
        {(universes ?? []).map((u) => (
          <Link key={u.id} href={`/universes/${u.id}`}>
            <Card className="h-full transition-colors hover:border-primary/50">
              {u.cover_url && (
                // eslint-disable-next-line @next/next/no-img-element
                <img
                  src={u.cover_url}
                  alt={u.name}
                  className="aspect-video w-full rounded-t-xl object-cover"
                />
              )}
              <CardHeader>
                <CardTitle>{u.name}</CardTitle>
                <CardDescription className="line-clamp-2">{u.description}</CardDescription>
              </CardHeader>
            </Card>
          </Link>
        ))}
      </div>

      <Card className="max-w-md">
        <CardHeader>
          <CardTitle className="text-base">Новая вселенная</CardTitle>
        </CardHeader>
        <CardContent>
          <form action={createUniverseAction} className="space-y-3">
            <Input name="name" placeholder="Название" required />
            <Textarea name="description" placeholder="Описание мира" rows={3} />
            <Button type="submit">Создать</Button>
          </form>
        </CardContent>
      </Card>
    </div>
  )
}
