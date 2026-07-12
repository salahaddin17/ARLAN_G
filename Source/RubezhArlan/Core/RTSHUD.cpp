#include "Core/RTSHUD.h"
#include "RubezhArlan.h"
#include "Core/RTSPlayerController.h"
#include "Units/UnitBase.h"
#include "Buildings/BuildingBase.h"
#include "Components/RTSHealthComponent.h"
#include "Components/RTSProductionComponent.h"
#include "Data/RTSDataSubsystem.h"
#include "Econ/RTSEconomySubsystem.h"
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
	DrawTopBar(RTSController);
	DrawSelectionPanel(RTSController);
	DrawMarquee(RTSController);
	DrawEndBanner();
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
}
