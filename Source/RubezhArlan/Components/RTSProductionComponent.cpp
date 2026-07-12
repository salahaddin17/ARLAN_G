#include "Components/RTSProductionComponent.h"
#include "RubezhArlan.h"
#include "Buildings/BuildingBase.h"
#include "Units/UnitBase.h"
#include "Units/RTSAIController.h"
#include "Data/RTSDataSubsystem.h"
#include "Econ/RTSEconomySubsystem.h"
#include "Econ/SupplyDepot.h"
#include "Engine/World.h"

void URTSProductionComponent::InitFor(ABuildingBase* InBuilding)
{
	Building = InBuilding;
	Queue.Reset();
}

bool URTSProductionComponent::CanProduceAnything() const
{
	if (!Building)
	{
		return false;
	}
	for (uint8 K = 0; K < 7; ++K)
	{
		if (URTSDataSubsystem::BuildingProduces(Building->BuildingKind, ERTSUnitKind(K)))
		{
			return true;
		}
	}
	return false;
}

bool URTSProductionComponent::TryEnqueue(ERTSUnitKind Kind)
{
	if (!Building || !Building->IsCompleted() || Queue.Num() >= MaxQueue)
	{
		return false;
	}
	if (!URTSDataSubsystem::BuildingProduces(Building->BuildingKind, Kind))
	{
		return false;
	}
	UWorld* World = GetWorld();
	URTSEconomySubsystem* Econ = World ? World->GetSubsystem<URTSEconomySubsystem>() : nullptr;
	URTSDataSubsystem* Data = Building->GetData();
	if (!Econ || !Data)
	{
		return false;
	}

	const int32 Cost = Data->GetUnitCost(Kind, Building->Faction);
	if (!Econ->TrySpend(Building->TeamId, Cost))
	{
		return false;
	}

	FRTSProductionItem Item;
	Item.Kind = Kind;
	Item.PaidCost = Cost;
	Queue.Add(Item);
	return true;
}

void URTSProductionComponent::CancelAt(int32 Index)
{
	if (!Queue.IsValidIndex(Index) || !Building)
	{
		return;
	}
	if (UWorld* World = GetWorld())
	{
		if (URTSEconomySubsystem* Econ = World->GetSubsystem<URTSEconomySubsystem>())
		{
			Econ->AddCredits(Building->TeamId, Queue[Index].PaidCost);
		}
	}
	Queue.RemoveAt(Index);
}

void URTSProductionComponent::Step(float DeltaSeconds)
{
	if (!Building || Queue.Num() == 0)
	{
		return;
	}
	UWorld* World = GetWorld();
	URTSEconomySubsystem* Econ = World ? World->GetSubsystem<URTSEconomySubsystem>() : nullptr;
	URTSDataSubsystem* Data = Building->GetData();
	if (!Econ || !Data)
	{
		return;
	}

	FRTSProductionItem& Current = Queue[0];
	const float BuildTime = FMath::Max(0.5f, Data->GetUnit(Current.Kind).BuildTime);
	Current.Progress += (DeltaSeconds / BuildTime) * Econ->GetProductionMultiplier(Building->TeamId);

	if (Current.Progress >= 1.f)
	{
		const ERTSUnitKind Kind = Current.Kind;
		Queue.RemoveAt(0);
		SpawnCompleted(Kind);
	}
}

void URTSProductionComponent::SpawnCompleted(ERTSUnitKind Kind)
{
	UWorld* World = GetWorld();
	if (!World || !Building)
	{
		return;
	}

	// выход — сторона здания, обращённая к точке сбора
	const FVector2D Extents = Building->GetFootprintExtents();
	const FVector ExitDir = (Building->RallyPoint - Building->GetActorLocation()).GetSafeNormal2D();
	const FVector SpawnLoc = Building->GetActorLocation() +
		ExitDir * (Extents.Size() + 120.f) + FVector(0, 0, 100.f);

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
	AUnitBase* Unit = World->SpawnActor<AUnitBase>(SpawnLoc, FRotator::ZeroRotator, Params);
	if (!Unit)
	{
		return;
	}
	Unit->InitUnit(Kind, Building->TeamId);

	if (ARTSAIController* Controller = Unit->GetRTSController())
	{
		if (Kind == ERTSUnitKind::Truck)
		{
			// грузовик сразу в работу
			URTSEconomySubsystem* Econ = World->GetSubsystem<URTSEconomySubsystem>();
			ASupplyDepot* Depot = Econ ? Econ->FindNearestDepotWithSupplies(SpawnLoc) : nullptr;
			if (Depot)
			{
				Controller->SetOrder(FRTSOrder::MakeHarvest(Depot));
				return;
			}
		}
		Controller->SetOrder(FRTSOrder::MakeMove(Building->RallyPoint));
	}
}
