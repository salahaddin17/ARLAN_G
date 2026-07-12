#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Data/RTSTypes.h"
#include "RTSEconomySubsystem.generated.h"

class AUnitBase;
class ABuildingBase;
class ASupplyDepot;

USTRUCT(BlueprintType)
struct FRTSTeamStats
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, Category = "RTS") int32 UnitsBuilt = 0;
	UPROPERTY(VisibleAnywhere, Category = "RTS") int32 UnitsLost = 0;
	UPROPERTY(VisibleAnywhere, Category = "RTS") int32 BuildingsBuilt = 0;
	UPROPERTY(VisibleAnywhere, Category = "RTS") int32 BuildingsLost = 0;
	UPROPERTY(VisibleAnywhere, Category = "RTS") int32 SuppliesGathered = 0;
	UPROPERTY(VisibleAnywhere, Category = "RTS") int32 EnemiesKilled = 0;
};

/**
 * Центральный реестр матча: юниты/здания/склады, припасы и энергия команд,
 * статистика, проверка победы. Тикается ЯВНО из ARTSGameMode::Tick (Step) —
 * детерминированный порядок, как в эталонном прототипе.
 */
UCLASS()
class RUBEZHARLAN_API URTSEconomySubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	// --- настройка матча -----------------------------------------------------
	void SetupMatch(ERTSFaction PlayerFaction, ERTSDifficulty InDifficulty);

	ERTSFaction GetFactionOf(uint8 TeamId) const { return TeamFactions[TeamId & 1]; }
	ERTSDifficulty GetDifficulty() const { return Difficulty; }
	float GetMatchTime() const { return MatchTime; }

	// --- реестры ---------------------------------------------------------------
	void RegisterUnit(AUnitBase* Unit);
	void UnregisterUnit(AUnitBase* Unit);
	void RegisterBuilding(ABuildingBase* Building);
	void UnregisterBuilding(ABuildingBase* Building);
	void RegisterDepot(ASupplyDepot* Depot);
	void UnregisterDepot(ASupplyDepot* Depot);

	const TArray<AUnitBase*>& GetAllUnits() const { return Units; }
	const TArray<ABuildingBase*>& GetAllBuildings() const { return Buildings; }
	const TArray<ASupplyDepot*>& GetAllDepots() const { return Depots; }

	// --- припасы -----------------------------------------------------------------
	int32 GetCredits(uint8 TeamId) const { return Credits[TeamId & 1]; }
	void AddCredits(uint8 TeamId, int32 Amount);
	bool TrySpend(uint8 TeamId, int32 Amount);

	// --- энергия -------------------------------------------------------------------
	int32 GetPowerUse(uint8 TeamId) const { return PowerUse[TeamId & 1]; }
	int32 GetPowerProduce(uint8 TeamId) const { return PowerProduce[TeamId & 1]; }
	bool HasPowerDeficit(uint8 TeamId) const { return GetPowerUse(TeamId) > GetPowerProduce(TeamId); }
	float GetProductionMultiplier(uint8 TeamId) const;

	// --- статистика ------------------------------------------------------------------
	FRTSTeamStats& StatsOf(uint8 TeamId) { return Stats[TeamId & 1]; }
	const FRTSTeamStats& GetStats(uint8 TeamId) const { return Stats[TeamId & 1]; }

	// --- запросы (авто-таргет, экономика, ИИ) ------------------------------------------
	AUnitBase* FindNearestEnemyUnit(uint8 MyTeam, const FVector& From, float MaxRange) const;
	/** Юниты приоритетнее зданий; bRequireVisible учитывает туман для команды 0. */
	AActor* FindNearestEnemyTarget(uint8 MyTeam, const FVector& From, float MaxRange, bool bRequireVisible) const;
	ABuildingBase* FindNearestEnemyBuilding(uint8 MyTeam, const FVector& From) const;
	ASupplyDepot* FindNearestDepotWithSupplies(const FVector& From) const;
	ABuildingBase* FindNearestHQ(uint8 TeamId, const FVector& From) const;
	int32 CountUnitsOfKind(uint8 TeamId, ERTSUnitKind Kind) const;
	int32 CountBuildingsOfKind(uint8 TeamId, ERTSBuildingKind Kind) const;

	/** Свободна ли площадка под здание (пересечения зданий/складов, границы карты). */
	bool IsPlacementFree(const FVector& Center, const FVector2D& HalfExtents) const;

	// --- ход матча -----------------------------------------------------------------------
	void Step(float DeltaSeconds);
	bool IsMatchEnded() const { return bMatchEnded; }
	int32 GetWinnerTeam() const { return WinnerTeam; }

private:
	void RecomputePower();
	void CheckVictory();

	TArray<AUnitBase*> Units;
	TArray<ABuildingBase*> Buildings;
	TArray<ASupplyDepot*> Depots;

	int32 Credits[2] = {0, 0};
	int32 PowerUse[2] = {0, 0};
	int32 PowerProduce[2] = {0, 0};
	FRTSTeamStats Stats[2];
	ERTSFaction TeamFactions[2] = {ERTSFaction::Legion, ERTSFaction::Front};
	ERTSDifficulty Difficulty = ERTSDifficulty::Normal;

	float MatchTime = 0.f;
	float PowerAccum = 0.f;
	float VictoryAccum = 0.f;
	bool bMatchEnded = false;
	int32 WinnerTeam = -1;
};
