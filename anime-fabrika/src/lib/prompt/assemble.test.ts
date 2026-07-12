import { describe, expect, it } from 'vitest'
import {
  assemblePrompt,
  buildCharactersBlock,
  buildShotVars,
  formatPalette,
} from './assemble'

describe('assemblePrompt', () => {
  it('подставляет переменные в плейсхолдеры', () => {
    const { prompt, missing } = assemblePrompt('{{style_lock}}, scene: {{scene}}', {
      style_lock: 'cel anime',
      scene: 'rooftop rain',
    })
    expect(prompt).toBe('cel anime, scene: rooftop rain')
    expect(missing).toEqual([])
  })

  it('терпит пробелы внутри скобок', () => {
    const { prompt } = assemblePrompt('A {{ x }} B', { x: '1' })
    expect(prompt).toBe('A 1 B')
  })

  it('собирает список отсутствующих переменных и чистит пробелы', () => {
    const { prompt, missing } = assemblePrompt('{{a}} mid {{b}} end', { a: 'A' })
    expect(missing).toEqual(['b'])
    expect(prompt).toBe('A mid end')
  })

  it('пустая строка считается отсутствующим значением', () => {
    const { missing } = assemblePrompt('{{scene}}', { scene: '' })
    expect(missing).toEqual(['scene'])
  })

  it('убирает пробел перед знаками препинания после пустой подстановки', () => {
    const { prompt } = assemblePrompt('cinematic {{extra}}, hero shot', {})
    expect(prompt).toBe('cinematic, hero shot')
  })
})

describe('buildCharactersBlock', () => {
  it('использует prompt_block версии, иначе имя персонажа', () => {
    const block = buildCharactersBlock([
      {
        character: { id: '1', name: 'Кай' },
        version: { prompt_block: 'Kai: ronin, cyan katana, face ref KAI-V2' },
      },
      { character: { id: '2', name: 'Юки' }, version: null },
    ])
    expect(block).toBe('Kai: ronin, cyan katana, face ref KAI-V2; Юки')
  })
})

describe('buildShotVars', () => {
  const bible = {
    art_direction: '90s cel anime',
    palette: [{ name: 'циан', hex: '#22d3ee' }],
    negative_prompt: '3d render',
    camera_rules: ['без резких зумов'],
  }
  const shot = {
    scene_description: 'rooftop in rain',
    dialogue: 'Верни катану',
    camera: 'wide shot',
    duration_sec: 6,
  }

  it('собирает полный словарь переменных', () => {
    const vars = buildShotVars(bible, [], shot)
    expect(vars.style_lock).toBe('90s cel anime')
    expect(vars.palette).toBe('циан #22d3ee')
    expect(vars.negative).toBe('3d render')
    expect(vars.scene).toBe('rooftop in rain')
    expect(vars.duration).toBe('6')
  })

  it('интегрируется с assemblePrompt как на проде', () => {
    const vars = buildShotVars(
      bible,
      [{ character: { id: '1', name: 'Кай' }, version: { prompt_block: 'Kai ref KAI-V2' } }],
      shot
    )
    const { prompt, missing } = assemblePrompt(
      '{{style_lock}}, {{characters}}, scene: {{scene}}, camera: {{camera}}, palette: {{palette}} --no {{negative}}',
      vars
    )
    expect(missing).toEqual([])
    expect(prompt).toBe(
      '90s cel anime, Kai ref KAI-V2, scene: rooftop in rain, camera: wide shot, palette: циан #22d3ee --no 3d render'
    )
  })
})

describe('formatPalette', () => {
  it('форматирует пары имя+hex', () => {
    expect(
      formatPalette([
        { name: 'a', hex: '#111111' },
        { name: 'b', hex: '#222222' },
      ])
    ).toBe('a #111111, b #222222')
  })
})
