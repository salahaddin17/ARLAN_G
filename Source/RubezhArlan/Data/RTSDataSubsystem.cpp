#include "Data/RTSDataSubsystem.h"
#include "RubezhArlan.h"
#include "Engine/DataTable.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

namespace
{
	const TCHAR* UnitRowNames[7] = {
		TEXT("Technician"), TEXT("Truck"), TEXT("Rifleman"), TEXT("Rocketeer"),
		TEXT("Scout"), TEXT("Tank"), TEXT("Artillery"),
	};
	const TCHAR* BuildingRowNames[5] = {
		TEXT("HQ"), TEXT("Power"), TEXT("Barracks"), TEXT("Factory"), TEXT("Turret"),
	};
	const TCHAR* FactionRowNames[2] = { TEXT("Legion"), TEXT("Front") };
	const TCHAR* DifficultyRowNames[3] = { TEXT("Easy"), TEXT("Normal"), TEXT("Hard") };
	const TCHAR* DamageRowNames[4] = { TEXT("Bullet"), TEXT("Rocket"), TEXT("Shell"), TEXT("Defense") };
}

void URTSDataSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	UGameInstanceSubsystem::Initialize(Collection);
	LoadDefaultsFromCore();
	TryOverrideFromCsv();
	UE_LOG(LogRubezh, Log, TEXT("RTSDataSubsystem готов: юнитов %d, зданий %d"), 7, 5);
}

void URTSDataSubsystem::LoadDefaultsFromCore()
{
	using namespace RTSCore;

	for (uint8 i = 0; i < 7; ++i)
	{
		const FUnitStats& S = UnitStats[i];
		FUnitRow& R = Units[i];
		R.DisplayName = FString(S.Name);
		R.Cost = S.Cost;
		R.MaxHp = S.MaxHp;
		R.Speed = S.Speed;
		R.Armor = ERTSArmor(uint8(S.Armor));
		R.VisionTiles = S.VisionTiles;
		R.BuildTime = S.BuildTime;
		R.Damage = S.Damage;
		R.Range = S.Range;
		R.Cooldown = S.Cooldown > 0.f ? S.Cooldown : 1.f;
		R.DamageType = ERTSDamageType(uint8(S.DmgType));
		R.SplashRadius = S.SplashRadius;
		R.MinRange = S.MinRange;
		R.ProjectileSpeed = S.ProjectileSpeed;
		R.CargoCapacity = S.CargoCapacity;
		R.bInfantry = S.bInfantry;
		R.ProducedAt = ERTSBuildingKind(uint8(S.ProducedAt));
	}

	for (uint8 i = 0; i < 5; ++i)
	{
		const FBuildingStats& S = BuildingStats[i];
		FBuildingRow& R = Buildings[i];
		R.DisplayName = FString(S.Name);
		R.Cost = S.Cost;
		R.MaxHp = S.MaxHp;
		R.SizeX = S.SizeX;
		R.SizeY = S.SizeY;
		R.BuildTime = S.BuildTime;
		R.VisionTiles = S.VisionTiles;
		R.PowerProduce = S.PowerProduce;
		R.PowerUse = S.PowerUse;
		R.Damage = S.Damage;
		R.Range = S.Range;
		R.Cooldown = S.Cooldown > 0.f ? S.Cooldown : 1.f;
		R.DamageType = ERTSDamageType(uint8(S.DmgType));
	}

	for (uint8 f = 0; f < 2; ++f)
	{
		const FFactionMods& M = FactionMods[f];
		FFactionRow& R = Factions[f];
		R.CostMul = M.CostMul;
		R.HpMul = M.HpMul;
		R.SpeedMul = M.SpeedMul;
		R.DamageMul = M.DamageMul;
	}
	Factions[0].DisplayName = TEXT("Легион");
	Factions[0].Color = FLinearColor(0.055f, 0.561f, 0.514f);
	Factions[1].DisplayName = TEXT("Вольный фронт");
	Factions[1].Color = FLinearColor(0.851f, 0.373f, 0.094f);

	for (uint8 d = 0; d < 3; ++d)
	{
		const FDifficultyStats& S = DifficultyStats[d];
		FDifficultyRow& R = Difficulties[d];
		R.DisplayName = FString(S.Name);
		R.IncomeMul = S.IncomeMul;
		R.FirstWaveAt = S.FirstWaveAt;
		R.WaveInterval = S.WaveInterval;
		R.WaveStart = S.WaveStart;
		R.WaveGrowth = S.WaveGrowth;
		R.ArmyCap = S.ArmyCap;
	}

	for (uint8 d = 0; d < 4; ++d)
	{
		for (uint8 a = 0; a < 4; ++a)
		{
			Matrix[d][a] = DamageMatrix[d][a];
		}
	}
}

UDataTable* URTSDataSubsystem::LoadCsvTable(const FString& FileName, UScriptStruct* RowStruct) const
{
	const FString Path = FPaths::ProjectContentDir() / TEXT("Data") / FileName;
	FString Csv;
	if (!FFileHelper::LoadFileToString(Csv, *Path))
	{
		UE_LOG(LogRubezh, Warning, TEXT("CSV не найден: %s — использую константы ядра"), *Path);
		return nullptr;
	}
	UDataTable* Table = NewObject<UDataTable>(const_cast<URTSDataSubsystem*>(this));
	Table->RowStruct = RowStruct;
	const TArray<FString> Problems = Table->CreateTableFromCSVString(Csv);
	for (const FString& P : Problems)
	{
		UE_LOG(LogRubezh, Warning, TEXT("CSV %s: %s"), *FileName, *P);
	}
	return Table;
}

void URTSDataSubsystem::TryOverrideFromCsv()
{
	if (UDataTable* T = LoadCsvTable(TEXT("Units.csv"), FUnitRow::StaticStruct()))
	{
		for (uint8 i = 0; i < 7; ++i)
		{
			if (const FUnitRow* Row = T->FindRow<FUnitRow>(UnitRowNames[i], TEXT("Units"), false))
			{
				Units[i] = *Row;
			}
		}
	}
	if (UDataTable* T = LoadCsvTable(TEXT("Buildings.csv"), FBuildingRow::StaticStruct()))
	{
		for (uint8 i = 0; i < 5; ++i)
		{
			if (const FBuildingRow* Row = T->FindRow<FBuildingRow>(BuildingRowNames[i], TEXT("Buildings"), false))
			{
				Buildings[i] = *Row;
			}
		}
	}
	if (UDataTable* T = LoadCsvTable(TEXT("DamageMatrix.csv"), FDamageMatrixRow::StaticStruct()))
	{
		for (uint8 d = 0; d < 4; ++d)
		{
			if (const FDamageMatrixRow* Row = T->FindRow<FDamageMatrixRow>(DamageRowNames[d], TEXT("Matrix"), false))
			{
				Matrix[d][0] = Row->VsINF;
				Matrix[d][1] = Row->VsLGT;
				Matrix[d][2] = Row->VsHVY;
				Matrix[d][3] = Row->VsBLD;
			}
		}
	}
	if (UDataTable* T = LoadCsvTable(TEXT("Factions.csv"), FFactionRow::StaticStruct()))
	{
		for (uint8 i = 0; i < 2; ++i)
		{
			if (const FFactionRow* Row = T->FindRow<FFactionRow>(FactionRowNames[i], TEXT("Factions"), false))
			{
				Factions[i] = *Row;
			}
		}
	}
	if (UDataTable* T = LoadCsvTable(TEXT("Difficulty.csv"), FDifficultyRow::StaticStruct()))
	{
		for (uint8 i = 0; i < 3; ++i)
		{
			if (const FDifficultyRow* Row = T->FindRow<FDifficultyRow>(DifficultyRowNames[i], TEXT("Difficulty"), false))
			{
				Difficulties[i] = *Row;
			}
		}
	}
}

const FUnitRow& URTSDataSubsystem::GetUnit(ERTSUnitKind Kind) const
{
	return Units[FMath::Clamp<uint8>(uint8(Kind), 0, 6)];
}

const FBuildingRow& URTSDataSubsystem::GetBuilding(ERTSBuildingKind Kind) const
{
	return Buildings[FMath::Clamp<uint8>(uint8(Kind), 0, 4)];
}

const FFactionRow& URTSDataSubsystem::GetFaction(ERTSFaction Faction) const
{
	return Factions[FMath::Clamp<uint8>(uint8(Faction), 0, 1)];
}

const FDifficultyRow& URTSDataSubsystem::GetDifficulty(ERTSDifficulty Difficulty) const
{
	return Difficulties[FMath::Clamp<uint8>(uint8(Difficulty), 0, 2)];
}

float URTSDataSubsystem::GetDamageMultiplier(ERTSDamageType DamageType, ERTSArmor Armor) const
{
	return Matrix[FMath::Clamp<uint8>(uint8(DamageType), 0, 3)][FMath::Clamp<uint8>(uint8(Armor), 0, 3)];
}

int32 URTSDataSubsystem::GetUnitCost(ERTSUnitKind Kind, ERTSFaction Faction) const
{
	return FMath::RoundToInt(GetUnit(Kind).Cost * GetFaction(Faction).CostMul);
}

int32 URTSDataSubsystem::GetBuildingCost(ERTSBuildingKind Kind, ERTSFaction Faction) const
{
	return FMath::RoundToInt(GetBuilding(Kind).Cost * GetFaction(Faction).CostMul);
}

float URTSDataSubsystem::GetUnitMaxHp(ERTSUnitKind Kind, ERTSFaction Faction) const
{
	return GetUnit(Kind).MaxHp * GetFaction(Faction).HpMul;
}

float URTSDataSubsystem::GetBuildingMaxHp(ERTSBuildingKind Kind, ERTSFaction Faction) const
{
	return GetBuilding(Kind).MaxHp * GetFaction(Faction).HpMul;
}

float URTSDataSubsystem::GetUnitSpeed(ERTSUnitKind Kind, ERTSFaction Faction) const
{
	return GetUnit(Kind).Speed * GetFaction(Faction).SpeedMul;
}

float URTSDataSubsystem::ComputeDamage(float BaseDamage, ERTSFaction AttackerFaction,
                                       ERTSDamageType DamageType, ERTSArmor TargetArmor) const
{
	return BaseDamage * GetFaction(AttackerFaction).DamageMul * GetDamageMultiplier(DamageType, TargetArmor);
}

bool URTSDataSubsystem::BuildingProduces(ERTSBuildingKind Building, ERTSUnitKind Unit)
{
	return RTSCore::Produces(ToCore(Building), ToCore(Unit));
}
