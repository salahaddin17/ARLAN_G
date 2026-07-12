// Сборка промпта из шаблона style_bible + данных персонажей и шота.
// Чистые функции без зависимостей — покрыты vitest (assemble.test.ts).

import type {
  Character,
  CharacterVersion,
  GenerationKind,
  PaletteEntry,
  Shot,
  StyleBible,
} from '@/lib/types'

const VAR_RE = /\{\{\s*([a-zA-Z0-9_]+)\s*\}\}/g

export interface AssembleResult {
  prompt: string
  /** Плейсхолдеры шаблона, для которых не нашлось значения */
  missing: string[]
}

/** Подставляет vars в {{плейсхолдеры}} шаблона, чистит лишние пробелы. */
export function assemblePrompt(
  template: string,
  vars: Record<string, string>
): AssembleResult {
  const missing = new Set<string>()
  const prompt = template
    .replace(VAR_RE, (_m, key: string) => {
      const value = vars[key]
      if (value === undefined || value === null || value === '') {
        missing.add(key)
        return ''
      }
      return value
    })
    .replace(/\s{2,}/g, ' ')
    .replace(/\s+([,.;])/g, '$1')
    .trim()
  return { prompt, missing: Array.from(missing) }
}

export function formatPalette(palette: PaletteEntry[]): string {
  return palette.map((p) => `${p.name} ${p.hex}`).join(', ')
}

export interface CharacterWithVersion {
  character: Pick<Character, 'id' | 'name'>
  version: Pick<CharacterVersion, 'prompt_block'> | null
}

/** Блок {{characters}}: consistency-фразы текущих версий персонажей шота. */
export function buildCharactersBlock(list: CharacterWithVersion[]): string {
  return list
    .map(({ character, version }) =>
      version && version.prompt_block ? version.prompt_block : character.name
    )
    .join('; ')
}

type ShotFields = Pick<
  Shot,
  'scene_description' | 'dialogue' | 'camera' | 'duration_sec'
>
type BibleFields = Pick<
  StyleBible,
  'art_direction' | 'palette' | 'negative_prompt' | 'camera_rules'
>

/** Собирает словарь переменных для шаблонов keyframe/video. */
export function buildShotVars(
  bible: BibleFields,
  characters: CharacterWithVersion[],
  shot: ShotFields
): Record<string, string> {
  return {
    style_lock: bible.art_direction,
    palette: formatPalette(bible.palette ?? []),
    negative: bible.negative_prompt,
    camera_rules: (bible.camera_rules ?? []).join(', '),
    characters: buildCharactersBlock(characters),
    scene: shot.scene_description,
    dialogue: shot.dialogue,
    camera: shot.camera,
    duration: String(shot.duration_sec),
  }
}

export function templateForKind(
  bible: Pick<StyleBible, 'prompt_template_keyframe' | 'prompt_template_video'>,
  kind: GenerationKind
): string {
  return kind === 'keyframe'
    ? bible.prompt_template_keyframe
    : bible.prompt_template_video
}
