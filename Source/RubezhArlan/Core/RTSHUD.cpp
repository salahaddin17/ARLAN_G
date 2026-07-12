#include "Core/RTSHUD.h"
#include "RubezhArlan.h"
#include "Core/RTSPlayerController.h"
#include "Core/RTSGameMode.h"
#include "Core/RTSCameraPawn.h"
#include "Units/UnitBase.h"
#include "Buildings/BuildingBase.h"
#include "Components/RTSHealthComponent.h"
#include "Components/RTSProductionComponent.h"
#include "Data/RTSDataSubsystem.h"
#include "Econ/RTSEconomySubsystem.h"
#include "Econ/SupplyDepot.h"
#include "Fog/RTSFogSubsystem.h"
#include "Engine/Canvas.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"

namespace
{
	const FLinearColor ConsoleBg(0.055f, 0.06f, 0.07f, 0.88f);   // #15171c
	const FLinearColor ConsoleLine(0.16f, 0.18f, 0.21f, 1.f);
	const FLinearColor TextMain(0.84f, 0.82f, 0.77f, 1.f);
	const FLinearColor TextMuted(0.55f, 0.54f, 0.49f, 1.f);
	const FLinearColor WarnRed(0.75f, 0.27f, 0.16f, 1.f);
	const FLinearColor OkGreen(0.44f, 0.61f, 0.28f, 1.f);
	const FLinearColor Marquee(0.92f, 0.9f, 0.85f, 0.9f);
}

void ARTSHUD::DrawHUD()
{
	AHUD::DrawHUD();
	ARTSPlayerController* RTSController = Cast<ARTSPlayerController>(PlayerOwner);
	if (!RTSController || !Canvas)
	{
		return;
	}

	const ARTSGameMode* Mode = GetWorld() ? Cast<ARTSGameMode>(GetWorld()->GetAuthGameMode()) : nullptr;
	if (Mode && Mode->GetPhase() == ERTSMatchPhase::Setup)
	{
		DrawSetupMenu();
		return;
	}

	DrawTopBar(RTSController);
	DrawSelectionPanel(RTSController);
	DrawMinimap();
	DrawMarquee(RTSController);
	DrawEndBanner();
}

// --- миникарта: раскладка и преобразования ------------------------------------

FRTSMinimapLayout ARTSHUD::GetMinimapLayout(float ViewX, float ViewY)
{
	FRTSMinimapLayout Layout;
	Layout.Size = 220.f;
	Layout.X = ViewX - Layout.Size - 12.f;
	Layout.Y = ViewY - 120.f - Layout.Size - 10.f; // над нижней панелью
	return Layout;
}

FVector ARTSHUD::MinimapToWorld(const FRTSMinimapLayout& Layout, float PX, float PY)
{
	const float H = RTSCore::MapHalfSize;
	const float U = FMath::Clamp((PX - Layout.X) / Layout.Size, 0.f, 1.f);
	const float V = FMath::Clamp((PY - Layout.Y) / Layout.Size, 0.f, 1.f);
	// экранный «верх» миникарты = мировой +X; «право» = +Y
	return FVector((1.f - V) * 2.f * H - H, U * 2.f * H - H, 0.f);
}

FVector2D ARTSHUD::WorldToMinimap(const FRTSMinimapLayout& Layout, const FVector& World)
{
	const float H = RTSCore::MapHalfSize;
	const float U = (float(World.Y) + H) / (2.f * H);
	const float V = 1.f - (float(World.X) + H) / (2.f * H);
	return FVector2D(Layout.X + U * Layout.Size, Layout.Y + V * Layout.Size);
}

// --- стартовое меню --------------------------------------------------------------

void ARTSHUD::DrawSetupMenu()
{
	UWorld* World = GetWorld();
	URTSDataSubsystem* Data = World && World->GetGameInstance()
		? World->GetGameInstance()->GetSubsystem<URTSDataSubsystem>() : nullptr;
	ARTSGameMode* Mode = World ? Cast<ARTSGameMode>(World->GetAuthGameMode()) : nullptr;
	if (!Data || !Mode)
	{
		return;
	}

	DrawRect(FLinearColor(0.05f, 0.055f, 0.065f, 0.94f), 0.f, 0.f, Canvas->SizeX, Canvas->SizeY);

	const float CX = Canvas->SizeX * 0.5f;
	float Y = Canvas->SizeY * 0.24f;

	const FFactionRow& Legion = Data->GetFaction(ERTSFaction::Legion);
	const FFactionRow& Front = Data->GetFaction(ERTSFaction::Front);

	DrawText(TEXT("Р У Б Е Ж :  А Р Л А Н"), Legion.Color, CX - 260.f, Y, nullptr, 3.f);
	Y += 70.f;
	DrawText(TEXT("тактическая операция · штабная карта"), TextMuted, CX - 150.f, Y);
	Y += 60.f;

	const ERTSFaction Picked = Mode->PlayerFaction;
	DrawText(TEXT("Фракция:"), TextMain, CX - 300.f, Y);
	DrawText(FString::Printf(TEXT("[1] %s — дороже, крепче, больнее"), *Legion.DisplayName),
	         Picked == ERTSFaction::Legion ? Legion.Color : TextMuted, CX - 180.f, Y);
	Y += 28.f;
	DrawText(FString::Printf(TEXT("[2] %s — дешевле и быстрее"), *Front.DisplayName),
	         Picked == ERTSFaction::Front ? Front.Color : TextMuted, CX - 180.f, Y);
	Y += 48.f;

	DrawText(TEXT("Сложность:"), TextMain, CX - 300.f, Y);
	const ERTSDifficulty Diff = Mode->Difficulty;
	const TCHAR* DiffKeys[3] = {TEXT("[3]"), TEXT("[4]"), TEXT("[5]")};
	for (int32 I = 0; I < 3; ++I)
	{
		const FDifficultyRow& Row = Data->GetDifficulty(ERTSDifficulty(I));
		DrawText(FString::Printf(TEXT("%s %s"), DiffKeys[I], *Row.DisplayName),
		         int32(Diff) == I ? TextMain : TextMuted, CX - 180.f + I * 170.f, Y);
	}
	Y += 60.f;

	DrawText(TEXT("ПРОБЕЛ — начать операцию"), FLinearColor(0.88f, 0.7f, 0.24f), CX - 140.f, Y, nullptr, 1.2f);
	Y += 50.f;
	DrawText(TEXT("ЛКМ — рамка · ПКМ — приказ · A — атака-движение · S — стоп · F — вся армия на экране"),
	         TextMuted, CX - 330.f, Y);
	Y += 22.f;
	DrawText(TEXT("Ctrl+1..9 — группы · H — к базе · колесо — зум · миникарта кликабельна · R после боя — заново"),
	         TextMuted, CX - 330.f, Y);
}

void ARTSHUD::DrawPanelRect(float X, float Y, float W, float H)
{
	DrawRect(ConsoleBg, X, Y, W, H);
	DrawRect(ConsoleLine, X, Y, W, 2.f);
}

void ARTSHUD::DrawMarquee(ARTSPlayerController* RTSController)
{
	if (!RTSController->IsSelecting())
	{
		return;
	}
	const FVector2D A = RTSController->GetSelectStart();
	const FVector2D B = RTSController->GetMouseScreen();
	const float X0 = FMath::Min(A.X, B.X), X1 = FMath::Max(A.X, B.X);
	const float Y0 = FMath::Min(A.Y, B.Y), Y1 = FMath::Max(A.Y, B.Y);
	DrawRect(FLinearColor(0.92f, 0.9f, 0.85f, 0.08f), X0, Y0, X1 - X0, Y1 - Y0);
	DrawLine(X0, Y0, X1, Y0, Marquee, 1.5f);
	DrawLine(X1, Y0, X1, Y1, Marquee, 1.5f);
	DrawLine(X1, Y1, X0, Y1, Marquee, 1.5f);
	DrawLine(X0, Y1, X0, Y0, Marquee, 1.5f);
}

void ARTSHUD::DrawTopBar(ARTSPlayerController* RTSController)
{
	UWorld* World = GetWorld();
	URTSEconomySubsystem* Econ = World ? World->GetSubsystem<URTSEconomySubsystem>() : nullptr;
	URTSDataSubsystem* Data = World && World->GetGameInstance()
		? World->GetGameInstance()->GetSubsystem<URTSDataSubsystem>() : nullptr;
	if (!Econ || !Data)
	{
		return;
	}

	DrawPanelRect(0.f, 0.f, Canvas->SizeX, 30.f);

	const int32 Credits = Econ->GetCredits(0);
	const int32 Use = Econ->GetPowerUse(0);
	const int32 Produce = Econ->GetPowerProduce(0);
	const bool bDeficit = Econ->HasPowerDeficit(0);

	DrawText(FString::Printf(TEXT("Припасы: %d"), Credits), TextMain, 14.f, 7.f);
	DrawText(FString::Printf(TEXT("Энергия: %d/%d"), Use, Produce),
	         bDeficit ? WarnRed : OkGreen, 170.f, 7.f);
	if (bDeficit)
	{
		DrawText(TEXT("ДЕФИЦИТ: производство ×0.5"), WarnRed, 320.f, 7.f);
	}

	const int32 Time = FMath::FloorToInt(Econ->GetMatchTime());
	DrawText(FString::Printf(TEXT("%02d:%02d"), Time / 60, Time % 60), TextMuted,
	         Canvas->SizeX * 0.5f - 20.f, 7.f);

	const FFactionRow& Faction = Data->GetFaction(Econ->GetFactionOf(0));
	DrawText(Faction.DisplayName, Faction.Color, Canvas->SizeX - 170.f, 7.f);

	// режимы
	if (RTSController->IsPlacing())
	{
		const FBuildingRow& Row = Data->GetBuilding(RTSController->GetPlacementKind());
		DrawText(FString::Printf(TEXT("СТРОЙКА: %s (%d). ЛКМ — поставить, ПКМ — отмена"),
			*Row.DisplayName, Data->GetBuildingCost(RTSController->GetPlacementKind(), Econ->GetFactionOf(0))),
			FLinearColor(0.88f, 0.7f, 0.24f), Canvas->SizeX * 0.5f - 260.f, 34.f);
	}
	else if (RTSController->IsAttackMoveArmed())
	{
		DrawText(TEXT("АТАКА-ДВИЖЕНИЕ: укажите точку (ЛКМ)"), WarnRed, Canvas->SizeX * 0.5f - 170.f, 34.f);
	}
}

void ARTSHUD::DrawSelectionPanel(ARTSPlayerController* RTSController)
{
	UWorld* World = GetWorld();
	URTSEconomySubsystem* Econ = World ? World->GetSubsystem<URTSEconomySubsystem>() : nullptr;
	URTSDataSubsystem* Data = World && World->GetGameInstance()
		? World->GetGameInstance()->GetSubsystem<URTSDataSubsystem>() : nullptr;
	if (!Econ || !Data)
	{
		return;
	}

	const float PanelH = 120.f;
	const float Y0 = Canvas->SizeY - PanelH;
	DrawPanelRect(0.f, Y0, Canvas->SizeX, PanelH);

	// выделенное здание: статус + очередь производства + хоткеи
	if (ABuildingBase* Building = RTSController->GetSelectedBuilding())
	{
		const FBuildingRow& Row = Data->GetBuilding(Building->BuildingKind);
		FString Title = Row.DisplayName;
		if (!Building->IsCompleted())
		{
			Title += FString::Printf(TEXT("  — стройка %d%%"), FMath::RoundToInt(Building->BuildProgress * 100.f));
		}
		DrawText(Title, TextMain, 16.f, Y0 + 10.f);
		if (Building->Health)
		{
			DrawText(FString::Printf(TEXT("Прочность %d/%d"),
				FMath::CeilToInt(Building->Health->GetHp()), FMath::CeilToInt(Building->Health->GetMaxHp())),
				TextMuted, 16.f, Y0 + 32.f);
		}

		if (Building->Production && Building->Production->CanProduceAnything() && Building->IsCompleted())
		{
			FString Hotkeys;
			switch (Building->BuildingKind)
			{
			case ERTSBuildingKind::HQ:       Hotkeys = TEXT("Q — Техник   W — Грузовик"); break;
			case ERTSBuildingKind::Barracks: Hotkeys = TEXT("Q — Пехотинец   W — Ракетчик"); break;
			case ERTSBuildingKind::Factory:  Hotkeys = TEXT("Q — Разведмашина   W — Танк   E — Артиллерия"); break;
			default: break;
			}
			DrawText(Hotkeys, TextMain, 16.f, Y0 + 56.f);

			const TArray<FRTSProductionItem>& Queue = Building->Production->GetQueue();
			FString QueueText = TEXT("Очередь: ");
			if (Queue.Num() == 0)
			{
				QueueText += TEXT("пусто");
			}
			for (int32 I = 0; I < Queue.Num(); ++I)
			{
				const FUnitRow& UnitRow = Data->GetUnit(Queue[I].Kind);
				QueueText += FString::Printf(TEXT("%s%s [%d%%]"), I > 0 ? TEXT(", ") : TEXT(""),
					*UnitRow.DisplayName, FMath::RoundToInt(Queue[I].Progress * 100.f));
			}
			DrawText(QueueText, TextMuted, 16.f, Y0 + 80.f);
			DrawText(TEXT("ПКМ на карте — точка сбора"), TextMuted, Canvas->SizeX - 300.f, Y0 + 10.f);
		}
		return;
	}

	// выделенные юниты
	const TArray<TWeakObjectPtr<AUnitBase>>& Selected = RTSController->GetSelectedUnits();
	int32 Count = 0;
	AUnitBase* First = nullptr;
	for (const TWeakObjectPtr<AUnitBase>& Weak : Selected)
	{
		if (AUnitBase* Unit = Weak.Get())
		{
			++Count;
			if (!First)
			{
				First = Unit;
			}
		}
	}

	if (Count == 0)
	{
		DrawText(TEXT("Нет выделения. ЛКМ — рамка. H — к базе. Ctrl+1..9 — группы."),
		         TextMuted, 16.f, Y0 + 12.f);
		return;
	}

	if (Count == 1 && First)
	{
		const FUnitRow& Row = Data->GetUnit(First->UnitKind);
		DrawText(Row.DisplayName, TextMain, 16.f, Y0 + 10.f);
		if (First->Health)
		{
			DrawText(FString::Printf(TEXT("Прочность %d/%d"),
				FMath::CeilToInt(First->Health->GetHp()), FMath::CeilToInt(First->Health->GetMaxHp())),
				TextMuted, 16.f, Y0 + 32.f);
		}
		if (Row.Damage > 0.f)
		{
			DrawText(FString::Printf(TEXT("Урон %d · дальность %d"),
				FMath::RoundToInt(Row.Damage), FMath::RoundToInt(Row.Range)), TextMuted, 16.f, Y0 + 54.f);
		}
		else if (First->UnitKind == ERTSUnitKind::Truck)
		{
			DrawText(FString::Printf(TEXT("Груз: %d/%d"), FMath::RoundToInt(First->Cargo),
				FMath::RoundToInt(RTSCore::Econ::TruckCapacity)), TextMuted, 16.f, Y0 + 54.f);
		}
	}
	else
	{
		DrawText(FString::Printf(TEXT("Выделено юнитов: %d"), Count), TextMain, 16.f, Y0 + 10.f);
	}

	// контекстная подсказка
	if (RTSController->HasTechnicianSelected())
	{
		DrawText(TEXT("Стройка: Q — Энергостанция  W — Казармы  E — Завод  R — Турель  T — КЦ"),
		         FLinearColor(0.88f, 0.7f, 0.24f), 16.f, Y0 + 92.f);
	}
	else
	{
		DrawText(TEXT("ПКМ — приказ · A — атака-движение · S — стоп"), TextMuted, 16.f, Y0 + 92.f);
	}
}

// --- миникарта --------------------------------------------------------------------

void ARTSHUD::DrawMinimap()
{
	UWorld* World = GetWorld();
	URTSEconomySubsystem* Econ = World ? World->GetSubsystem<URTSEconomySubsystem>() : nullptr;
	URTSFogSubsystem* Fog = World ? World->GetSubsystem<URTSFogSubsystem>() : nullptr;
	URTSDataSubsystem* Data = World && World->GetGameInstance()
		? World->GetGameInstance()->GetSubsystem<URTSDataSubsystem>() : nullptr;
	if (!Econ || !Data)
	{
		return;
	}
	const FRTSMinimapLayout Layout = GetMinimapLayout(Canvas->SizeX, Canvas->SizeY);

	// подложка «бумага» + рамка
	DrawRect(FLinearColor(0.72f, 0.67f, 0.5f, 0.95f), Layout.X, Layout.Y, Layout.Size, Layout.Size);
	DrawRect(ConsoleLine, Layout.X - 2.f, Layout.Y - 2.f, Layout.Size + 4.f, 2.f);
	DrawRect(ConsoleLine, Layout.X - 2.f, Layout.Y + Layout.Size, Layout.Size + 4.f, 2.f);
	DrawRect(ConsoleLine, Layout.X - 2.f, Layout.Y, 2.f, Layout.Size);
	DrawRect(ConsoleLine, Layout.X + Layout.Size, Layout.Y, 2.f, Layout.Size);

	// туман: агрегируем сетку 64×64 в блоки 2×2 (32×32 прямоугольников)
	if (Fog)
	{
		const int32 Block = 2;
		const int32 N = URTSFogSubsystem::GridSize / Block; // 32
		const float Cell = Layout.Size / float(N);
		for (int32 BY = 0; BY < N; ++BY)
		{
			for (int32 BX = 0; BX < N; ++BX)
			{
				uint8 MaxState = 0;
				for (int32 DY = 0; DY < Block; ++DY)
				{
					for (int32 DX = 0; DX < Block; ++DX)
					{
						MaxState = FMath::Max(MaxState, Fog->CellAt(BX * Block + DX, BY * Block + DY));
					}
				}
				if (MaxState == 2)
				{
					continue; // видно — бумага без затемнения
				}
				// сетка X — вертикаль миникарты (верх = +X): тайл (BX,BY) — мир
				// X = BY-строка? Нет: CellAt(X,Y): X — мировой X-столбец? В фоге
				// X — столбец по мировому X, Y — по Y. Мир→мини: U от Y, V от X.
				const float PX = Layout.X + (float(BY) + 0.f) * Cell;          // U ← мировой Y (BY)
				const float PY = Layout.Y + Layout.Size - (float(BX) + 1.f) * Cell; // V ← мировой X (BX)
				const float Alpha = MaxState == 1 ? 0.35f : 0.85f;
				DrawRect(FLinearColor(0.05f, 0.055f, 0.065f, Alpha), PX, PY, Cell + 0.5f, Cell + 0.5f);
			}
		}
	}

	// склады
	for (ASupplyDepot* Depot : Econ->GetAllDepots())
	{
		if (!IsValid(Depot) || !Depot->HasSupplies())
		{
			continue;
		}
		if (Fog && !Fog->IsExplored(Depot->GetActorLocation()))
		{
			continue;
		}
		const FVector2D P = WorldToMinimap(Layout, Depot->GetActorLocation());
		DrawRect(FLinearColor(0.9f, 0.85f, 0.5f), P.X - 2.f, P.Y - 2.f, 4.f, 4.f);
	}

	// здания (враг — только разведанные), мигание атакованных
	const float Now = World->GetTimeSeconds();
	for (ABuildingBase* Building : Econ->GetAllBuildings())
	{
		if (!IsValid(Building))
		{
			continue;
		}
		if (Building->TeamId != 0 && !Building->bSeenByPlayer)
		{
			continue;
		}
		const FVector2D P = WorldToMinimap(Layout, Building->GetActorLocation());
		DrawRect(Data->GetFaction(Building->Faction).Color, P.X - 3.f, P.Y - 3.f, 6.f, 6.f);
		if (Building->TeamId == 0 && Now - Building->LastDamagedTime < 3.f &&
			FMath::Fmod(Now, 0.5f) < 0.25f)
		{
			DrawRect(FLinearColor(1.f, 0.25f, 0.15f), P.X - 5.f, P.Y - 5.f, 10.f, 2.f);
			DrawRect(FLinearColor(1.f, 0.25f, 0.15f), P.X - 5.f, P.Y + 3.f, 10.f, 2.f);
		}
	}

	// юниты (враг — только в зоне видимости)
	for (AUnitBase* Unit : Econ->GetAllUnits())
	{
		if (!IsValid(Unit))
		{
			continue;
		}
		if (Unit->TeamId != 0 && Fog && !Fog->IsVisibleFor(0, Unit->GetActorLocation()))
		{
			continue;
		}
		const FVector2D P = WorldToMinimap(Layout, Unit->GetActorLocation());
		DrawRect(Data->GetFaction(Unit->Faction).Color, P.X - 1.5f, P.Y - 1.5f, 3.f, 3.f);
	}

	// рамка обзора камеры
	if (ARTSPlayerController* RTSController = Cast<ARTSPlayerController>(PlayerOwner))
	{
		if (const APawn* CamPawn = RTSController->GetPawn())
		{
			const ARTSCameraPawn* Cam = Cast<ARTSCameraPawn>(CamPawn);
			const float HalfView = Cam ? Cam->GetArmLength() * 0.55f : 1500.f;
			const FVector2D C = WorldToMinimap(Layout, CamPawn->GetActorLocation());
			const float R = HalfView / RTSCore::MapHalfSize * Layout.Size * 0.5f;
			const FLinearColor Frame(0.95f, 0.93f, 0.87f, 0.9f);
			DrawRect(Frame, C.X - R, C.Y - R, R * 2.f, 1.f);
			DrawRect(Frame, C.X - R, C.Y + R, R * 2.f, 1.f);
			DrawRect(Frame, C.X - R, C.Y - R, 1.f, R * 2.f);
			DrawRect(Frame, C.X + R, C.Y - R, 1.f, R * 2.f);
		}
	}
}

void ARTSHUD::DrawEndBanner()
{
	UWorld* World = GetWorld();
	URTSEconomySubsystem* Econ = World ? World->GetSubsystem<URTSEconomySubsystem>() : nullptr;
	if (!Econ || !Econ->IsMatchEnded())
	{
		return;
	}
	const bool bWon = Econ->GetWinnerTeam() == 0;

	DrawRect(FLinearColor(0.04f, 0.045f, 0.055f, 0.82f), 0.f, 0.f, Canvas->SizeX, Canvas->SizeY);
	const FLinearColor StampColor = bWon
		? FLinearColor(0.055f, 0.561f, 0.514f) : FLinearColor(0.78f, 0.27f, 0.17f);

	const float CenterX = Canvas->SizeX * 0.5f;
	const float CenterY = Canvas->SizeY * 0.4f;
	DrawText(bWon ? TEXT("П О Б Е Д А") : TEXT("Р А З Г Р О М"), StampColor,
	         CenterX - 170.f, CenterY - 40.f, nullptr, 4.f);

	const FRTSTeamStats& Mine = Econ->GetStats(0);
	const FRTSTeamStats& Theirs = Econ->GetStats(1);
	const int32 Time = FMath::FloorToInt(Econ->GetMatchTime());

	TArray<FString> Lines;
	Lines.Add(FString::Printf(TEXT("Время операции: %02d:%02d"), Time / 60, Time % 60));
	Lines.Add(FString::Printf(TEXT("Собрано припасов: %d — у противника %d"), Mine.SuppliesGathered, Theirs.SuppliesGathered));
	Lines.Add(FString::Printf(TEXT("Юнитов выпущено: %d — потеряно %d"), Mine.UnitsBuilt, Mine.UnitsLost));
	Lines.Add(FString::Printf(TEXT("Зданий построено: %d — потеряно %d"), Mine.BuildingsBuilt, Mine.BuildingsLost));
	Lines.Add(FString::Printf(TEXT("Целей уничтожено: %d"), Mine.EnemiesKilled));

	for (int32 I = 0; I < Lines.Num(); ++I)
	{
		DrawText(Lines[I], TextMain, CenterX - 190.f, CenterY + 60.f + I * 26.f);
	}
	DrawText(TEXT("R — новая операция"), FLinearColor(0.88f, 0.7f, 0.24f),
	         CenterX - 90.f, CenterY + 60.f + Lines.Num() * 26.f + 30.f, nullptr, 1.2f);
}
