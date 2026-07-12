#include "Units/RTSAIController.h"
#include "RubezhArlan.h"
#include "Units/UnitBase.h"
#include "Buildings/BuildingBase.h"
#include "Econ/SupplyDepot.h"
#include "Econ/RTSEconomySubsystem.h"
#include "Components/RTSHealthComponent.h"
#include "Components/RTSWeaponComponent.h"
#include "Data/RTSDataSubsystem.h"
#include "Fog/RTSFogSubsystem.h"
#include "Engine/World.h"

ARTSAIController::ARTSAIController()
{
	PrimaryActorTick.bCanEverTick = true;
}

void ARTSAIController::OnPossess(APawn* InPawn)
{
	AAIController::OnPossess(InPawn);
	CurrentOrder = FRTSOrder::MakeIdle();
}

AUnitBase* ARTSAIController::GetUnit() const
{
	return Cast<AUnitBase>(GetPawn());
}

void ARTSAIController::SetOrder(const FRTSOrder& NewOrder)
{
	CurrentOrder = NewOrder;
	bMoveIssued = false;
	RepathAccum = 0.f;
	EngageTarget.Reset();
	UnloadTimer = 0.f;

	if (NewOrder.Type == ERTSOrderType::Stop || NewOrder.Type == ERTSOrderType::Idle)
	{
		StopMovement();
		CurrentOrder = FRTSOrder::MakeIdle();
	}
	if (NewOrder.Type == ERTSOrderType::Harvest)
	{
		AUnitBase* Unit = GetUnit();
		HarvestPhase = (Unit && Unit->Cargo >= RTSCore::Econ::TruckCapacity - 0.01f)
			? ERTSHarvestPhase::ToBase : ERTSHarvestPhase::ToDepot;
	}
}

void ARTSAIController::Tick(float DeltaSeconds)
{
	AAIController::Tick(DeltaSeconds);

	AUnitBase* Unit = GetUnit();
	if (!Unit || !Unit->Health || !Unit->Health->IsAlive())
	{
		return;
	}

	switch (CurrentOrder.Type)
	{
	case ERTSOrderType::Idle:         TickIdle(Unit, DeltaSeconds); break;
	case ERTSOrderType::Move:         TickMove(Unit, DeltaSeconds); break;
	case ERTSOrderType::AttackMove:   TickAttackMove(Unit, DeltaSeconds); break;
	case ERTSOrderType::AttackTarget: TickAttackTarget(Unit, DeltaSeconds); break;
	case ERTSOrderType::Harvest:      TickHarvest(Unit, DeltaSeconds); break;
	case ERTSOrderType::Build:        TickBuild(Unit, DeltaSeconds); break;
	default: break;
	}
}

// --- вспомогательное -----------------------------------------------------------

bool ARTSAIController::IsNear(const AActor* Target, float Radius) const
{
	const APawn* MyPawn = GetPawn();
	if (!MyPawn || !Target)
	{
		return false;
	}
	return FVector::Dist2D(MyPawn->GetActorLocation(), Target->GetActorLocation()) <= Radius;
}

bool ARTSAIController::IsNearPoint(const FVector& Point, float Radius) const
{
	const APawn* MyPawn = GetPawn();
	return MyPawn && FVector::Dist2D(MyPawn->GetActorLocation(), Point) <= Radius;
}

void ARTSAIController::MoveToPoint(const FVector& Point, float AcceptanceRadius)
{
	MoveToLocation(Point, AcceptanceRadius, true, true, true);
	bMoveIssued = true;
}

AActor* ARTSAIController::FindAutoTarget(AUnitBase* Unit, float Radius, bool bAllowBuildings)
{
	UWorld* World = GetWorld();
	URTSEconomySubsystem* Econ = World ? World->GetSubsystem<URTSEconomySubsystem>() : nullptr;
	if (!Econ)
	{
		return nullptr;
	}
	if (bAllowBuildings)
	{
		return Econ->FindNearestEnemyTarget(Unit->TeamId, Unit->GetActorLocation(), Radius, true);
	}
	AUnitBase* Enemy = Econ->FindNearestEnemyUnit(Unit->TeamId, Unit->GetActorLocation(), Radius);
	if (Enemy && Unit->TeamId == 0)
	{
		if (const URTSFogSubsystem* Fog = World->GetSubsystem<URTSFogSubsystem>())
		{
			if (!Fog->IsVisibleFor(0, Enemy->GetActorLocation()))
			{
				return nullptr;
			}
		}
	}
	return Enemy;
}

// --- состояния --------------------------------------------------------------------

void ARTSAIController::TickIdle(AUnitBase* Unit, float DeltaSeconds)
{
	if (!Unit->IsCombatUnit())
	{
		return;
	}
	// стоя на месте, бьём врага в радиусе оружия (без погони)
	ScanAccum += DeltaSeconds;
	AActor* Target = EngageTarget.Get();
	if (!IsValid(Target) || !Unit->Weapon->InRangeOf(Target))
	{
		if (ScanAccum < 0.4f)
		{
			return;
		}
		ScanAccum = 0.f;
		Target = FindAutoTarget(Unit, Unit->Weapon->GetRange() + 50.f, false);
		EngageTarget = Target;
	}
	if (IsValid(Target))
	{
		Unit->Weapon->TryFireAt(Target);
	}
}

void ARTSAIController::TickMove(AUnitBase* Unit, float DeltaSeconds)
{
	if (!bMoveIssued)
	{
		MoveToPoint(CurrentOrder.TargetLocation);
	}
	if (IsNearPoint(CurrentOrder.TargetLocation, 130.f))
	{
		StopMovement();
		SetOrder(FRTSOrder::MakeIdle());
	}
	(void)DeltaSeconds;
}

void ARTSAIController::TickAttackMove(AUnitBase* Unit, float DeltaSeconds)
{
	if (Unit->IsCombatUnit())
	{
		AActor* Target = EngageTarget.Get();
		ScanAccum += DeltaSeconds;
		const float Aggro = FMath::Max(Unit->Weapon->GetRange() * 1.2f, 550.f);
		if (!IsValid(Target) && ScanAccum >= 0.35f)
		{
			ScanAccum = 0.f;
			Target = FindAutoTarget(Unit, Aggro, true);
			EngageTarget = Target;
			if (Target)
			{
				bMoveIssued = false; // переключаемся с марша на бой
			}
		}
		if (IsValid(Target))
		{
			if (ChaseAndFire(Unit, Target, DeltaSeconds))
			{
				return;
			}
			// цель умерла/ушла — продолжаем марш
			EngageTarget.Reset();
			bMoveIssued = false;
		}
	}

	if (!bMoveIssued)
	{
		MoveToPoint(CurrentOrder.TargetLocation);
	}
	if (IsNearPoint(CurrentOrder.TargetLocation, 150.f))
	{
		StopMovement();
		SetOrder(FRTSOrder::MakeIdle());
	}
}

void ARTSAIController::TickAttackTarget(AUnitBase* Unit, float DeltaSeconds)
{
	AActor* Target = CurrentOrder.TargetActor.Get();
	if (!IsValid(Target) || !Unit->IsCombatUnit() || !ChaseAndFire(Unit, Target, DeltaSeconds))
	{
		SetOrder(FRTSOrder::MakeIdle());
	}
}

bool ARTSAIController::ChaseAndFire(AUnitBase* Unit, AActor* Target, float DeltaSeconds)
{
	URTSHealthComponent* TargetHealth = URTSWeaponComponent::FindTargetHealth(Target);
	if (!TargetHealth || !TargetHealth->IsAlive())
	{
		return false;
	}

	URTSWeaponComponent* Weapon = Unit->Weapon;
	const float Dist = Weapon->DistanceTo(Target);

	if (Dist < Weapon->GetMinRange())
	{
		// артиллерия пятится от слишком близкой цели
		RepathAccum += DeltaSeconds;
		if (RepathAccum >= 0.5f)
		{
			RepathAccum = 0.f;
			const FVector Away = (Unit->GetActorLocation() - Target->GetActorLocation()).GetSafeNormal2D();
			MoveToPoint(Unit->GetActorLocation() + Away * (Weapon->GetMinRange() - Dist + 150.f), 40.f);
		}
		return true;
	}

	if (Dist <= Weapon->GetRange())
	{
		StopMovement();
		bMoveIssued = false;
		const ERTSFireResult Result = Weapon->TryFireAt(Target);
		// в туман не стреляем и не пялимся — цель потеряна
		return Result != ERTSFireResult::NotVisible && Result != ERTSFireResult::Invalid;
	}

	// догоняем; перепрокладка раз в 0.7 c
	RepathAccum += DeltaSeconds;
	if (!bMoveIssued || RepathAccum >= 0.7f)
	{
		RepathAccum = 0.f;
		MoveToPoint(Target->GetActorLocation(), FMath::Max(60.f, Weapon->GetRange() * 0.8f));
	}
	return true;
}

// --- грузовик ----------------------------------------------------------------------

void ARTSAIController::TickHarvest(AUnitBase* Unit, float DeltaSeconds)
{
	UWorld* World = GetWorld();
	URTSEconomySubsystem* Econ = World ? World->GetSubsystem<URTSEconomySubsystem>() : nullptr;
	if (!Econ || Unit->UnitKind != ERTSUnitKind::Truck)
	{
		SetOrder(FRTSOrder::MakeIdle());
		return;
	}
	const float Capacity = RTSCore::Econ::TruckCapacity;

	switch (HarvestPhase)
	{
	case ERTSHarvestPhase::ToDepot:
	{
		ASupplyDepot* Depot = Cast<ASupplyDepot>(CurrentOrder.TargetActor.Get());
		if (!IsValid(Depot) || !Depot->HasSupplies())
		{
			Depot = Econ->FindNearestDepotWithSupplies(Unit->GetActorLocation());
			if (!Depot)
			{
				SetOrder(FRTSOrder::MakeIdle());
				return;
			}
			CurrentOrder.TargetActor = Depot;
			bMoveIssued = false;
		}
		if (IsNear(Depot, 260.f))
		{
			StopMovement();
			HarvestPhase = ERTSHarvestPhase::Loading;
		}
		else if (!bMoveIssued)
		{
			MoveToPoint(Depot->GetActorLocation(), 180.f);
		}
		break;
	}
	case ERTSHarvestPhase::Loading:
	{
		ASupplyDepot* Depot = Cast<ASupplyDepot>(CurrentOrder.TargetActor.Get());
		if (!IsValid(Depot) || !Depot->HasSupplies())
		{
			HarvestPhase = Unit->Cargo > 0.f ? ERTSHarvestPhase::ToBase : ERTSHarvestPhase::ToDepot;
			bMoveIssued = false;
			return;
		}
		const float Want = FMath::Min(RTSCore::Econ::LoadRate * DeltaSeconds, Capacity - Unit->Cargo);
		Unit->Cargo += Depot->TakeSupplies(Want);
		if (Unit->Cargo >= Capacity - 0.01f)
		{
			Unit->Cargo = Capacity;
			HarvestPhase = ERTSHarvestPhase::ToBase;
			bMoveIssued = false;
		}
		break;
	}
	case ERTSHarvestPhase::ToBase:
	{
		ABuildingBase* HQ = Econ->FindNearestHQ(Unit->TeamId, Unit->GetActorLocation());
		if (!HQ)
		{
			SetOrder(FRTSOrder::MakeIdle());
			return;
		}
		if (IsNear(HQ, HQ->GetFootprintExtents().Size() + 200.f))
		{
			StopMovement();
			HarvestPhase = ERTSHarvestPhase::Unloading;
			UnloadTimer = RTSCore::Econ::UnloadTime;
		}
		else if (!bMoveIssued)
		{
			MoveToPoint(HQ->GetActorLocation(), HQ->GetFootprintExtents().Size() + 150.f);
		}
		break;
	}
	case ERTSHarvestPhase::Unloading:
	{
		UnloadTimer -= DeltaSeconds;
		if (UnloadTimer > 0.f)
		{
			return;
		}
		float IncomeMul = 1.f;
		if (Unit->TeamId == 1)
		{
			if (URTSDataSubsystem* Data = Unit->GetData())
			{
				IncomeMul = Data->GetDifficulty(Econ->GetDifficulty()).IncomeMul;
			}
		}
		const int32 Gained = FMath::RoundToInt(Unit->Cargo * IncomeMul);
		Econ->AddCredits(Unit->TeamId, Gained);
		Econ->StatsOf(Unit->TeamId).SuppliesGathered += Gained;
		Unit->Cargo = 0.f;
		HarvestPhase = ERTSHarvestPhase::ToDepot;
		bMoveIssued = false;
		break;
	}
	}
}

// --- техник строит --------------------------------------------------------------------

void ARTSAIController::TickBuild(AUnitBase* Unit, float DeltaSeconds)
{
	ABuildingBase* Site = Cast<ABuildingBase>(CurrentOrder.TargetActor.Get());
	if (!IsValid(Site) || Site->TeamId != Unit->TeamId || Unit->UnitKind != ERTSUnitKind::Technician)
	{
		SetOrder(FRTSOrder::MakeIdle());
		return;
	}
	const bool bDone = Site->IsCompleted() && !Site->NeedsRepair();
	if (bDone)
	{
		SetOrder(FRTSOrder::MakeIdle());
		return;
	}

	const float WorkRange = Site->GetFootprintExtents().Size() + 160.f;
	if (IsNear(Site, WorkRange))
	{
		StopMovement();
		bMoveIssued = false;
		if (!Site->IsCompleted())
		{
			Site->AddConstructionProgress(DeltaSeconds);
		}
		else
		{
			Site->RepairTick(DeltaSeconds);
		}
	}
	else if (!bMoveIssued)
	{
		MoveToPoint(Site->GetActorLocation(), WorkRange - 60.f);
	}
}
