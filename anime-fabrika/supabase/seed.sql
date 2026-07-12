-- Демо-данные: вселенная «Неоновый ронин», 2 персонажа, 1 эпизод, 6 шотов.
-- Применять после 0001_init.sql: supabase db push && psql ... -f supabase/seed.sql
-- (или вставить в SQL Editor). Скрипт идемпотентен по фиксированным UUID.

-- Вселенная
insert into universes (id, name, slug, description, cover_url) values
  ('10000000-0000-4000-8000-000000000001', 'Неоновый ронин', 'neon-ronin',
   'Киберпанк-аниме о бродячем мечнике в мегаполисе Синдзю-2099. Серийный формат: вертикальные эпизоды 60–90 сек.',
   'https://picsum.photos/seed/neon-ronin/800/450')
on conflict (id) do nothing;

-- Библия стиля с шаблонами промптов
insert into style_bibles (id, universe_id, version, is_active, art_direction, palette,
                          camera_rules, negative_prompt,
                          prompt_template_keyframe, prompt_template_video) values
  ('20000000-0000-4000-8000-000000000001', '10000000-0000-4000-8000-000000000001', 1, true,
   '90s cel anime style, film grain, neon-lit cyberpunk megacity, dramatic rim light, 2D flat shading',
   '[{"name":"неон-циан","hex":"#22d3ee"},{"name":"маджента","hex":"#e879f9"},{"name":"глубокий синий","hex":"#1e1b4b"},{"name":"тёплый янтарь","hex":"#f59e0b"}]',
   '["без резких зумов", "движение камеры медленное, параллакс слоёв", "крупные планы на эмоциях"]',
   'photorealistic, 3d render, watermark, text, extra fingers, blurry',
   '{{style_lock}}, anime key visual, {{characters}}, scene: {{scene}}, camera: {{camera}}, color palette: {{palette}}, high detail, clean lineart --no {{negative}}',
   '{{style_lock}}, anime sequence, {{characters}}, action: {{scene}}, camera: {{camera}}, duration {{duration}}s, color palette: {{palette}} --no {{negative}}')
on conflict (id) do nothing;

-- Персонажи
insert into characters (id, universe_id, name, slug, role, description, voice_provider, voice_id) values
  ('30000000-0000-4000-8000-000000000001', '10000000-0000-4000-8000-000000000001',
   'Кай', 'kai', 'протагонист',
   'Бродячий ронин с катаной из светового волокна. Немногословен, ироничен.',
   'elevenlabs', 'voice_kai_v1'),
  ('30000000-0000-4000-8000-000000000002', '10000000-0000-4000-8000-000000000001',
   'Юки', 'yuki', 'дейтерагонист',
   'Хакерша 17 лет, напарница Кая. Быстрая речь, сленг.',
   'elevenlabs', 'voice_yuki_v1')
on conflict (id) do nothing;

insert into character_versions (id, character_id, version_no, model_sheet_url, palette, prompt_block, voice_settings, changelog) values
  ('31000000-0000-4000-8000-000000000001', '30000000-0000-4000-8000-000000000001', 1,
   'https://picsum.photos/seed/kai-sheet-v1/1024/576',
   '[{"name":"плащ","hex":"#1e1b4b"},{"name":"катана","hex":"#22d3ee"}]',
   'Kai: young male ronin, long dark coat, glowing cyan fiber-katana, silver undercut hair, scar over left eyebrow, consistent face ref KAI-V1',
   '{"stability":0.6,"similarity":0.85}', 'Первая версия model sheet'),
  ('31000000-0000-4000-8000-000000000002', '30000000-0000-4000-8000-000000000001', 2,
   'https://picsum.photos/seed/kai-sheet-v2/1024/576',
   '[{"name":"плащ","hex":"#1e1b4b"},{"name":"катана","hex":"#22d3ee"},{"name":"шарф","hex":"#f59e0b"}]',
   'Kai: young male ronin, long dark coat with amber scarf, glowing cyan fiber-katana, silver undercut hair, scar over left eyebrow, consistent face ref KAI-V2',
   '{"stability":0.6,"similarity":0.85}', 'Добавлен янтарный шарф, уточнено лицо'),
  ('31000000-0000-4000-8000-000000000003', '30000000-0000-4000-8000-000000000002', 1,
   'https://picsum.photos/seed/yuki-sheet-v1/1024/576',
   '[{"name":"куртка","hex":"#e879f9"},{"name":"волосы","hex":"#22d3ee"}]',
   'Yuki: 17yo hacker girl, magenta bomber jacket, short cyan hair, AR goggles on forehead, consistent face ref YUKI-V1',
   '{"stability":0.5,"similarity":0.9}', 'Первая версия model sheet')
on conflict (id) do nothing;

update characters set current_version_id = '31000000-0000-4000-8000-000000000002'
  where id = '30000000-0000-4000-8000-000000000001';
update characters set current_version_id = '31000000-0000-4000-8000-000000000003'
  where id = '30000000-0000-4000-8000-000000000002';

-- Эпизод
insert into episodes (id, universe_id, number, title, synopsis, script, status, target_duration_sec) values
  ('40000000-0000-4000-8000-000000000001', '10000000-0000-4000-8000-000000000001', 1,
   'Дождь над Синдзю', 'Кай прибывает в Синдзю-2099 и встречает Юки, взломавшую его катану.',
   'Сцена 1: крыша под дождём... Сцена 2: погоня по неоновому рынку...', 'production', 75)
on conflict (id) do nothing;

-- Шоты по всем статусам
insert into shots (id, episode_id, order_index, code, status, scene_description, dialogue, camera, duration_sec, character_ids) values
  ('50000000-0000-4000-8000-000000000001', '40000000-0000-4000-8000-000000000001', 1, 'E01-S01', 'approved',
   'Кай стоит на краю крыши под неоновым дождём, город внизу в дымке', '', 'wide establishing shot, slow push-in', 6,
   array['30000000-0000-4000-8000-000000000001']::uuid[]),
  ('50000000-0000-4000-8000-000000000002', '40000000-0000-4000-8000-000000000001', 2, 'E01-S02', 'qc',
   'Крупный план: капли на лезвии катаны, отражение неона', '', 'extreme close-up, rack focus', 4,
   array['30000000-0000-4000-8000-000000000001']::uuid[]),
  ('50000000-0000-4000-8000-000000000003', '40000000-0000-4000-8000-000000000001', 3, 'E01-S03', 'rendering',
   'Юки на пожарной лестнице машет Каю, голограммы рекламы мигают', 'Юки: «Эй, самурай! Твоя железка теперь моя!»', 'medium shot, slight dutch angle', 5,
   array['30000000-0000-4000-8000-000000000001','30000000-0000-4000-8000-000000000002']::uuid[]),
  ('50000000-0000-4000-8000-000000000004', '40000000-0000-4000-8000-000000000001', 4, 'E01-S04', 'keyframe_approved',
   'Погоня по неоновому рынку, лотки с лапшой, толпа расступается', '', 'tracking shot, parallax layers', 8,
   array['30000000-0000-4000-8000-000000000001','30000000-0000-4000-8000-000000000002']::uuid[]),
  ('50000000-0000-4000-8000-000000000005', '40000000-0000-4000-8000-000000000001', 5, 'E01-S05', 'draft',
   'Кай и Юки лицом к лицу в тупике, катана гаснет', 'Кай: «Верни. Пока прошу вежливо.»', 'over-the-shoulder, close-up', 6,
   array['30000000-0000-4000-8000-000000000001','30000000-0000-4000-8000-000000000002']::uuid[]),
  ('50000000-0000-4000-8000-000000000006', '40000000-0000-4000-8000-000000000001', 6, 'E01-S06', 'draft',
   'Титульный кадр: логотип эпизода на фоне дождя', '', 'static, title card', 3,
   array[]::uuid[])
on conflict (id) do nothing;

-- Генерации (дубли), стоимость, hit/miss
insert into generations (id, shot_id, kind, provider, model_name, status, prompt, attempt_no, cost_usd, is_hit, error, started_at, finished_at, created_at) values
  ('60000000-0000-4000-8000-000000000001', '50000000-0000-4000-8000-000000000001', 'keyframe', 'fal', 'flux-pro-1.1', 'succeeded',
   'seeded demo prompt (kai rooftop, take 1)', 1, 0.0500, false, null, now() - interval '3 days', now() - interval '3 days', now() - interval '3 days'),
  ('60000000-0000-4000-8000-000000000002', '50000000-0000-4000-8000-000000000001', 'keyframe', 'fal', 'flux-pro-1.1', 'succeeded',
   'seeded demo prompt (kai rooftop, take 2)', 2, 0.0500, true, null, now() - interval '3 days', now() - interval '3 days', now() - interval '3 days'),
  ('60000000-0000-4000-8000-000000000003', '50000000-0000-4000-8000-000000000001', 'video', 'kling', 'kling-2.1-pro', 'succeeded',
   'seeded demo prompt (kai rooftop, video)', 1, 0.5400, true, null, now() - interval '2 days', now() - interval '2 days', now() - interval '2 days'),
  ('60000000-0000-4000-8000-000000000004', '50000000-0000-4000-8000-000000000002', 'video', 'seedance', 'seedance-1.0-pro', 'succeeded',
   'seeded demo prompt (katana close-up, video)', 1, 0.2400, false, null, now() - interval '1 day', now() - interval '1 day', now() - interval '1 day'),
  ('60000000-0000-4000-8000-000000000005', '50000000-0000-4000-8000-000000000003', 'video', 'kling', 'kling-2.1-pro', 'failed',
   'seeded demo prompt (yuki fire escape, video)', 1, 0.0000, false, 'mock: provider timeout', now() - interval '5 hours', now() - interval '5 hours', now() - interval '5 hours'),
  ('60000000-0000-4000-8000-000000000006', '50000000-0000-4000-8000-000000000003', 'video', 'kling', 'kling-2.1-pro', 'queued',
   'seeded demo prompt (yuki fire escape, video, retry)', 2, 0.0000, false, null, null, null, now() - interval '1 hour')
on conflict (id) do nothing;

-- Ассеты (мок-результаты)
insert into assets (id, universe_id, kind, external_url, mime, width, height, duration_sec, source_generation_id) values
  ('70000000-0000-4000-8000-000000000001', '10000000-0000-4000-8000-000000000001', 'image',
   'https://picsum.photos/seed/e01s01-kf1/1024/576', 'image/jpeg', 1024, 576, null, '60000000-0000-4000-8000-000000000001'),
  ('70000000-0000-4000-8000-000000000002', '10000000-0000-4000-8000-000000000001', 'image',
   'https://picsum.photos/seed/e01s01-kf2/1024/576', 'image/jpeg', 1024, 576, null, '60000000-0000-4000-8000-000000000002'),
  ('70000000-0000-4000-8000-000000000003', '10000000-0000-4000-8000-000000000001', 'video',
   'https://interactive-examples.mdn.mozilla.net/media/cc0-videos/flower.mp4', 'video/mp4', 1920, 1080, 6, '60000000-0000-4000-8000-000000000003'),
  ('70000000-0000-4000-8000-000000000004', '10000000-0000-4000-8000-000000000001', 'video',
   'https://interactive-examples.mdn.mozilla.net/media/cc0-videos/flower.mp4', 'video/mp4', 1920, 1080, 4, '60000000-0000-4000-8000-000000000004')
on conflict (id) do nothing;

update generations set result_asset_id = '70000000-0000-4000-8000-000000000001' where id = '60000000-0000-4000-8000-000000000001';
update generations set result_asset_id = '70000000-0000-4000-8000-000000000002' where id = '60000000-0000-4000-8000-000000000002';
update generations set result_asset_id = '70000000-0000-4000-8000-000000000003' where id = '60000000-0000-4000-8000-000000000003';
update generations set result_asset_id = '70000000-0000-4000-8000-000000000004' where id = '60000000-0000-4000-8000-000000000004';

update shots set keyframe_asset_id = '70000000-0000-4000-8000-000000000002',
                 selected_generation_id = '60000000-0000-4000-8000-000000000003'
  where id = '50000000-0000-4000-8000-000000000001';

-- Джобы очереди: один ожидает воркера, один завершён
insert into jobs (id, generation_id, status, run_after, attempts, max_attempts, last_error) values
  ('80000000-0000-4000-8000-000000000001', '60000000-0000-4000-8000-000000000006', 'pending', now(), 0, 3, null),
  ('80000000-0000-4000-8000-000000000002', '60000000-0000-4000-8000-000000000003', 'done', now() - interval '2 days', 1, 3, null)
on conflict (id) do nothing;

-- Аккаунты дистрибуции
insert into dist_accounts (id, platform, handle, default_format) values
  ('90000000-0000-4000-8000-000000000001', 'youtube', '@neonronin', 'shorts'),
  ('90000000-0000-4000-8000-000000000002', 'tiktok', '@neon.ronin', 'shorts')
on conflict (id) do nothing;

-- План публикаций
insert into publications (id, episode_id, shot_id, account_id, format, title, scheduled_at, published_at, status, external_url) values
  ('a1000000-0000-4000-8000-000000000001', '40000000-0000-4000-8000-000000000001', null,
   '90000000-0000-4000-8000-000000000001', 'shorts', 'Неоновый ронин — E01 (тизер)',
   now() - interval '1 day', now() - interval '1 day', 'published', 'https://youtube.com/shorts/demo1'),
  ('a1000000-0000-4000-8000-000000000002', '40000000-0000-4000-8000-000000000001', null,
   '90000000-0000-4000-8000-000000000002', 'shorts', 'Неоновый ронин — E01 (тизер)',
   now() + interval '2 days', null, 'scheduled', null),
  ('a1000000-0000-4000-8000-000000000003', '40000000-0000-4000-8000-000000000001', null,
   '90000000-0000-4000-8000-000000000001', 'full', 'Неоновый ронин — Эпизод 1: Дождь над Синдзю',
   now() + interval '5 days', null, 'planned', null)
on conflict (id) do nothing;

-- Метрики опубликованного
insert into metrics (id, publication_id, collected_at, views, likes, comments, shares, watch_time_sec) values
  ('a2000000-0000-4000-8000-000000000001', 'a1000000-0000-4000-8000-000000000001', now() - interval '12 hours', 1200, 96, 14, 8, 41000),
  ('a2000000-0000-4000-8000-000000000002', 'a1000000-0000-4000-8000-000000000001', now(), 4800, 350, 42, 31, 168000)
on conflict (id) do nothing;
