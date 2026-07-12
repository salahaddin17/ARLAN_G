#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Data/RTSTypes.h"
#include "RTSProductionComponent.generated.h"

class ABuildingBase;

USTRUCT()
struct FRTSProductionItem
{
	GENERATED_BODY()

	UPROPERTY() ERTSUnitKind Kind = ERTSUnitKind::Rifleman;
	UPROPERTY() float Progress = 0.f;   // 0..1
	UPROPERTY() int32 PaidCost = 0;     // для возврата при отмене
};

/**
 * Очередь производства юнитов (макс 5). Скорость умножается на
 * энергетический множитель команды (дефицит → ×0.5). Готовый юнит
 * спавнится у выхода и получает приказ идти в точку сбора;
 * грузовики сразу отправляются возить припасы.
 */
UCLASS(ClassGroup = (RTS), meta = (BlueprintSpawnableComponent))
class RUBEZHARLAN_API URTSProductionComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	void InitFor(ABuildingBase* InBuilding);

	/** Ставит юнита в очередь, списывая цену. false — нет денег/места/не то здание. */
	bool TryEnqueue(ERTSUnitKind Kind);

	/** Отмена позиции с возвратом денег. */
	void CancelAt(int32 Index);

	void Step(float DeltaSeconds);

	const TArray<FRTSProductionItem>& GetQueue() const { return Queue; }
	bool CanProduceAnything() const;

	static constexpr int32 MaxQueue = 5;

private:
	UPROPERTY() TObjectPtr<ABuildingBase> Building;
	UPROPERTY() TArray<FRTSProductionItem> Queue;

	void SpawnCompleted(ERTSUnitKind Kind);
};
