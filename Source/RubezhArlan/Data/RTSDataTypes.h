#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "Data/RTSTypes.h"
#include "RTSDataTypes.generated.h"

// ============================================================================
// Строки DataTable. Баланс живёт в Content/Data/*.csv; RTSDataSubsystem
// грузит их в рантайме (fallback — константы RTSBalanceCore).
// ============================================================================

USTRUCT(BlueprintType)
struct FUnitRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RTS") FString DisplayName;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RTS") int32 Cost = 100;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RTS") float MaxHp = 100.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RTS") float Speed = 200.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RTS") ERTSArmor Armor = ERTSArmor::INF;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RTS") int32 VisionTiles = 6;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RTS") float BuildTime = 8.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RTS") float Damage = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RTS") float Range = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RTS") float Cooldown = 1.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RTS") ERTSDamageType DamageType = ERTSDamageType::Bullet;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RTS") float SplashRadius = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RTS") float MinRange = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RTS") float ProjectileSpeed = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RTS") float CargoCapacity = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RTS") bool bInfantry = true;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RTS") ERTSBuildingKind ProducedAt = ERTSBuildingKind::HQ;
};

USTRUCT(BlueprintType)
struct FBuildingRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RTS") FString DisplayName;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RTS") int32 Cost = 200;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RTS") float MaxHp = 500.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RTS") int32 SizeX = 2;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RTS") int32 SizeY = 2;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RTS") float BuildTime = 15.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RTS") int32 VisionTiles = 5;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RTS") int32 PowerProduce = 0;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RTS") int32 PowerUse = 0;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RTS") float Damage = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RTS") float Range = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RTS") float Cooldown = 1.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RTS") ERTSDamageType DamageType = ERTSDamageType::Defense;
};

USTRUCT(BlueprintType)
struct FDamageMatrixRow : public FTableRowBase
{
	GENERATED_BODY()

	// Имя строки = тип урона (Bullet/Rocket/Shell/Defense)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RTS") float VsINF = 1.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RTS") float VsLGT = 1.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RTS") float VsHVY = 1.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RTS") float VsBLD = 1.f;
};

USTRUCT(BlueprintType)
struct FFactionRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RTS") FString DisplayName;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RTS") float CostMul = 1.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RTS") float HpMul = 1.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RTS") float SpeedMul = 1.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RTS") float DamageMul = 1.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RTS") FLinearColor Color = FLinearColor::White;
};

USTRUCT(BlueprintType)
struct FDifficultyRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RTS") FString DisplayName;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RTS") float IncomeMul = 1.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RTS") float FirstWaveAt = 190.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RTS") float WaveInterval = 90.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RTS") int32 WaveStart = 5;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RTS") int32 WaveGrowth = 3;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RTS") int32 ArmyCap = 22;
};
