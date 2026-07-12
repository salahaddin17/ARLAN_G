#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Data/RTSTypes.h"
#include "EnemyCommander.generated.h"

class AUnitBase;
class ABuildingBase;
class URTSEconomySubsystem;
class URTSDataSubsystem;

/**
 * ИИ противника (команда 1): билд-ордер → экономика грузовиками →
 * производство армии → волны нарастающей силы → оборона базы.
 * Работает только публичными приказами (SetOrder), юнитов не телепортирует.
 * Параметры темпа — из таблицы сложности.
 */
UCLASS()
class RUBEZHARLAN_API AEnemyCommander : public AActor
{
	GENERATED_BODY()

public:
	AEnemyCommander();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

private:
	static constexpr uint8 AITeam = 1;

	float ThinkAccum = 0.f;
	int32 BuildPlanIndex = 0;
	TArray<ERTSBuildingKind> BuildPlan;

	float NextWaveAt = 190.f;
	int32 WaveTarget = 5;
	int32 WaveNumber = 0;
	TArray<TWeakObjectPtr<AUnitBase>> Offensive;

	void Think();
	void ManageEconomy(URTSEconomySubsystem* Econ);
	void ManageConstruction(URTSEconomySubsystem* Econ, URTSDataSubsystem* Data);
	void ManageArmyProduction(URTSEconomySubsystem* Econ, URTSDataSubsystem* Data);
	void ManageDefense(URTSEconomySubsystem* Econ);
	void ManageWaves(URTSEconomySubsystem* Econ, URTSDataSubsystem* Data);

	ERTSBuildingKind NextBuildItem(URTSEconomySubsystem* Econ) const;
	bool FindBuildSpot(URTSEconomySubsystem* Econ, URTSDataSubsystem* Data,
	                   ERTSBuildingKind Kind, FVector& OutLocation) const;

	ABuildingBase* MyHQ(URTSEconomySubsystem* Econ) const;
	TArray<AUnitBase*> MyUnits(URTSEconomySubsystem* Econ, ERTSUnitKind Kind) const;
	TArray<AUnitBase*> MyArmy(URTSEconomySubsystem* Econ) const;
};
