#include "Fog/RTSFogSubsystem.h"
#include "RubezhArlan.h"
#include "Econ/RTSEconomySubsystem.h"
#include "Units/UnitBase.h"
#include "Buildings/BuildingBase.h"
#include "Data/RTSDataSubsystem.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"

URTSFogSubsystem::URTSFogSubsystem()
{
	for (int32 I = 0; I < GridSize * GridSize; ++I)
	{
		Cells[I] = 0;
	}
}

void URTSFogSubsystem::WorldToTile(const FVector& WorldPos, int32& OutX, int32& OutY)
{
	OutX = FMath::FloorToInt((float(WorldPos.X) + RTSCore::MapHalfSize) / RTSCore::UUPerTile);
	OutY = FMath::FloorToInt((float(WorldPos.Y) + RTSCore::MapHalfSize) / RTSCore::UUPerTile);
}

bool URTSFogSubsystem::IsVisibleFor(uint8 ViewerTeam, const FVector& WorldPos) const
{
	if (ViewerTeam != 0)
	{
		return true; // ИИ туман не рисуем и не считаем
	}
	int32 X, Y;
	WorldToTile(WorldPos, X, Y);
	return CellAt(X, Y) == 2;
}

bool URTSFogSubsystem::IsExplored(const FVector& WorldPos) const
{
	int32 X, Y;
	WorldToTile(WorldPos, X, Y);
	return CellAt(X, Y) >= 1;
}

float URTSFogSubsystem::GetExploredFraction() const
{
	int32 N = 0;
	for (int32 I = 0; I < GridSize * GridSize; ++I)
	{
		if (Cells[I] >= 1)
		{
			++N;
		}
	}
	return float(N) / float(GridSize * GridSize);
}

void URTSFogSubsystem::Step(float DeltaSeconds)
{
	Accum += DeltaSeconds;
	if (Accum < 0.25f)
	{
		return;
	}
	Accum = 0.f;
	Recompute();
	ApplyActorVisibility();
}

void URTSFogSubsystem::Recompute()
{
	// «видно» деградирует до «разведано»
	for (int32 I = 0; I < GridSize * GridSize; ++I)
	{
		if (Cells[I] == 2)
		{
			Cells[I] = 1;
		}
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	URTSEconomySubsystem* Econ = World->GetSubsystem<URTSEconomySubsystem>();
	URTSDataSubsystem* Data = World->GetGameInstance()
		? World->GetGameInstance()->GetSubsystem<URTSDataSubsystem>() : nullptr;
	if (!Econ || !Data)
	{
		return;
	}

	for (AUnitBase* Unit : Econ->GetAllUnits())
	{
		if (IsValid(Unit) && Unit->TeamId == 0)
		{
			StampVision(Unit->GetActorLocation(), Data->GetUnit(Unit->UnitKind).VisionTiles);
		}
	}
	for (ABuildingBase* Building : Econ->GetAllBuildings())
	{
		if (IsValid(Building) && Building->TeamId == 0)
		{
			const int32 Vision = Building->IsCompleted()
				? Data->GetBuilding(Building->BuildingKind).VisionTiles : 2;
			StampVision(Building->GetActorLocation(), Vision);
		}
	}
}

void URTSFogSubsystem::StampVision(const FVector& Center, int32 VisionTiles)
{
	int32 CX, CY;
	WorldToTile(Center, CX, CY);
	const int32 R = FMath::Max(1, VisionTiles);
	const float R2 = (R + 0.5f) * (R + 0.5f);
	for (int32 DY = -R; DY <= R; ++DY)
	{
		const int32 Y = CY + DY;
		if (Y < 0 || Y >= GridSize)
		{
			continue;
		}
		for (int32 DX = -R; DX <= R; ++DX)
		{
			const int32 X = CX + DX;
			if (X < 0 || X >= GridSize)
			{
				continue;
			}
			if (float(DX * DX + DY * DY) <= R2)
			{
				Cells[Y * GridSize + X] = 2;
			}
		}
	}
}

void URTSFogSubsystem::ApplyActorVisibility()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	URTSEconomySubsystem* Econ = World->GetSubsystem<URTSEconomySubsystem>();
	if (!Econ)
	{
		return;
	}

	for (AUnitBase* Unit : Econ->GetAllUnits())
	{
		if (IsValid(Unit) && Unit->TeamId != 0)
		{
			Unit->SetActorHiddenInGame(!IsVisibleFor(0, Unit->GetActorLocation()));
		}
	}
	for (ABuildingBase* Building : Econ->GetAllBuildings())
	{
		if (!IsValid(Building) || Building->TeamId == 0)
		{
			continue;
		}
		// здание, однажды разведанное, остаётся видимым «призраком»
		if (Building->bSeenByPlayer)
		{
			Building->SetActorHiddenInGame(false);
		}
		else if (IsVisibleFor(0, Building->GetActorLocation()))
		{
			Building->bSeenByPlayer = true;
			Building->SetActorHiddenInGame(false);
		}
		else
		{
			Building->SetActorHiddenInGame(true);
		}
	}
}
