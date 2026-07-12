#pragma once

#include "CoreMinimal.h"
#include "Core/RTSBalanceCore.h"
#include "RTSTypes.generated.h"

// ============================================================================
// UE-обёртки перечислений ядра. Порядок значений обязан 1:1 совпадать
// с RTSCore (проверяется static_assert'ами внизу).
// ============================================================================

UENUM(BlueprintType)
enum class ERTSDamageType : uint8
{
	Bullet = 0 UMETA(DisplayName = "Пули"),
	Rocket     UMETA(DisplayName = "Ракеты"),
	Shell      UMETA(DisplayName = "Снаряды"),
	Defense    UMETA(DisplayName = "Оборонительный"),
};

UENUM(BlueprintType)
enum class ERTSArmor : uint8
{
	INF = 0 UMETA(DisplayName = "Пехота"),
	LGT     UMETA(DisplayName = "Лёгкая броня"),
	HVY     UMETA(DisplayName = "Тяжёлая броня"),
	BLD     UMETA(DisplayName = "Здание"),
};

UENUM(BlueprintType)
enum class ERTSFaction : uint8
{
	Legion = 0 UMETA(DisplayName = "Легион"),
	Front      UMETA(DisplayName = "Вольный фронт"),
};

UENUM(BlueprintType)
enum class ERTSUnitKind : uint8
{
	Technician = 0 UMETA(DisplayName = "Техник"),
	Truck          UMETA(DisplayName = "Грузовик"),
	Rifleman       UMETA(DisplayName = "Пехотинец"),
	Rocketeer      UMETA(DisplayName = "Ракетчик"),
	Scout          UMETA(DisplayName = "Разведмашина"),
	Tank           UMETA(DisplayName = "Танк"),
	Artillery      UMETA(DisplayName = "Артиллерия"),
};

UENUM(BlueprintType)
enum class ERTSBuildingKind : uint8
{
	HQ = 0    UMETA(DisplayName = "Командный центр"),
	Power     UMETA(DisplayName = "Энергостанция"),
	Barracks  UMETA(DisplayName = "Казармы"),
	Factory   UMETA(DisplayName = "Завод техники"),
	Turret    UMETA(DisplayName = "Турель"),
};

UENUM(BlueprintType)
enum class ERTSDifficulty : uint8
{
	Easy = 0 UMETA(DisplayName = "Рекрут"),
	Normal   UMETA(DisplayName = "Ветеран"),
	Hard     UMETA(DisplayName = "Комиссар"),
};

UENUM(BlueprintType)
enum class ERTSOrderType : uint8
{
	Idle = 0,
	Move,
	AttackMove,
	AttackTarget,
	Harvest,
	Build,
	Stop,
};

/** Приказ юниту: тип + точка и/или целевой актор. */
USTRUCT(BlueprintType)
struct FRTSOrder
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTS")
	ERTSOrderType Type = ERTSOrderType::Idle;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTS")
	FVector TargetLocation = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTS")
	TWeakObjectPtr<AActor> TargetActor;

	static FRTSOrder MakeIdle() { return FRTSOrder{}; }
	static FRTSOrder MakeMove(const FVector& Loc)
	{
		FRTSOrder O; O.Type = ERTSOrderType::Move; O.TargetLocation = Loc; return O;
	}
	static FRTSOrder MakeAttackMove(const FVector& Loc)
	{
		FRTSOrder O; O.Type = ERTSOrderType::AttackMove; O.TargetLocation = Loc; return O;
	}
	static FRTSOrder MakeAttackTarget(AActor* Target)
	{
		FRTSOrder O; O.Type = ERTSOrderType::AttackTarget; O.TargetActor = Target; return O;
	}
	static FRTSOrder MakeHarvest(AActor* Depot)
	{
		FRTSOrder O; O.Type = ERTSOrderType::Harvest; O.TargetActor = Depot; return O;
	}
	static FRTSOrder MakeBuild(AActor* Site)
	{
		FRTSOrder O; O.Type = ERTSOrderType::Build; O.TargetActor = Site; return O;
	}
};

// --- Мосты UE-enum <-> ядро -------------------------------------------------

inline RTSCore::EDmg ToCore(ERTSDamageType V) { return RTSCore::EDmg(uint8(V)); }
inline RTSCore::EArmor ToCore(ERTSArmor V) { return RTSCore::EArmor(uint8(V)); }
inline RTSCore::EFaction ToCore(ERTSFaction V) { return RTSCore::EFaction(uint8(V)); }
inline RTSCore::EUnit ToCore(ERTSUnitKind V) { return RTSCore::EUnit(uint8(V)); }
inline RTSCore::EBuilding ToCore(ERTSBuildingKind V) { return RTSCore::EBuilding(uint8(V)); }
inline RTSCore::EDifficulty ToCore(ERTSDifficulty V) { return RTSCore::EDifficulty(uint8(V)); }

static_assert(uint8(RTSCore::EDmg::COUNT) == 4, "damage enum drift");
static_assert(uint8(RTSCore::EArmor::COUNT) == 4, "armor enum drift");
static_assert(uint8(RTSCore::EUnit::COUNT) == 7, "unit enum drift");
static_assert(uint8(RTSCore::EBuilding::COUNT) == 5, "building enum drift");
static_assert(uint8(RTSCore::EDifficulty::COUNT) == 3, "difficulty enum drift");
