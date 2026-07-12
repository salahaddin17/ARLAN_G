-- Аниме-фабрика: начальная схема
-- Применение: supabase db push  (или вставить в SQL Editor дашборда Supabase)

create extension if not exists "pgcrypto";

-- ---------- Enum-типы ----------

create type shot_status as enum
  ('draft', 'keyframe_approved', 'rendering', 'qc', 'approved', 'published');

create type episode_status as enum
  ('writing', 'storyboard', 'production', 'done');

create type generation_kind as enum ('keyframe', 'video', 'audio', 'voice');

create type generation_status as enum
  ('queued', 'running', 'succeeded', 'failed', 'canceled');

create type job_status as enum ('pending', 'processing', 'done', 'failed');

create type publication_status as enum
  ('planned', 'scheduled', 'published', 'failed');

create type provider_name as enum
  ('mock', 'fal', 'kling', 'seedance', 'veo', 'elevenlabs');

create type asset_kind as enum ('image', 'video', 'audio');

create type pub_format as enum ('shorts', 'reel', 'full');

create type platform_name as enum ('youtube', 'tiktok', 'instagram', 'vk');

-- ---------- Таблицы ----------

create table universes (
  id          uuid primary key default gen_random_uuid(),
  name        text not null,
  slug        text not null unique,
  description text not null default '',
  cover_url   text,
  created_at  timestamptz not null default now(),
  updated_at  timestamptz not null default now()
);

create table style_bibles (
  id                      uuid primary key default gen_random_uuid(),
  universe_id             uuid not null references universes(id) on delete cascade,
  version                 int not null default 1,
  is_active               boolean not null default true,
  art_direction           text not null default '',   -- style lock: базовое описание стиля
  palette                 jsonb not null default '[]', -- [{name, hex}]
  camera_rules            jsonb not null default '[]', -- ["правило", ...]
  negative_prompt         text not null default '',
  prompt_template_keyframe text not null default '',  -- шаблон с {{переменными}}
  prompt_template_video    text not null default '',
  created_at              timestamptz not null default now(),
  updated_at              timestamptz not null default now()
);

-- одна активная библия на вселенную
create unique index style_bibles_one_active
  on style_bibles (universe_id) where is_active;

create table characters (
  id             uuid primary key default gen_random_uuid(),
  universe_id    uuid not null references universes(id) on delete cascade,
  name           text not null,
  slug           text not null,
  role           text not null default '',
  description    text not null default '',
  voice_provider provider_name not null default 'elevenlabs',
  voice_id       text not null default '',
  created_at     timestamptz not null default now(),
  updated_at     timestamptz not null default now(),
  unique (universe_id, slug)
);

create table character_versions (
  id                   uuid primary key default gen_random_uuid(),
  character_id         uuid not null references characters(id) on delete cascade,
  version_no           int not null,
  model_sheet_url      text,
  palette              jsonb not null default '[]',
  prompt_block         text not null default '', -- consistency-блок, подставляется в {{characters}}
  voice_settings       jsonb not null default '{}',
  changelog            text not null default '',
  created_at           timestamptz not null default now(),
  unique (character_id, version_no)
);

alter table characters
  add column current_version_id uuid references character_versions(id) on delete set null;

create table episodes (
  id                  uuid primary key default gen_random_uuid(),
  universe_id         uuid not null references universes(id) on delete cascade,
  number              int not null,
  title               text not null,
  synopsis            text not null default '',
  script              text not null default '',
  status              episode_status not null default 'writing',
  target_duration_sec int not null default 60,
  created_at          timestamptz not null default now(),
  updated_at          timestamptz not null default now(),
  unique (universe_id, number)
);

create table shots (
  id                uuid primary key default gen_random_uuid(),
  episode_id        uuid not null references episodes(id) on delete cascade,
  order_index       int not null default 0,
  code              text not null,               -- «E01-S03»
  status            shot_status not null default 'draft',
  scene_description text not null default '',
  dialogue          text not null default '',
  camera            text not null default '',
  duration_sec      numeric not null default 5,
  character_ids     uuid[] not null default '{}',
  prompt_vars       jsonb,                       -- снапшот переменных при сборке
  assembled_prompts jsonb not null default '{}', -- {keyframe: "...", video: "..."}
  keyframe_asset_id uuid,                        -- FK ниже (assets создаётся позже)
  selected_generation_id uuid,                   -- FK ниже
  created_at        timestamptz not null default now(),
  updated_at        timestamptz not null default now()
);

create table generations (
  id              uuid primary key default gen_random_uuid(),
  shot_id         uuid not null references shots(id) on delete cascade,
  kind            generation_kind not null,
  provider        provider_name not null default 'mock',
  model_name      text not null,
  status          generation_status not null default 'queued',
  prompt          text not null,
  params          jsonb not null default '{}',
  seed            bigint,
  attempt_no      int not null default 1,
  cost_usd        numeric(10,4) not null default 0,
  is_hit          boolean not null default false, -- выбран лучшим дублем
  result_asset_id uuid,                           -- FK ниже
  error           text,
  started_at      timestamptz,
  finished_at     timestamptz,
  created_at      timestamptz not null default now(),
  updated_at      timestamptz not null default now()
);

create table assets (
  id                   uuid primary key default gen_random_uuid(),
  universe_id          uuid not null references universes(id) on delete cascade,
  kind                 asset_kind not null,
  storage_path         text,          -- путь в Supabase Storage (bucket assets)
  external_url         text,          -- URL мок-результата или внешнего файла
  mime                 text,
  width                int,
  height               int,
  duration_sec         numeric,
  meta                 jsonb not null default '{}',
  source_generation_id uuid references generations(id) on delete set null,
  created_at           timestamptz not null default now()
);

alter table generations
  add constraint generations_result_asset_fk
  foreign key (result_asset_id) references assets(id) on delete set null;

alter table shots
  add constraint shots_keyframe_asset_fk
  foreign key (keyframe_asset_id) references assets(id) on delete set null;

alter table shots
  add constraint shots_selected_generation_fk
  foreign key (selected_generation_id) references generations(id) on delete set null;

create table jobs (
  id            uuid primary key default gen_random_uuid(),
  generation_id uuid not null references generations(id) on delete cascade,
  status        job_status not null default 'pending',
  run_after     timestamptz not null default now(),
  attempts      int not null default 0,
  max_attempts  int not null default 3,
  locked_at     timestamptz,
  last_error    text,
  created_at    timestamptz not null default now(),
  updated_at    timestamptz not null default now()
);

create table dist_accounts (
  id             uuid primary key default gen_random_uuid(),
  platform       platform_name not null,
  handle         text not null,
  default_format pub_format not null default 'shorts',
  created_at     timestamptz not null default now()
);

create table publications (
  id           uuid primary key default gen_random_uuid(),
  episode_id   uuid not null references episodes(id) on delete cascade,
  shot_id      uuid references shots(id) on delete set null,
  account_id   uuid not null references dist_accounts(id) on delete cascade,
  format       pub_format not null default 'shorts',
  title        text not null default '',
  scheduled_at timestamptz not null,
  published_at timestamptz,
  status       publication_status not null default 'planned',
  external_url text,
  created_at   timestamptz not null default now(),
  updated_at   timestamptz not null default now()
);

create table metrics (
  id             uuid primary key default gen_random_uuid(),
  publication_id uuid not null references publications(id) on delete cascade,
  collected_at   timestamptz not null default now(),
  views          int not null default 0,
  likes          int not null default 0,
  comments       int not null default 0,
  shares         int not null default 0,
  watch_time_sec int not null default 0
);

-- ---------- Индексы ----------

create index shots_episode_idx        on shots (episode_id, order_index);
create index shots_status_idx         on shots (status);
create index generations_shot_idx     on generations (shot_id, created_at);
create index generations_status_idx   on generations (status);
create index generations_created_idx  on generations (created_at);
create index jobs_pending_idx         on jobs (status, run_after);
create index assets_universe_idx      on assets (universe_id);
create index characters_universe_idx  on characters (universe_id);
create index episodes_universe_idx    on episodes (universe_id, number);
create index publications_sched_idx   on publications (scheduled_at);
create index metrics_publication_idx  on metrics (publication_id, collected_at);

-- ---------- updated_at ----------

create or replace function set_updated_at() returns trigger
language plpgsql as $$
begin
  new.updated_at = now();
  return new;
end $$;

do $$
declare t text;
begin
  foreach t in array array[
    'universes','style_bibles','characters','episodes','shots',
    'generations','jobs','publications'
  ] loop
    execute format(
      'create trigger %I_updated_at before update on %I
       for each row execute function set_updated_at()', t, t);
  end loop;
end $$;

-- ---------- Очередь: захват джобов (FOR UPDATE SKIP LOCKED) ----------

create or replace function claim_jobs(batch int default 5)
returns setof jobs
language sql as $$
  update jobs set
    status = 'processing',
    locked_at = now(),
    attempts = attempts + 1,
    updated_at = now()
  where id in (
    select id from jobs
    where status = 'pending' and run_after <= now()
    order by created_at
    limit batch
    for update skip locked
  )
  returning *;
$$;

-- ---------- RLS: полный доступ авторизованным (MVP, одна студия) ----------

do $$
declare t text;
begin
  foreach t in array array[
    'universes','style_bibles','characters','character_versions','episodes',
    'shots','generations','assets','jobs','dist_accounts','publications','metrics'
  ] loop
    execute format('alter table %I enable row level security', t);
    execute format(
      'create policy authenticated_all on %I
       for all to authenticated using (true) with check (true)', t);
  end loop;
end $$;

-- ---------- Storage bucket для ассетов ----------

insert into storage.buckets (id, name, public)
values ('assets', 'assets', true)
on conflict (id) do nothing;

create policy "authenticated write assets" on storage.objects
  for insert to authenticated with check (bucket_id = 'assets');

create policy "public read assets" on storage.objects
  for select using (bucket_id = 'assets');
