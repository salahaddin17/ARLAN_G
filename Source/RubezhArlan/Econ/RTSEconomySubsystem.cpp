#include "Econ/RTSEconomySubsystem.h"
#include "RubezhArlan.h"
#include "Units/UnitBase.h"
#include "Buildings/BuildingBase.h"
#include "Econ/SupplyDepot.h"
#include "Fog/RTSFogSubsystem.h"
#include "Components/RTSHealthComponent.h"
#include "Engine/World.h"

void URTSEconomySubsystem::SetupMatch(ERTSFaction PlayerFaction, ERTSDifficulty InDifficulty)
{
	TeamFactions[0] = PlayerFaction;
	TeamFactions[1] = PlayerFaction == ERTSFaction::Legion ? ERTSFaction::Front : ERTSFaction::Legion;
	Difficulty = InDifficulty;
	Credits[0] = RTSCore::Econ::StartCredits;
	Credits[1] = RTSCore::Econ::StartCredits;
	Stats[0] = FRTSTeamStats();
	Stats[1] = FRTSTeamStats();
	MatchTime = 0.f;
	bMatchEnded = false;
	WinnerTeam = -1;
	UE_LOG(LogRubezh, Log, TEXT("Матч: игрок=%d, сложность=%d"), int32(PlayerFaction), int32(InDifficulty));
}

// --- реестры -----------------------------------------------------------------

void URTSEconomySubsystem::RegisterUnit(AUnitBase* Unit) { Units.AddUnique(Unit); }
void URTSEconomySubsystem::UnregisterUnit(AUnitBase* Unit) { Units.Remove(Unit); }
void URTSEconomySubsystem::RegisterBuilding(ABuildingBase* Building) { Buildings.AddUnique(Building); }
void URTSEconomySubsystem::UnregisterBuilding(ABuildingBase* Building) { Buildings.Remove(Building); }
void URTSEconomySubsystem::RegisterDepot(ASupplyDepot* Depot) { Depots.AddUnique(Depot); }
void URTSEconomySubsystem::UnregisterDepot(ASupplyDepot* Depot) { Depots.Remove(Depot); }

// --- припасы ------------------------------------------------------------------

void URTSEconomySubsystem::AddCredits(uint8 TeamId, int32 Amount)
{
	Credits[TeamId & 1] += Amount;
}

bool URTSEconomySubsystem::TrySpend(uint8 TeamId, int32 Amount)
{
	if (Credits[TeamId & 1] < Amount)
	{
		return false;
	}
	Credits[TeamId & 1] -= Amount;
	return true;
}

float URTSEconomySubsystem::GetProductionMultiplier(uint8 TeamId) const
{
	return RTSCore::Econ::ProductionMultiplier(GetPowerUse(TeamId), GetPowerProduce(TeamId));
}

// --- запросы -------------------------------------------------------------------

AUnitBase* URTSEconomySubsystem::FindNearestEnemyUnit(uint8 MyTeam, const FVector& From, float MaxRange) const
{
	AUnitBase* Best = nullptr;
	float BestDist = MaxRange;
	for (AUnitBase* Unit : Units)
	{
		if (!IsValid(Unit) || Unit->TeamId == MyTeam || !Unit->Health || !Unit->Health->IsAlive())
		{
			continue;
		}
		const float D = float(FVector::Dist2D(From, Unit->GetActorLocation()));
		if (D < BestDist)
		{
			BestDist = D;
			Best = Unit;
		}
	}
	return Best;
}

AActor* URTSEconomySubsystem::FindNearestEnemyTarget(uint8 MyTeam, const FVector& From,
                                                     float MaxRange, bool bRequireVisible) const
{
	const URTSFogSubsystem* Fog = nullptr;
	if (bRequireVisible && MyTeam == 0)
	{
		if (const UWorld* World = GetWorld())
		{
			Fog = World->GetSubsystem<URTSFogSubsystem>();
		}
	}

	AActor* Best = nullptr;
	float BestDist = MaxRange;
	for (AUnitBase* Unit : Units)
	{
		if (!IsValid(Unit) || Unit->TeamId == MyTeam || !Unit->Health || !Unit->Health->IsAlive())
		{
			continue;
		}
		if (Fog && !Fog->IsVisibleFor(MyTeam, Unit->GetActorLocation()))
		{
			continue;
		}
		const float D = float(FVector::Dist2D(From, Unit->GetActorLocation()));
		if (D < BestDist)
		{
			BestDist = D;
			Best = Unit;
		}
	}
	if (Best)
	{
		return Best; // юниты приоритетнее зданий
	}

	BestDist = MaxRange;
	for (ABuildingBase* Building : Buildings)
	{
		if (!IsValid(Building) || Building->TeamId == MyTeam || !Building->Health || !Building->Health->IsAlive())
		{
			continue;
		}
		if (Fog && !Fog->IsVisibleFor(MyTeam, Building->GetActorLocation()))
		{
			continue;
		}
		const float D = float(FVector::Dist2D(From, Building->GetActorLocation()));
		if (D < BestDist)
		{
			BestDist = D;
			Best = Building;
		}
	}
	return Best;
}

ABuildingBase* URTSEconomySubsystem::FindNearestEnemyBuilding(uint8 MyTeam, const FVector& From) const
{
	ABuildingBase* Best = nullptr;
	float BestDist = 1e12f;
	for (ABuildingBase* Building : Buildings)
	{
		if (!IsValid(Building) || Building->TeamId == MyTeam)
		{
			continue;
		}
		const float D = float(FVector::Dist2D(From, Building->GetActorLocation()));
		if (D < BestDist)
		{
			BestDist = D;
			Best = Building;
		}
	}
	return Best;
}

ASupplyDepot* URTSEconomySubsystem::FindNearestDepotWithSupplies(const FVector& From) const
{
	ASupplyDepot* Best = nullptr;
	float BestDist = 1e12f;
	for (ASupplyDepot* Depot : Depots)
	{
		if (!IsValid(Depot) || !Depot->HasSupplies())
		{
			continue;
		}
		const float D = float(FVector::Dist2D(From, Depot->GetActorLocation()));
		if (D < BestDist)
		{
			BestDist = D;
			Best = Depot;
		}
	}
	return Best;
}

ABuildingBase* URTSEconomySubsystem::FindNearestHQ(uint8 TeamId, const FVector& From) const
{
	ABuildingBase* Best = nullptr;
	float BestDist = 1e12f;
	for (ABuildingBase* Building : Buildings)
	{
		if (!IsValid(Building) || Building->TeamId != TeamId ||
			Building->BuildingKind != ERTSBuildingKind::HQ || !Building->IsCompleted())
		{
			continue;
		}
		const float D = float(FVector::Dist2D(From, Building->GetActorLocation()));
		if (D < BestDist)
		{
			BestDist = D;
			Best = Building;
		}
	}
	return Best;
}

int32 URTSEconomySubsystem::CountUnitsOfKind(uint8 TeamId, ERTSUnitKind Kind) const
{
	int32 N = 0;
	for (AUnitBase* Unit : Units)
	{
		if (IsValid(Unit) && Unit->TeamId == TeamId && Unit->UnitKind == Kind)
		{
			++N;
		}
	}
	return N;
}

int32 URTSEconomySubsystem::CountBuildingsOfKind(uint8 TeamId, ERTSBuildingKind Kind) const
{
	int32 N = 0;
	for (ABuildingBase* Building : Buildings)
	{
		if (IsValid(Building) && Building->TeamId == TeamId && Building->BuildingKind == Kind)
		{
			++N;
		}
	}
	return N;
}

bool URTSEconomySubsystem::IsPlacementFree(const FVector& Center, const FVector2D& HalfExtents) const
{
	const float Bound = RTSCore::MapHalfSize - 200.f;
	if (FMath::Abs(float(Center.X)) + HalfExtents.X > Bound ||
		FMath::Abs(float(Center.Y)) + HalfExtents.Y > Bound)
	{
		return false;
	}
	for (ABuildingBase* Building : Buildings)
	{
		if (!IsValid(Building))
		{
			continue;
		}
		const FVector2D Other = Building->GetFootprintExtents();
		const FVector Delta = Building->GetActorLocation() - Center;
		const float Margin = 60.f; // зазор для проходимости
		if (FMath::Abs(float(Delta.X)) < HalfExtents.X + Other.X + Margin &&
			FMath::Abs(float(Delta.Y)) < HalfExtents.Y + Other.Y + Margin)
		{
			return false;
		}
	}
	for (ASupplyDepot* Depot : Depots)
	{
		if (!IsValid(Depot) || !Depot->HasSupplies())
		{
			continue;
		}
		const FVector Delta = Depot->GetActorLocation() - Center;
		if (FMath::Abs(float(Delta.X)) < HalfExtents.X + 250.f &&
			FMath::Abs(float(Delta.Y)) < HalfExtents.Y + 250.f)
		{
			return false;
		}
	}
	return true;
}

// --- ход матча ---------------------------------------------------------------------

void URTSEconomySubsystem::Step(float DeltaSeconds)
{
	if (bMatchEnded)
	{
		return;
	}
	MatchTime += DeltaSeconds;

	PowerAccum += DeltaSeconds;
	if (PowerAccum >= 0.25f)
	{
		PowerAccum = 0.f;
		RecomputePower();
	}

	VictoryAccum += DeltaSeconds;
	if (VictoryAccum >= 1.f)
	{
		VictoryAccum = 0.f;
		CheckVictory();
	}
}

void URTSEconomySubsystem::RecomputePower()
{
	PowerUse[0] = PowerUse[1] = 0;
	PowerProduce[0] = PowerProduce[1] = 0;
	for (ABuildingBase* Building : Buildings)
	{
		if (!IsValid(Building) || !Building->IsCompleted())
		{
			continue;
		}
		const uint8 T = Building->TeamId & 1;
		PowerUse[T] += Building->GetPowerUse();
		PowerProduce[T] += Building->GetPowerProduce();
	}
}

void URTSEconomySubsystem::CheckVictory()
{
	// стартовая пауза: базы ещё спавнятся
	if (MatchTime < 5.f)
	{
		return;
	}
	int32 Count[2] = {0, 0};
	for (ABuildingBase* Building : Buildings)
	{
		if (IsValid(Building) && Building->Health && Building->Health->IsAlive())
		{
			++Count[Building->TeamId & 1];
		}
	}
	if (Count[0] > 0 && Count[1] > 0)
	{
		return;
	}
	bMatchEnded = true;
	WinnerTeam = Count[0] > 0 ? 0 : 1;
	UE_LOG(LogRubezh, Log, TEXT("Матч окончен: победитель — команда %d, время %.0f c"), WinnerTeam, MatchTime);
}
