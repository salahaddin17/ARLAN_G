#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "Data/RTSTypes.h"
#include "RTSAIController.generated.h"

class AUnitBase;
class ASupplyDepot;
class ABuildingBase;

UENUM()
enum class ERTSHarvestPhase : uint8
{
	ToDepot, Loading, ToBase, Unloading,
};

/**
 * Исполнитель приказов юнита: move/attack-move/attack/harvest/build/stop.
 * Логика 1:1 с эталонным прототипом; движение — NavMesh (MoveToLocation).
 */
UCLASS()
class RUBEZHARLAN_API ARTSAIController : public AAIController
{
	GENERATED_BODY()

public:
	ARTSAIController();

	virtual void Tick(float DeltaSeconds) override;
	virtual void OnPossess(APawn* InPawn) override;

	void SetOrder(const FRTSOrder& NewOrder);
	const FRTSOrder& GetOrder() const { return CurrentOrder; }
	ERTSOrderType GetOrderType() const { return CurrentOrder.Type; }
	bool IsIdle() const { return CurrentOrder.Type == ERTSOrderType::Idle; }

private:
	FRTSOrder CurrentOrder;

	// движение
	bool bMoveIssued = false;
	float RepathAccum = 0.f;

	// авто-бой
	TWeakObjectPtr<AActor> EngageTarget;
	float ScanAccum = 0.f;

	// грузовик
	ERTSHarvestPhase HarvestPhase = ERTSHarvestPhase::ToDepot;
	float UnloadTimer = 0.f;

	AUnitBase* GetUnit() const;

	void TickIdle(AUnitBase* Unit, float DeltaSeconds);
	void TickMove(AUnitBase* Unit, float DeltaSeconds);
	void TickAttackMove(AUnitBase* Unit, float DeltaSeconds);
	void TickAttackTarget(AUnitBase* Unit, float DeltaSeconds);
	void TickHarvest(AUnitBase* Unit, float DeltaSeconds);
	void TickBuild(AUnitBase* Unit, float DeltaSeconds);

	/** Преследование и стрельба по цели; true — цель ещё актуальна. */
	bool ChaseAndFire(AUnitBase* Unit, AActor* Target, float DeltaSeconds);

	AActor* FindAutoTarget(AUnitBase* Unit, float Radius, bool bAllowBuildings);
	bool IsNear(const AActor* Target, float Radius) const;
	bool IsNearPoint(const FVector& Point, float Radius) const;
	void MoveToPoint(const FVector& Point, float AcceptanceRadius = 60.f);
};
