'use client'

import { useState } from 'react'
import { Mic, Plus } from 'lucide-react'
import { addCharacterVersion, createCharacter, setCurrentVersion } from '@/actions/characters'
import { Badge } from '@/components/ui/badge'
import { Button } from '@/components/ui/button'
import { Card, CardContent, CardHeader, CardTitle } from '@/components/ui/card'
import {
  Dialog,
  DialogContent,
  DialogDescription,
  DialogHeader,
  DialogTitle,
  DialogTrigger,
} from '@/components/ui/dialog'
import { Input } from '@/components/ui/input'
import { Label } from '@/components/ui/label'
import { Textarea } from '@/components/ui/textarea'
import type { Character, CharacterVersion, PaletteEntry } from '@/lib/types'

interface Props {
  universeId: string
  characters: Character[]
  versions: CharacterVersion[]
}

export function CharacterVault({ universeId, characters, versions }: Props) {
  return (
    <div className="grid gap-4 md:grid-cols-2 xl:grid-cols-3">
      {characters.map((c) => (
        <CharacterCard
          key={c.id}
          character={c}
          versions={versions.filter((v) => v.character_id === c.id)}
        />
      ))}
      <NewCharacterCard universeId={universeId} />
    </div>
  )
}

function CharacterCard({
  character,
  versions,
}: {
  character: Character
  versions: CharacterVersion[]
}) {
  const current =
    versions.find((v) => v.id === character.current_version_id) ?? versions[0] ?? null

  return (
    <Card>
      {current?.model_sheet_url && (
        // eslint-disable-next-line @next/next/no-img-element
        <img
          src={current.model_sheet_url}
          alt={`Model sheet: ${character.name}`}
          className="aspect-video w-full rounded-t-xl object-cover"
        />
      )}
      <CardHeader className="pb-2">
        <div className="flex items-center justify-between">
          <CardTitle className="text-base">{character.name}</CardTitle>
          <Badge variant="info">v{current?.version_no ?? '—'}</Badge>
        </div>
        <p className="text-xs text-muted-foreground">{character.role}</p>
      </CardHeader>
      <CardContent className="space-y-3">
        <p className="line-clamp-2 text-sm text-muted-foreground">{character.description}</p>

        {current && (
          <div className="rounded-md bg-muted p-2">
            <p className="mb-1 text-[10px] uppercase tracking-wide text-muted-foreground">
              Промпт-блок (style lock / consistency)
            </p>
            <p className="font-mono text-xs">{current.prompt_block || '—'}</p>
          </div>
        )}

        <div className="flex items-center gap-2 text-xs text-muted-foreground">
          {current && (current.palette as PaletteEntry[]).length > 0 && (
            <span className="flex gap-1">
              {(current.palette as PaletteEntry[]).map((p) => (
                <span
                  key={p.hex}
                  title={`${p.name} ${p.hex}`}
                  className="h-4 w-4 rounded border"
                  style={{ backgroundColor: p.hex }}
                />
              ))}
            </span>
          )}
          {character.voice_id && (
            <span className="ml-auto flex items-center gap-1">
              <Mic className="h-3 w-3" /> {character.voice_id}
            </span>
          )}
        </div>

        <VersionsDialog character={character} versions={versions} />
      </CardContent>
    </Card>
  )
}

function VersionsDialog({
  character,
  versions,
}: {
  character: Character
  versions: CharacterVersion[]
}) {
  const [open, setOpen] = useState(false)

  return (
    <Dialog open={open} onOpenChange={setOpen}>
      <DialogTrigger asChild>
        <Button variant="outline" size="sm" className="w-full">
          Версии ({versions.length})
        </Button>
      </DialogTrigger>
      <DialogContent className="max-w-xl">
        <DialogHeader>
          <DialogTitle>{character.name}: версии</DialogTitle>
          <DialogDescription>
            Текущая версия подставляется в переменную {'{{characters}}'} при сборке промпта.
          </DialogDescription>
        </DialogHeader>

        <div className="space-y-2">
          {versions.map((v) => (
            <div key={v.id} className="flex items-start gap-3 rounded-md border p-3">
              <Badge variant={v.id === character.current_version_id ? 'default' : 'secondary'}>
                v{v.version_no}
              </Badge>
              <div className="min-w-0 flex-1">
                <p className="font-mono text-xs">{v.prompt_block}</p>
                {v.changelog && (
                  <p className="mt-1 text-xs text-muted-foreground">{v.changelog}</p>
                )}
              </div>
              {v.id !== character.current_version_id && (
                <Button
                  variant="ghost"
                  size="sm"
                  onClick={async () => {
                    await setCurrentVersion(character.id, v.id)
                  }}
                >
                  Сделать текущей
                </Button>
              )}
            </div>
          ))}
        </div>

        <form
          action={async (fd) => {
            await addCharacterVersion(character.id, fd)
            setOpen(false)
          }}
          className="space-y-2 border-t pt-3"
        >
          <Label>Новая версия</Label>
          <Textarea
            name="prompt_block"
            placeholder="Промпт-блок (consistency): Kai: young male ronin, ..."
            rows={2}
            required
            className="font-mono text-xs"
          />
          <Input name="model_sheet_url" placeholder="URL model sheet (необязательно)" />
          <Input name="changelog" placeholder="Что изменилось" />
          <Button type="submit" size="sm">
            Добавить версию
          </Button>
        </form>
      </DialogContent>
    </Dialog>
  )
}

function NewCharacterCard({ universeId }: { universeId: string }) {
  const [open, setOpen] = useState(false)

  return (
    <Dialog open={open} onOpenChange={setOpen}>
      <DialogTrigger asChild>
        <button className="flex min-h-40 items-center justify-center rounded-xl border border-dashed text-muted-foreground transition-colors hover:border-primary/50 hover:text-foreground">
          <Plus className="mr-2 h-4 w-4" /> Новый персонаж
        </button>
      </DialogTrigger>
      <DialogContent>
        <DialogHeader>
          <DialogTitle>Новый персонаж</DialogTitle>
        </DialogHeader>
        <form
          action={async (fd) => {
            await createCharacter(universeId, fd)
            setOpen(false)
          }}
          className="space-y-3"
        >
          <Input name="name" placeholder="Имя" required />
          <Input name="role" placeholder="Роль (протагонист, антагонист…)" />
          <Textarea name="description" placeholder="Описание" rows={2} />
          <Textarea
            name="prompt_block"
            placeholder="Промпт-блок v1 (consistency-фраза для генераций)"
            rows={2}
            className="font-mono text-xs"
          />
          <Input name="voice_id" placeholder="Voice ID (ElevenLabs)" />
          <Button type="submit">Создать</Button>
        </form>
      </DialogContent>
    </Dialog>
  )
}
