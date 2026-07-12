// Локальный запуск «Аниме-фабрики» без Supabase: npm run local
//
// Поднимает всё сразу:
//  1) PGlite (Postgres в WASM) с персистентными данными в local/data/,
//     при первом старте применяет миграцию и demo-seed;
//  2) REST-эмулятор Supabase API (протокол PostgREST) на :54321 —
//     покрывает подмножество, которое использует приложение;
//  3) SVG-плейсхолдеры вместо внешних картинок (/img/...);
//  4) сам Next.js dev-сервер (создаёт .env.local, если его нет);
//  5) автопрогон воркера очереди каждые 20 секунд (локальный «cron»).
//
// Нужен только Node 18+. Данные живут между запусками; сброс: удалить local/data.

import http from 'node:http'
import fs from 'node:fs'
import path from 'node:path'
import { spawn } from 'node:child_process'
import { fileURLToPath } from 'node:url'
import { PGlite } from '@electric-sql/pglite'

const __dirname = path.dirname(fileURLToPath(import.meta.url))
const ROOT = path.join(__dirname, '..')
const DATA_DIR = path.join(__dirname, 'data')
const API_PORT = 54321
const APP_PORT = Number(process.env.PORT || 3000)
const CRON_SECRET = 'local-demo'

// ─── 1. БД ──────────────────────────────────────────────────────────

const firstRun = !fs.existsSync(DATA_DIR)
const db = new PGlite(DATA_DIR)
await db.waitReady

if (firstRun) {
  console.log('Первый запуск: применяю миграцию и demo-seed…')
  const prelude = `
    do $$ begin create role authenticated; exception when duplicate_object then null; end $$;
    create schema if not exists storage;
    create table if not exists storage.buckets(id text primary key, name text, public boolean);
    create table if not exists storage.objects(id uuid, bucket_id text);
  `
  const migration = fs
    .readFileSync(path.join(ROOT, 'supabase/migrations/0001_init.sql'), 'utf8')
    .replace(/create extension[^;]*;/i, '') // gen_random_uuid встроен в PG13+
  const seed = fs.readFileSync(path.join(ROOT, 'supabase/seed.sql'), 'utf8')
  // внешние картинки недоступны оффлайн — подменяем на локальные SVG
  const localImages = `
    update universes set cover_url = 'http://127.0.0.1:${API_PORT}/img/neon-ronin.svg';
    update character_versions cv set model_sheet_url =
      'http://127.0.0.1:${API_PORT}/img/' ||
      (select c.slug from characters c where c.id = cv.character_id) || '-v' || cv.version_no || '.svg';
    update assets set external_url = 'http://127.0.0.1:${API_PORT}/img/keyframe-' || left(id::text, 8) || '.svg' where kind = 'image';
    update assets set external_url = 'http://127.0.0.1:${API_PORT}/img/video-take-' || left(id::text, 8) || '.svg' where kind = 'video';
  `
  try {
    await db.exec(prelude)
    await db.exec(migration)
    await db.exec(seed)
    await db.exec(localImages)
    console.log('БД готова: вселенная «Неоновый ронин», 6 шотов, очередь, календарь.')
  } catch (e) {
    console.error('Ошибка инициализации БД:', e.message)
    process.exit(1)
  }
}

// ─── 2. REST-эмулятор (подмножество PostgREST) ─────────────────────

// FK-карта: таблица-источник -> (цель или hint) -> fk-колонка источника
const FK = {
  shots: { episodes: 'episode_id', shots_keyframe_asset_fk: 'keyframe_asset_id' },
  episodes: { universes: 'universe_id' },
  generations: { shots: 'shot_id', assets: 'result_asset_id', generations_result_asset_fk: 'result_asset_id' },
  jobs: { generations: 'generation_id' },
  publications: { dist_accounts: 'account_id', episodes: 'episode_id', shots: 'shot_id' },
  character_versions: { characters: 'character_id' },
  characters: { universes: 'universe_id', character_versions: 'current_version_id', characters_current_version_id_fkey: 'current_version_id' },
  assets: { universes: 'universe_id', generations: 'source_generation_id' },
  style_bibles: { universes: 'universe_id' },
  metrics: { publications: 'publication_id' },
}

const JSONB_COLS = new Set([
  'style_bibles.palette', 'style_bibles.camera_rules', 'universes.bible',
  'character_versions.palette', 'character_versions.voice_settings',
  'shots.prompt_vars', 'shots.assembled_prompts', 'generations.params',
  'assets.meta', 'episodes.script',
])

const qi = (s) => '"' + String(s).replace(/"/g, '') + '"'
const lit = (v) => "'" + String(v).replace(/'/g, "''") + "'"

function splitTop(s) {
  const parts = []
  let depth = 0
  let cur = ''
  for (const ch of s) {
    if (ch === '(') depth++
    if (ch === ')') depth--
    if (ch === ',' && depth === 0) {
      parts.push(cur)
      cur = ''
    } else cur += ch
  }
  if (cur) parts.push(cur)
  return parts.map((p) => p.trim()).filter(Boolean)
}

function parseSelect(sel) {
  const cols = []
  const embeds = []
  for (const part of splitTop(sel || '*')) {
    const m = part.match(/^(?:([\w]+):)?([\w]+)(!([\w]+))?\((.*)\)$/s)
    if (m) {
      embeds.push({ alias: m[1] || m[2], target: m[2], hint: m[4] || null, inner: parseSelect(m[5] || '*') })
    } else cols.push(part)
  }
  return { cols: cols.length ? cols : ['*'], embeds }
}

let aliasN = 0
function buildJson(table, alias, parsed) {
  let expr = parsed.cols.includes('*')
    ? `to_jsonb(${alias}.*)`
    : `jsonb_build_object(${parsed.cols.map((c) => `'${c}', ${alias}.${qi(c)}`).join(', ')})`
  for (const e of parsed.embeds) {
    const fkMap = FK[table] || {}
    const fk = fkMap[e.hint || 'none'] || fkMap[e.target] || fkMap[e.alias]
    if (!fk) throw new Error(`no FK: ${table} -> ${e.target} (hint ${e.hint})`)
    const a2 = `e${aliasN++}`
    const sub = `(select ${buildJson(e.target, a2, e.inner)} from ${qi(e.target)} ${a2} where ${a2}.id = ${alias}.${qi(fk)})`
    expr += ` || jsonb_build_object('${e.alias}', ${sub})`
  }
  return expr
}

function parseFilters(table, params) {
  const where = []
  for (const [key, raw] of params) {
    if (['select', 'order', 'limit', 'offset', 'on_conflict', 'columns'].includes(key)) continue
    const dotIdx = raw.indexOf('.')
    const op = raw.slice(0, dotIdx)
    const value = raw.slice(dotIdx + 1)
    if (key.includes('.')) {
      const [ref, col] = key.split('.')
      const fk = (FK[table] || {})[ref]
      if (!fk || op !== 'eq') throw new Error(`unsupported embedded filter ${key}`)
      const a2 = `f${aliasN++}`
      where.push(`exists (select 1 from ${qi(ref)} ${a2} where ${a2}.id = s.${qi(fk)} and ${a2}.${qi(col)} = ${lit(value)})`)
      continue
    }
    const OPS = { eq: '=', neq: '<>', gt: '>', gte: '>=', lt: '<', lte: '<=' }
    if (OPS[op]) where.push(`s.${qi(key)} ${OPS[op]} ${lit(value)}`)
    else if (op === 'in') {
      const vals = value.replace(/^\(|\)$/g, '').split(',').map((v) => v.trim().replace(/^"|"$/g, ''))
      where.push(`s.${qi(key)} in (${vals.map(lit).join(', ')})`)
    } else if (op === 'is') where.push(`s.${qi(key)} is ${value === 'null' ? 'null' : value}`)
    else throw new Error(`unsupported op ${op}`)
  }
  return where
}

function serializeValue(table, col, v) {
  if (v === null || v === undefined) return null
  if (Array.isArray(v)) {
    if (JSONB_COLS.has(`${table}.${col}`)) return JSON.stringify(v)
    return '{' + v.map((x) => '"' + String(x).replace(/"/g, '') + '"').join(',') + '}' // uuid[] и т.п.
  }
  if (typeof v === 'object') return JSON.stringify(v)
  return v
}

async function handleRest(req, res, url, body) {
  const table = url.pathname.replace('/rest/v1/', '')
  const params = url.searchParams
  const wantObject = (req.headers.accept || '').includes('vnd.pgrst.object')
  const prefer = req.headers.prefer || ''

  if (table.startsWith('rpc/')) {
    const fn = table.slice(4)
    const names = Object.keys(body || {})
    const call = names.map((k, i) => `${qi(k)} => $${i + 1}`).join(', ')
    const { rows } = await db.query(`select to_jsonb(t.*) as j from ${qi(fn)}(${call}) t`, names.map((k) => body[k]))
    return sendJson(res, 200, rows.map((r) => r.j))
  }

  aliasN = 0
  const where = parseFilters(table, params)
  const whereSql = where.length ? ' where ' + where.join(' and ') : ''

  if (req.method === 'HEAD') {
    const { rows } = await db.query(`select count(*)::int as n from ${qi(table)} s${whereSql}`)
    res.writeHead(200, { 'Content-Range': `0-${rows[0].n}/${rows[0].n}`, 'Content-Type': 'application/json' })
    return res.end()
  }

  if (req.method === 'GET') {
    const parsed = parseSelect(params.get('select') || '*')
    let sql = `select ${buildJson(table, 's', parsed)} as j from ${qi(table)} s${whereSql}`
    const order = params.get('order')
    if (order) {
      sql += ' order by ' + order.split(',').map((o) => {
        const [col, ...mods] = o.split('.')
        return `s.${qi(col)} ${mods.includes('desc') ? 'desc' : 'asc'}`
      }).join(', ')
    }
    if (params.get('limit')) sql += ` limit ${parseInt(params.get('limit'), 10)}`
    const { rows } = await db.query(sql)
    const data = rows.map((r) => r.j)
    return sendJson(res, 200, wantObject ? data[0] ?? null : data)
  }

  if (req.method === 'POST') {
    const items = Array.isArray(body) ? body : [body]
    const wantRep = prefer.includes('return=representation') || params.get('select')
    const out = []
    for (const item of items) {
      const cols = Object.keys(item)
      const vals = cols.map((c) => serializeValue(table, c, item[c]))
      let sql = `insert into ${qi(table)} as s (${cols.map(qi).join(', ')}) values (${cols.map((_, i) => `$${i + 1}`).join(', ')})`
      if (wantRep) sql += ` returning ${buildJson(table, 's', parseSelect(params.get('select') || '*'))} as j`
      const { rows } = await db.query(sql, vals)
      if (wantRep) out.push(rows[0].j)
    }
    return sendJson(res, 201, wantObject ? out[0] ?? null : out)
  }

  if (req.method === 'PATCH') {
    const cols = Object.keys(body || {})
    const vals = cols.map((c) => serializeValue(table, c, body[c]))
    const wantRep = prefer.includes('return=representation') || params.get('select')
    let sql = `update ${qi(table)} as s set ${cols.map((c, i) => `${qi(c)} = $${i + 1}`).join(', ')}${whereSql}`
    if (wantRep) sql += ` returning ${buildJson(table, 's', parseSelect(params.get('select') || '*'))} as j`
    const { rows } = await db.query(sql, vals)
    const data = wantRep ? rows.map((r) => r.j) : []
    return sendJson(res, 200, wantObject ? data[0] ?? null : data)
  }

  sendJson(res, 405, { message: 'method not supported' })
}

function sendJson(res, code, data) {
  res.writeHead(code, { 'Content-Type': 'application/json' })
  res.end(JSON.stringify(data))
}

// ─── 3. SVG-плейсхолдеры ────────────────────────────────────────────

function hash(s) {
  let h = 2166136261
  for (let i = 0; i < s.length; i++) {
    h ^= s.charCodeAt(i)
    h = Math.imul(h, 16777619)
  }
  return h >>> 0
}

function svgPlaceholder(seed) {
  const h1 = hash(seed) % 360
  const h2 = (h1 + 60 + (hash(seed + 'x') % 120)) % 360
  return `<svg xmlns="http://www.w3.org/2000/svg" width="1024" height="576">
  <defs><linearGradient id="g" x1="0" y1="0" x2="1" y2="1">
    <stop offset="0%" stop-color="hsl(${h1},70%,22%)"/><stop offset="100%" stop-color="hsl(${h2},80%,45%)"/>
  </linearGradient></defs>
  <rect width="1024" height="576" fill="url(#g)"/>
  <circle cx="${200 + (hash(seed + 'c') % 600)}" cy="${150 + (hash(seed + 'd') % 250)}" r="120" fill="hsl(${h2},90%,70%)" opacity="0.25"/>
  <text x="50%" y="52%" font-family="sans-serif" font-size="44" fill="rgba(255,255,255,0.85)" text-anchor="middle">${seed.replace(/[-_]/g, ' ')}</text>
  <text x="50%" y="62%" font-family="sans-serif" font-size="20" fill="rgba(255,255,255,0.5)" text-anchor="middle">local asset</text>
</svg>`
}

// ─── HTTP-сервер API ────────────────────────────────────────────────

http.createServer((req, res) => {
  const url = new URL(req.url, 'http://127.0.0.1')
  if (url.pathname.startsWith('/img/')) {
    res.writeHead(200, { 'Content-Type': 'image/svg+xml', 'Cache-Control': 'public,max-age=3600' })
    return res.end(svgPlaceholder(url.pathname.slice(5).replace(/\.svg$/, '')))
  }
  if (url.pathname.startsWith('/auth/')) return sendJson(res, 200, { user: null })

  let body = ''
  req.on('data', (c) => (body += c))
  req.on('end', async () => {
    try {
      if (url.pathname.startsWith('/rest/v1/')) return await handleRest(req, res, url, body ? JSON.parse(body) : null)
      sendJson(res, 404, { message: 'not found' })
    } catch (e) {
      console.error('[api]', req.method, req.url, '->', e.message)
      sendJson(res, 400, { message: e.message, code: 'LOCAL' })
    }
  })
}).listen(API_PORT, '127.0.0.1', () => console.log(`Локальный Supabase-API: http://127.0.0.1:${API_PORT}`))

// ─── 4. .env.local + Next.js ────────────────────────────────────────

const envPath = path.join(ROOT, '.env.local')
if (!fs.existsSync(envPath)) {
  fs.writeFileSync(envPath, [
    `NEXT_PUBLIC_SUPABASE_URL=http://127.0.0.1:${API_PORT}`,
    'NEXT_PUBLIC_SUPABASE_ANON_KEY=local-demo',
    'SUPABASE_SERVICE_ROLE_KEY=local-demo',
    `CRON_SECRET=${CRON_SECRET}`,
    'DEMO_MODE=1',
    '',
  ].join('\n'))
  console.log('Создан .env.local (демо-режим).')
}

const isWin = process.platform === 'win32'
const app = spawn(isWin ? 'npm.cmd' : 'npm', ['run', 'dev', '--', '-p', String(APP_PORT)], {
  cwd: ROOT,
  stdio: 'inherit',
  shell: isWin,
})
app.on('exit', (code) => process.exit(code ?? 0))

// ─── 5. локальный «cron»: прогон воркера очереди каждые 20 сек ──────

setInterval(() => {
  fetch(`http://127.0.0.1:${APP_PORT}/api/cron/worker`, {
    headers: { authorization: `Bearer ${CRON_SECRET}` },
  }).catch(() => {})
}, 20_000)

console.log(`Приложение: http://localhost:${APP_PORT} (первая загрузка страницы — до ~20 сек, dev-компиляция)`)
