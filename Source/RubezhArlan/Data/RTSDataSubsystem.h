#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Data/RTSDataTypes.h"
#include "RTSDataSubsystem.generated.h"

/**
 * Единая точка доступа к балансу. При старте пытается загрузить CSV из
 * Content/Data/ (Units, Buildings, DamageMatrix, Factions, Difficulty);
 * всё, что не загрузилось, берётся из констант RTSBalanceCore — игра
 * работоспособна даже с пустым Content.
 */
UCLASS()
class RUBEZHARLAN_API URTSDataSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	UFUNCTION(BlueprintPure, Category = "RTS|Data")
	const FUnitRow& GetUnit(ERTSUnitKind Kind) const;

	UFUNCTION(BlueprintPure, Category = "RTS|Data")
	const FBuildingRow& GetBuilding(ERTSBuildingKind Kind) const;

	UFUNCTION(BlueprintPure, Category = "RTS|Data")
	const FFactionRow& GetFaction(ERTSFaction Faction) const;

	UFUNCTION(BlueprintPure, Category = "RTS|Data")
	const FDifficultyRow& GetDifficulty(ERTSDifficulty Difficulty) const;

	UFUNCTION(BlueprintPure, Category = "RTS|Data")
	float GetDamageMultiplier(ERTSDamageType DamageType, ERTSArmor Armor) const;

	// Производные величины с учётом фракции
	UFUNCTION(BlueprintPure, Category = "RTS|Data")
	int32 GetUnitCost(ERTSUnitKind Kind, ERTSFaction Faction) const;

	UFUNCTION(BlueprintPure, Category = "RTS|Data")
	int32 GetBuildingCost(ERTSBuildingKind Kind, ERTSFaction Faction) const;

	UFUNCTION(BlueprintPure, Category = "RTS|Data")
	float GetUnitMaxHp(ERTSUnitKind Kind, ERTSFaction Faction) const;

	UFUNCTION(BlueprintPure, Category = "RTS|Data")
	float GetBuildingMaxHp(ERTSBuildingKind Kind, ERTSFaction Faction) const;

	UFUNCTION(BlueprintPure, Category = "RTS|Data")
	float GetUnitSpeed(ERTSUnitKind Kind, ERTSFaction Faction) const;

	/** Урон с учётом фракции атакующего и брони цели. */
	UFUNCTION(BlueprintPure, Category = "RTS|Data")
	float ComputeDamage(float BaseDamage, ERTSFaction AttackerFaction,
	                    ERTSDamageType DamageType, ERTSArmor TargetArmor) const;

	static bool BuildingProduces(ERTSBuildingKind Building, ERTSUnitKind Unit);

private:
	FUnitRow Units[7];
	FBuildingRow Buildings[5];
	FFactionRow Factions[2];
	FDifficultyRow Difficulties[3];
	float Matrix[4][4];

	void LoadDefaultsFromCore();
	void TryOverrideFromCsv();
	UDataTable* LoadCsvTable(const FString& FileName, UScriptStruct* RowStruct) const;
};
