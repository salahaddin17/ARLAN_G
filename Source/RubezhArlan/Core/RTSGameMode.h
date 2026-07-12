#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "Data/RTSTypes.h"
#include "RTSGameMode.generated.h"

class ABuildingBase;
class AUnitBase;
class AEnemyCommander;

UENUM(BlueprintType)
enum class ERTSMatchPhase : uint8
{
	Setup,    // стартовое меню: выбор фракции и сложности
	Playing,
	Ended,
};

/**
 * Режим матча: стартовое меню (Setup) → спавн баз и складов по подтверждению
 * → явный тик подсистем (экономика → туман) → финал и рестарт по R.
 */
UCLASS()
class RUBEZHARLAN_API ARTSGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ARTSGameMode();

	virtual void StartPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	// --- фаза матча и стартовое меню ------------------------------------
	ERTSMatchPhase GetPhase() const { return Phase; }
	void SetPlayerFaction(ERTSFaction Faction);
	void SetDifficulty(ERTSDifficulty InDifficulty);
	void ConfirmStart();
	void RestartMatch();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTS")
	ERTSFaction PlayerFaction = ERTSFaction::Legion;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTS")
	ERTSDifficulty Difficulty = ERTSDifficulty::Normal;

	/** Пропустить меню (автостарт матча) — удобно для автотестов. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTS")
	bool bSkipSetupMenu = false;

private:
	ERTSMatchPhase Phase = ERTSMatchPhase::Setup;
	bool bInitialOrdersGiven = false;

	void EnsureWorldDressing();
	void SpawnWorldContent();
	ABuildingBase* SpawnBuildingAt(const FVector& Location, ERTSBuildingKind Kind, uint8 TeamId, bool bCompleted);
	AUnitBase* SpawnUnitAt(const FVector& Location, ERTSUnitKind Kind, uint8 TeamId);
	void SpawnDepotAt(const FVector& Location, float Amount);
	void GiveInitialOrders();
};
