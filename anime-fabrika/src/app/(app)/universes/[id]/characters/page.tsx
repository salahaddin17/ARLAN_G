import Link from 'next/link'
import { notFound } from 'next/navigation'
import { ArrowLeft } from 'lucide-react'
import { createClient } from '@/lib/supabase/server'
import { CharacterVault } from '@/components/character-vault'
import { Button } from '@/components/ui/button'
import type { Character, CharacterVersion, Universe } from '@/lib/types'

export const dynamic = 'force-dynamic'

export default async function CharactersPage({ params }: { params: { id: string } }) {
  const supabase = createClient()

  const [{ data: universe }, { data: characters }, { data: versions }] = await Promise.all([
    supabase.from('universes').select('*').eq('id', params.id).single<Universe>(),
    supabase
      .from('characters')
      .select('*')
      .eq('universe_id', params.id)
      .order('created_at')
      .returns<Character[]>(),
    supabase
      .from('character_versions')
      .select('*, characters!inner(universe_id)')
      .eq('characters.universe_id', params.id)
      .order('version_no', { ascending: false })
      .returns<(CharacterVersion & { characters: { universe_id: string } })[]>(),
  ])

  if (!universe) notFound()

  return (
    <div className="space-y-6">
      <div className="flex items-center gap-3">
        <Button asChild variant="ghost" size="icon">
          <Link href={`/universes/${universe.id}`}>
            <ArrowLeft className="h-4 w-4" />
          </Link>
        </Button>
        <h1 className="text-2xl font-semibold">Character Vault · {universe.name}</h1>
      </div>

      <CharacterVault
        universeId={universe.id}
        characters={characters ?? []}
        versions={(versions ?? []).map(({ characters: _c, ...v }) => v)}
      />
    </div>
  )
}
