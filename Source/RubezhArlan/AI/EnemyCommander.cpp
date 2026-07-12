#include "AI/EnemyCommander.h"
#include "RubezhArlan.h"
#include "Units/UnitBase.h"
#include "Units/RTSAIController.h"
#include "Buildings/BuildingBase.h"
#include "Components/RTSHealthComponent.h"
#include "Components/RTSProductionComponent.h"
#include "Data/RTSDataSubsystem.h"
#include "Econ/RTSEconomySubsystem.h"
#include "Econ/SupplyDepot.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"

AEnemyCommander::AEnemyCommander()
{
	PrimaryActorTick.bCanEverTick = true;

	BuildPlan.Add(ERTSBuildingKind::Power);
	BuildPlan.Add(ERTSBuildingKind::Barracks);
	BuildPlan.Add(ERTSBuildingKind::Power);
	BuildPlan.Add(ERTSBuildingKind::Turret);
	BuildPlan.Add(ERTSBuildingKind::Factory);
	BuildPlan.Add(ERTSBuildingKind::Turret);
	BuildPlan.Add(ERTSBuildingKind::Power);
	BuildPlan.Add(ERTSBuildingKind::Factory);
	BuildPlan.Add(ERTSBuildingKind::Turret);
	BuildPlan.Add(ERTSBuildingKind::Turret);
}

void AEnemyCommander::BeginPlay()
{
	AActor::BeginPlay();
	if (UWorld* World = GetWorld())
	{
		URTSDataSubsystem* Data = World->GetGameInstance()
			? World->GetGameInstance()->GetSubsystem<URTSDataSubsystem>() : nullptr;
		URTSEconomySubsystem* Econ = World->GetSubsystem<URTSEconomySubsystem>();
		if (Data && Econ)
		{
			const FDifficultyRow& Diff = Data->GetDifficulty(Econ->GetDifficulty());
			NextWaveAt = Diff.FirstWaveAt;
			WaveTarget = Diff.WaveStart;
		}
	}
}

void AEnemyCommander::Tick(float DeltaSeconds)
{
	AActor::Tick(DeltaSeconds);
	ThinkAccum += DeltaSeconds;
	if (ThinkAccum >= 1.f)
	{
		ThinkAccum = 0.f;
		Think();
	}
}

void AEnemyCommander::Think()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	URTSEconomySubsystem* Econ = World->GetSubsystem<URTSEconomySubsystem>();
	URTSDataSubsystem* Data = World->GetGameInstance()
		? World->GetGameInstance()->GetSubsystem<URTSDataSubsystem>() : nullptr;
	if (!Econ || !Data || Econ->IsMatchEnded())
	{
		return;
	}

	ManageEconomy(Econ);
	ManageConstruction(Econ, Data);
	ManageArmyProduction(Econ, Data);
	ManageDefense(Econ);
	ManageWaves(Econ, Data);
}

// --- выборки ---------------------------------------------------------------------

ABuildingBase* AEnemyCommander::MyHQ(URTSEconomySubsystem* Econ) const
{
	return Econ->FindNearestHQ(AITeam, GetActorLocation());
}

TArray<AUnitBase*> AEnemyCommander::MyUnits(URTSEconomySubsystem* Econ, ERTSUnitKind Kind) const
{
	TArray<AUnitBase*> Out;
	for (AUnitBase* Unit : Econ->GetAllUnits())
	{
		if (IsValid(Unit) && Unit->TeamId == AITeam && Unit->UnitKind == Kind)
		{
			Out.Add(Unit);
		}
	}
	return Out;
}

TArray<AUnitBase*> AEnemyCommander::MyArmy(URTSEconomySubsystem* Econ) const
{
	TArray<AUnitBase*> Out;
	for (AUnitBase* Unit : Econ->GetAllUnits())
	{
		if (IsValid(Unit) && Unit->TeamId == AITeam && Unit->IsCombatUnit())
		{
			Out.Add(Unit);
		}
	}
	return Out;
}

// --- экономика --------------------------------------------------------------------

void AEnemyCommander::ManageEconomy(URTSEconomySubsystem* Econ)
{
	ABuildingBase* HQ = MyHQ(Econ);
	if (!HQ || !HQ->Production)
	{
		return;
	}

	const int32 WantTrucks = Econ->GetDifficulty() == ERTSDifficulty::Easy ? 2 : 3;
	const int32 Trucks = Econ->CountUnitsOfKind(AITeam, ERTSUnitKind::Truck);
	const int32 Techs = Econ->CountUnitsOfKind(AITeam, ERTSUnitKind::Technician);

	int32 QueuedTrucks = 0, QueuedTechs = 0;
	for (const FRTSProductionItem& Item : HQ->Production->GetQueue())
	{
		if (Item.Kind == ERTSUnitKind::Truck) { ++QueuedTrucks; }
		if (Item.Kind == ERTSUnitKind::Technician) { ++QueuedTechs; }
	}

	if (Techs + QueuedTechs < 1)
	{
		HQ->Production->TryEnqueue(ERTSUnitKind::Technician);
	}
	if (Trucks + QueuedTrucks < WantTrucks && HQ->Production->GetQueue().Num() < 2)
	{
		HQ->Production->TryEnqueue(ERTSUnitKind::Truck);
	}

	// простаивающие грузовики — обратно на маршрут
	for (AUnitBase* Truck : MyUnits(Econ, ERTSUnitKind::Truck))
	{
		ARTSAIController* Controller = Truck->GetRTSController();
		if (Controller && Controller->IsIdle())
		{
			if (ASupplyDepot* Depot = Econ->FindNearestDepotWithSupplies(Truck->GetActorLocation()))
			{
				Controller->SetOrder(FRTSOrder::MakeHarvest(Depot));
			}
		}
	}
}

// --- стройка ----------------------------------------------------------------------

ERTSBuildingKind AEnemyCommander::NextBuildItem(URTSEconomySubsystem* Econ) const
{
	// аварийные приоритеты
	if (Econ->HasPowerDeficit(AITeam))
	{
		return ERTSBuildingKind::Power;
	}
	if (Econ->CountBuildingsOfKind(AITeam, ERTSBuildingKind::HQ) == 0)
	{
		return ERTSBuildingKind::HQ;
	}
	if (BuildPlanIndex > 1 && Econ->CountBuildingsOfKind(AITeam, ERTSBuildingKind::Barracks) == 0)
	{
		return ERTSBuildingKind::Barracks;
	}
	if (BuildPlanIndex > 4 && Econ->CountBuildingsOfKind(AITeam, ERTSBuildingKind::Factory) == 0)
	{
		return ERTSBuildingKind::Factory;
	}
	if (BuildPlanIndex < BuildPlan.Num())
	{
		return BuildPlan[BuildPlanIndex];
	}
	// расширение при избытке
	if (Econ->GetCredits(AITeam) > 1400)
	{
		if (Econ->CountBuildingsOfKind(AITeam, ERTSBuildingKind::Factory) < 3)
		{
			return ERTSBuildingKind::Factory;
		}
		if (Econ->CountBuildingsOfKind(AITeam, ERTSBuildingKind::Turret) < 8)
		{
			return ERTSBuildingKind::Turret;
		}
		if (Econ->CountBuildingsOfKind(AITeam, ERTSBuildingKind::Power) < 6)
		{
			return ERTSBuildingKind::Power;
		}
	}
	return ERTSBuildingKind::HQ; // маркер «строить нечего» — HQ уже есть, вернётся false ниже
}

bool AEnemyCommander::FindBuildSpot(URTSEconomySubsystem* Econ, URTSDataSubsystem* Data,
                                    ERTSBuildingKind Kind, FVector& OutLocation) const
{
	ABuildingBase* HQ = MyHQ(Econ);
	if (!HQ)
	{
		return false;
	}
	const FBuildingRow& Row = Data->GetBuilding(Kind);
	const FVector2D HalfExtents(Row.SizeX * RTSCore::UUPerTile * 0.5f, Row.SizeY * RTSCore::UUPerTile * 0.5f);

	// турели — к фронту (в сторону игрока), остальное — кольцами вокруг КЦ
	const FVector Front = (FVector(-2100.f, -2100.f, 0.f) - HQ->GetActorLocation()).GetSafeNormal2D();

	const float Tile = RTSCore::UUPerTile;
	FVector Best = FVector::ZeroVector;
	float BestScore = 1e12f;
	bool bFound = false;

	for (int32 Ring = 3; Ring <= 14; ++Ring)
	{
		for (int32 DY = -Ring; DY <= Ring; ++DY)
		{
			for (int32 DX = -Ring; DX <= Ring; ++DX)
			{
				if (FMath::Max(FMath::Abs(DX), FMath::Abs(DY)) != Ring)
				{
					continue;
				}
				const FVector Candidate = HQ->GetActorLocation() + FVector(DX * Tile, DY * Tile, 0.f);
				if (!Econ->IsPlacementFree(Candidate, HalfExtents))
				{
					continue;
				}
				float Score = float(Ring);
				if (Kind == ERTSBuildingKind::Turret)
				{
					const FVector Dir = (Candidate - HQ->GetActorLocation()).GetSafeNormal2D();
					const float Toward = float(Dir.X * Front.X + Dir.Y * Front.Y);
					Score = -Toward * 10.f + FMath::Abs(float(Ring) - 8.f) * 0.5f;
				}
				if (Score < BestScore)
				{
					BestScore = Score;
					Best = Candidate;
					bFound = true;
				}
			}
		}
		if (bFound && Kind != ERTSBuildingKind::Turret && BestScore <= float(Ring))
		{
			break; // ближайшее кольцо найдено
		}
	}
	OutLocation = Best;
	return bFound;
}

void AEnemyCommander::ManageConstruction(URTSEconomySubsystem* Econ, URTSDataSubsystem* Data)
{
	TArray<AUnitBase*> Techs = MyUnits(Econ, ERTSUnitKind::Technician);
	if (Techs.Num() == 0)
	{
		return;
	}

	// недостроенные площадки — назначаем свободных техников
	TArray<ABuildingBase*> Sites;
	for (ABuildingBase* Building : Econ->GetAllBuildings())
	{
		if (IsValid(Building) && Building->TeamId == AITeam && !Building->IsCompleted())
		{
			Sites.Add(Building);
		}
	}
	for (AUnitBase* Tech : Techs)
	{
		ARTSAIController* Controller = Tech->GetRTSController();
		if (Controller && Controller->IsIdle() && Sites.Num() > 0)
		{
			ABuildingBase* Nearest = Sites[0];
			float BestDist = 1e12f;
			for (ABuildingBase* Site : Sites)
			{
				const float D = float(FVector::Dist2D(Tech->GetActorLocation(), Site->GetActorLocation()));
				if (D < BestDist)
				{
					BestDist = D;
					Nearest = Site;
				}
			}
			Controller->SetOrder(FRTSOrder::MakeBuild(Nearest));
		}
	}
	if (Sites.Num() > 0)
	{
		return; // одна стройка за раз
	}

	const ERTSBuildingKind Next = NextBuildItem(Econ);
	if (Next == ERTSBuildingKind::HQ && Econ->CountBuildingsOfKind(AITeam, ERTSBuildingKind::HQ) > 0)
	{
		return; // строить нечего
	}
	const int32 Cost = Data->GetBuildingCost(Next, Econ->GetFactionOf(AITeam));
	if (Econ->GetCredits(AITeam) < Cost)
	{
		return;
	}

	FVector Spot;
	if (!FindBuildSpot(Econ, Data, Next, Spot))
	{
		return;
	}
	if (!Econ->TrySpend(AITeam, Cost))
	{
		return;
	}

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ABuildingBase* Site = GetWorld()->SpawnActor<ABuildingBase>(Spot, FRotator::ZeroRotator, Params);
	if (!Site)
	{
		Econ->AddCredits(AITeam, Cost);
		return;
	}
	Site->InitBuilding(Next, AITeam, false);

	if (ARTSAIController* Controller = Techs[0]->GetRTSController())
	{
		Controller->SetOrder(FRTSOrder::MakeBuild(Site));
	}
	if (BuildPlanIndex < BuildPlan.Num() && BuildPlan[BuildPlanIndex] == Next)
	{
		++BuildPlanIndex;
	}
}

// --- армия -------------------------------------------------------------------------

void AEnemyCommander::ManageArmyProduction(URTSEconomySubsystem* Econ, URTSDataSubsystem* Data)
{
	const FDifficultyRow& Diff = Data->GetDifficulty(Econ->GetDifficulty());
	if (MyArmy(Econ).Num() >= Diff.ArmyCap)
	{
		return;
	}
	// резерв на здания, пока план не выполнен
	const int32 Reserve = BuildPlanIndex < BuildPlan.Num() ? 320 : 0;

	for (ABuildingBase* Building : Econ->GetAllBuildings())
	{
		if (!IsValid(Building) || Building->TeamId != AITeam || !Building->IsCompleted() ||
			!Building->Production || Building->Production->GetQueue().Num() >= 2)
		{
			continue;
		}
		ERTSUnitKind Kind;
		if (Building->BuildingKind == ERTSBuildingKind::Barracks)
		{
			Kind = FMath::FRand() < 0.55f ? ERTSUnitKind::Rifleman : ERTSUnitKind::Rocketeer;
		}
		else if (Building->BuildingKind == ERTSBuildingKind::Factory)
		{
			const float Roll = FMath::FRand();
			if (WaveNumber < 1)
			{
				Kind = Roll < 0.5f ? ERTSUnitKind::Scout : ERTSUnitKind::Tank;
			}
			else if (Roll < 0.5f)
			{
				Kind = ERTSUnitKind::Tank;
			}
			else if (Roll < 0.8f)
			{
				Kind = ERTSUnitKind::Artillery;
			}
			else
			{
				Kind = ERTSUnitKind::Scout;
			}
		}
		else
		{
			continue;
		}
		const int32 Cost = Data->GetUnitCost(Kind, Econ->GetFactionOf(AITeam));
		if (Econ->GetCredits(AITeam) - Cost >= Reserve)
		{
			Building->Production->TryEnqueue(Kind);
		}
	}
}

// --- оборона ------------------------------------------------------------------------

void AEnemyCommander::ManageDefense(URTSEconomySubsystem* Econ)
{
	const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
	ABuildingBase* Attacked = nullptr;
	for (ABuildingBase* Building : Econ->GetAllBuildings())
	{
		if (IsValid(Building) && Building->TeamId == AITeam && Now - Building->LastDamagedTime < 3.f)
		{
			Attacked = Building;
			break;
		}
	}
	if (!Attacked)
	{
		return;
	}
	for (AUnitBase* Unit : MyArmy(Econ))
	{
		if (Offensive.Contains(TWeakObjectPtr<AUnitBase>(Unit)))
		{
			continue; // наступающие не отзываются
		}
		ARTSAIController* Controller = Unit->GetRTSController();
		if (Controller && (Controller->IsIdle() || Controller->GetOrderType() == ERTSOrderType::Move))
		{
			Controller->SetOrder(FRTSOrder::MakeAttackMove(Attacked->GetActorLocation()));
		}
	}
}

// --- волны --------------------------------------------------------------------------

void AEnemyCommander::ManageWaves(URTSEconomySubsystem* Econ, URTSDataSubsystem* Data)
{
	// чистка списка наступающих: погибшие и вернувшиеся в idle
	Offensive.RemoveAll([](const TWeakObjectPtr<AUnitBase>& Weak)
	{
		AUnitBase* Unit = Weak.Get();
		if (!Unit)
		{
			return true;
		}
		ARTSAIController* Controller = Unit->GetRTSController();
		return !Controller || Controller->IsIdle();
	});

	if (Econ->GetMatchTime() < NextWaveAt)
	{
		return;
	}
	const FDifficultyRow& Diff = Data->GetDifficulty(Econ->GetDifficulty());

	TArray<AUnitBase*> Ready;
	for (AUnitBase* Unit : MyArmy(Econ))
	{
		if (!Offensive.Contains(TWeakObjectPtr<AUnitBase>(Unit)))
		{
			Ready.Add(Unit);
		}
	}
	if (Ready.Num() < FMath::Max(2, int32(WaveTarget * 0.6f)))
	{
		NextWaveAt = Econ->GetMatchTime() + 15.f; // армия не готова — короткая отсрочка
		return;
	}

	ABuildingBase* Target = Econ->FindNearestEnemyBuilding(AITeam, GetActorLocation());
	if (!Target)
	{
		return;
	}

	const int32 SquadSize = FMath::Min(WaveTarget, Ready.Num());
	for (int32 I = 0; I < SquadSize; ++I)
	{
		if (ARTSAIController* Controller = Ready[I]->GetRTSController())
		{
			Controller->SetOrder(FRTSOrder::MakeAttackMove(Target->GetActorLocation()));
			Offensive.Add(TWeakObjectPtr<AUnitBase>(Ready[I]));
		}
	}

	++WaveNumber;
	WaveTarget += Diff.WaveGrowth;
	NextWaveAt = Econ->GetMatchTime() + FMath::Max(45.f, Diff.WaveInterval - WaveNumber * 3.f);
	UE_LOG(LogRubezh, Log, TEXT("ИИ: волна %d из %d юнитов"), WaveNumber, SquadSize);
}
