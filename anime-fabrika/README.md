# Аниме-фабрика

Внутренняя платформа студии серийного производства аниме-контента:
вселенные → персонажи с версиями → эпизоды → шоты (kanban) → очередь
генераций (мок-провайдеры) → календарь дистрибуции.

Стек: **Next.js 14 (App Router) · Supabase (Postgres, Auth, Storage) ·
Tailwind · shadcn/ui · vitest**.

## Запуск за 2 минуты (локально, без Supabase)

Нужен только Node 18+. Postgres встроен (PGlite, WASM), Supabase-аккаунт не нужен:

```bash
npm install
npm run local     # БД + API + приложение + автоворкер очереди
# открыть http://localhost:3000
```

Первый запуск применяет миграцию и demo-seed (вселенная «Неоновый ронин»).
Данные живут в `local/data/` между запусками; полный сброс — удалить эту папку.
Очередь генераций обрабатывается автоматически каждые 20 секунд (мок-провайдеры),
плюс кнопка «Прогнать воркер» на экране очереди. Аутентификация в этом режиме
выключена (`DEMO_MODE=1` в автоматически созданном `.env.local`).

## Запуск на Supabase (прод)

1. Создайте проект на [supabase.com](https://supabase.com), включите Email-аутентификацию
   (Authentication → Providers → Email, magic link включён по умолчанию).
2. Примените схему и демо-данные — два пути:
   - **SQL Editor** в дашборде: вставьте содержимое
     `supabase/migrations/0001_init.sql`, затем `supabase/seed.sql`;
   - **CLI**: `supabase link --project-ref <ref> && supabase db push`,
     затем seed через SQL Editor или `psql`.
3. Скопируйте ключи:
   ```bash
   cp .env.example .env.local
   # заполните NEXT_PUBLIC_SUPABASE_URL, NEXT_PUBLIC_SUPABASE_ANON_KEY,
   # SUPABASE_SERVICE_ROLE_KEY, CRON_SECRET
   ```
4. Установка и запуск:
   ```bash
   npm install
   npm run dev        # http://localhost:3000
   npm test           # vitest: сборка промптов
   npm run typecheck
   ```
5. Войдите по magic-ссылке (любая почта, письмо придёт от Supabase).

## Очередь генераций

- «В очередь» на карточке шота создаёт `generation` + `job`.
- Джобы обрабатывает воркер: `POST`-кнопка «Прогнать воркер» на экране
  очереди (дев-режим) или cron `GET /api/cron/worker` с заголовком
  `Authorization: Bearer $CRON_SECRET` (Vercel Cron настроен в
  `vercel.json`, раз в минуту).
- Провайдеры (fal.ai / Kling / Seedance / Veo / ElevenLabs) сейчас
  замоканы: `src/lib/providers/mock.ts` возвращает placeholder-ассеты,
  реалистичную стоимость и ~15% отказов (для проверки ретраев с backoff).
  Реальный провайдер подключается заменой записи в
  `src/lib/providers/registry.ts` на реализацию интерфейса
  `GenerationAdapter` из `src/lib/providers/adapter.ts`.

## Ключевая механика: промпт не пишется руками

Шаблоны промптов живут в `style_bibles.prompt_template_keyframe/video`
с плейсхолдерами `{{style_lock}} {{characters}} {{scene}} {{camera}}
{{palette}} {{negative}} {{duration}} {{dialogue}}`.

Кнопка **«Собрать промпт»** в карточке шота: активная библия вселенной +
`prompt_block` текущих версий персонажей шота + данные сцены →
`src/lib/prompt/assemble.ts` → превью → **«В очередь»**. Снапшот
переменных и собранных промптов сохраняется в шоте, сам текст промпта
руками не редактируется — только источники (библия, персонаж, шот).

## Статусы шота (kanban)

`draft → keyframe_approved → rendering → qc → approved → published` —
вперёд только на соседний статус, назад можно на любой (доработка).
Постановка видео в очередь автоматически переводит шот в `rendering`,
успешный рендер — в `qc`.

## Структура

```
supabase/            миграция схемы + seed (вселенная «Неоновый ронин»)
src/lib/prompt/      сборка промпта (чистые функции + vitest)
src/lib/providers/   adapter-интерфейс, мок, каталог моделей с ценами
src/lib/queue/       воркер очереди (claim_jobs SKIP LOCKED, backoff)
src/actions/         server actions (шоты, генерации, персонажи, публикации)
src/app/(app)/       dashboard, universes/[id](+characters),
                     episodes/[id]/board, queue, calendar
src/components/      kanban, карточка шота, vault, очередь, календарь, ui/
```
