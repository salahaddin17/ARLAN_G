#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "Data/RTSTypes.h"
#include "RTSGameMode.generated.h"

class ABuildingBase;
class AUnitBase;
class AEnemyCommander;

/**
 * Режим матча: настройка команд, спавн баз и складов, явный тик подсистем
 * (экономика → туман), выдача стартовых приказов, спавн ИИ-командира.
 * Фракция игрока и сложность — свойства (правятся в BP-наследнике
 * или прямо тут до сборки).
 */
UCLASS()
class RUBEZHARLAN_API ARTSGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ARTSGameMode();

	virtual void StartPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTS")
	ERTSFaction PlayerFaction = ERTSFaction::Legion;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTS")
	ERTSDifficulty Difficulty = ERTSDifficulty::Normal;

private:
	bool bInitialOrdersGiven = false;

	void SpawnWorldContent();
	ABuildingBase* SpawnBuildingAt(const FVector& Location, ERTSBuildingKind Kind, uint8 TeamId, bool bCompleted);
	AUnitBase* SpawnUnitAt(const FVector& Location, ERTSUnitKind Kind, uint8 TeamId);
	void SpawnDepotAt(const FVector& Location, float Amount);
	void GiveInitialOrders();
};
