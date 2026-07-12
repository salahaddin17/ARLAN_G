# RUBEZH: ARLAN — UE5 RTS (контекст проекта)

Top-down RTS «РУБЕЖ: АРЛАН» на Unreal Engine 5.8+, стиль «штабная карта».
Target-файлы используют BuildSettingsVersion.Latest — жёсткой привязки
к минорной версии движка нет.
Разработчик — соло-новичок в UE; Claude — инженерный отдел: пишет код,
человек кликает в редакторе по инструкциям из `EDITOR_TASKS.md`.

## Правда о среде

- Облачная Claude-сессия — Linux без UE/VS: настоящая сборка UBT возможна
  только на машине пользователя (Windows, UE 5.8 + VS 2022 C++ workload).
- Здесь работает «псевдосборка»: `Tools/check.sh` компилирует модуль clang++
  с мини-шимом UE-заголовков (`Tools/shim/`). Она ловит ошибки C++ в нашем
  коде, но НЕ гарантирует совместимость с настоящими API UE — финальная
  правда только у UnrealBuildTool.
- Чистая логика игры вынесена в `Source/RubezhArlan/Core/RTSBalanceCore.h`
  (ноль UE-зависимостей) и покрыта тестами, которые бегут прямо здесь:
  `Tools/run_logic_tests.sh`.

## Команды (машина пользователя, Windows)

```bat
:: сборка редактора
"C:\Program Files\Epic Games\UE_5.8\Engine\Build\BatchFiles\Build.bat" ^
  RubezhArlanEditor Win64 Development -project="%CD%\RubezhArlan.uproject" -waitmutex

:: запуск редактора
"C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe" ^
  "%CD%\RubezhArlan.uproject"

:: Automation-тесты из командной строки
"C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" ^
  "%CD%\RubezhArlan.uproject" -ExecCmds="Automation RunTests Rubezh; Quit" ^
  -unattended -nop4 -nosplash -log
```

Псевдосборка и логические тесты в Claude-сессии:

```bash
Tools/check.sh            # clang-компиляция всего модуля через шим
Tools/run_logic_tests.sh  # тесты чистого ядра (матрица урона, экономика)
```

## Игра (краткий дизайн)

- Фракции: «Легион» (teal #0e8f83, +15% цена/HP/урон) и «Вольный фронт»
  (orange #d95f18, −15% цена, −10% HP, +10% скорость).
- Экономика: склады припасов → грузовики → командный центр. Энергостанции;
  дефицит энергии = производство ×0.5.
- Здания: КЦ, энергостанция, казармы, завод техники, турель. Строит техник.
- Юниты: техник, грузовик, пехотинец, ракетчик, разведмашина, танк,
  артиллерия (сплеш). Матрица урона bullet/rocket/shell/defense ×
  INF/LGT/HVY/BLD — вся в таблицах.
- Туман войны двухуровневый; движение NavMesh; ИИ с билд-ордером и волнами,
  3 сложности; победа — снос всех зданий врага.
- Эталон логики и чисел: `prototype-web/` (рабочий Canvas-прототип,
  `npm run dev` внутри папки; там же vitest-тесты баланса).

## Архитектура модуля `Source/RubezhArlan/`

```
Core/    RTSBalanceCore.h (чистое ядро), RTSGameMode, RTSPlayerController,
         RTSCameraPawn, RTSHUD
Data/    RTSTypes.h (enum'ы, FRTSOrder), RTSDataTypes.h (строки DataTable),
         RTSDataSubsystem (CSV → таблицы в рантайме)
Units/   UnitBase (ACharacter), RTSAIController (исполнение приказов)
Buildings/ BuildingBase (+ турель), SupplyDepot
Components/ HealthComponent, WeaponComponent, ProductionComponent
Econ/    EconomySubsystem (припасы, энергия, статистика, победа)
Combat/  RTSProjectile (артиллерия, сплеш)
Fog/     FogOfWarSubsystem (сетка 64×64, 2 уровня)
AI/      EnemyCommander (state machine противника)
Tests/   Automation-тесты (зеркалят Tools/logic_tests.cpp)
```

Принципы: вся логика в C++, Blueprint — только тонкие обёртки при
необходимости; баланс ТОЛЬКО в `Content/Data/*.csv` + fallback-константы
ядра (кросс-проверяются тестом); никаких таймеров UE в геймлогике —
тик-аккумуляторы (детерминизм и простота портирования).

## Рабочий процесс

- Ветка: `claude/sharp-cerf-2536nr`. Мелкие коммиты после каждого блока.
- `PROGRESS.md` — сделано/дальше/проблемы (обновлять каждый блок).
- `EDITOR_TASKS.md` — шаги пользователя в редакторе, по 1-2 строки, для новичка.
- Push работает (после Unsuspend установки GitHub-приложения). CI на
  GitHub Actions гоняет псевдосборку и все тесты на каждый push.
