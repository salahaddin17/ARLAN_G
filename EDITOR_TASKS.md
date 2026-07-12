# EDITOR_TASKS — твои шаги (Windows, по порядку)

Каждый пункт — 1-2 действия. Если что-то пошло не так — фиксируй текст
ошибки и неси в следующую сессию, продолжим по PROGRESS.md.

## 0. Установка (однократно, ~1-2 часа скачивания)

1. Epic Games Launcher → Unreal Engine → Library → `+` → **UE 5.8.x (последняя)** → Install
   (галочки по умолчанию; путь `C:\Program Files\Epic Games\UE_5.8`).
2. **Visual Studio 2022 Community**: winget или сайт MS. В установщике отметить
   workload **«Разработка игр на C++»** (Game development with C++), внутри —
   галочку «Unreal Engine installer» (если есть).
3. Git и LFS (PowerShell):
   `winget install Git.Git GitHub.GitLFS` → новая консоль → `git lfs install`.

## 1. Получить код

4. Если у GitHub-приложения Claude появился доступ на запись — просто
   `git clone https://github.com/salahaddin17/ARLAN_G -b claude/sharp-cerf-2536nr`.
   Если ветки на GitHub нет — возьми присланный `rubezh-arlan-ue.bundle`:
   `git clone rubezh-arlan-ue.bundle ARLAN_G && cd ARLAN_G && git checkout claude/sharp-cerf-2536nr`.

## 2. Собрать проект

5. ПКМ по `RubezhArlan.uproject` → **Generate Visual Studio project files**.
   (Если пункта нет: панель Epic Launcher → UE 5.8 → Options → включён ли
   «Editor symbols»? Не обязательно. Пункт даёт установка UE.)
6. Сборка из командной строки (из папки проекта, cmd):
   ```bat
   "C:\Program Files\Epic Games\UE_5.8\Engine\Build\BatchFiles\Build.bat" RubezhArlanEditor Win64 Development -project="%CD%\RubezhArlan.uproject" -waitmutex
   ```
   Ошибки компиляции? Скопируй ЦЕЛИКОМ первый `error ...` в чат следующей
   сессии — псевдосборка ловит не всё, это ожидаемо.

## 3. Первый запуск

7. Двойной клик `RubezhArlan.uproject`. На вопрос «Rebuild modules?» — **Yes**.
8. Редактор откроется на пустом уровне TestMap может отсутствовать — создаём:
   **Window → Output Log**, внизу смени выпадашку `Cmd` на `Python`, вставь:
   `py "Content/Python/setup_test_map.py"` → Enter.
   В логе появится «TestMap готова».
9. **Play** (Alt+P). Сначала СТАРТОВОЕ МЕНЮ: 1/2 — фракция, 3/4/5 —
   сложность, ПРОБЕЛ — начать. После старта: две базы-коробки (teal/orange),
   жетоны юнитов, грузовики поехали к складам, HUD с припасами/энергией,
   миникарта справа внизу.
10. Проверь управление: рамка ЛКМ, ПКМ приказы, A — атака-движение, S — стоп,
    H — к базе, F — вся армия на экране, колесо — зум, Q/W/E/R/T —
    контекстные действия, Ctrl+1..9 — группы, миникарта: ЛКМ — камера
    (можно вести), ПКМ — приказ. После финала R — новая операция.
    ВАЖНО: в PIE Ctrl+цифра может перехватываться редактором — тогда
    запускай Standalone Game (кнопка ▸ рядом с Play → Standalone Game).

## 4. Тесты

11. **Tools → Session Frontend → Automation**, в фильтр вбей `Rubezh`,
    отметь все 4 теста → Start Tests. Все зелёные?
    Либо из cmd:
    ```bat
    "C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "%CD%\RubezhArlan.uproject" -ExecCmds="Automation RunTests Rubezh; Quit" -unattended -nop4 -nosplash -log
    ```

## 5. Опционально

12. DataTable-ассеты для правки баланса в UI:
    `py "Content/Python/import_datatables.py"` (игра и без них читает CSV).
13. Пост-процесс «штабная карта»: инструкция в
    `Content/Materials/StaffMap_PostProcess.hlsl` (черновик, не обязателен).

## Известные шероховатости первого запуска (не баги дня)

- Кольца выделения и трассеры — отладочная графика (DrawDebug*), в
  Shipping-сборке их не будет; это временный визуал дня 1.
- Туман войны пока скрывает врагов, но не затемняет землю (оверлей — след. шаг).
- Кириллица в HUD зависит от глифов встроенного шрифта; если вместо букв
  квадраты — скажи, переключим на Roboto из движка явно.
