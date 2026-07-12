#include "Buildings/BuildingBase.h"
#include "RubezhArlan.h"
#include "Components/RTSHealthComponent.h"
#include "Components/RTSWeaponComponent.h"
#include "Components/RTSProductionComponent.h"
#include "Data/RTSDataSubsystem.h"
#include "Econ/RTSEconomySubsystem.h"
#include "Units/UnitBase.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "UObject/ConstructorHelpers.h"
#include "DrawDebugHelpers.h"

namespace
{
	// высоты корпусов по типам, UU
	constexpr float BuildingHeights[5] = {320.f, 220.f, 250.f, 300.f, 260.f};
}

ABuildingBase::ABuildingBase()
{
	PrimaryActorTick.bCanEverTick = true;

	BodyMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BodyMesh"));
	SetRootComponent(BodyMesh);
	BodyMesh->SetCollisionProfileName(TEXT("BlockAll"));

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeFinder(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeFinder.Succeeded())
	{
		BodyMesh->SetStaticMesh(CubeFinder.Object);
	}

	Health = CreateDefaultSubobject<URTSHealthComponent>(TEXT("Health"));
	Weapon = CreateDefaultSubobject<URTSWeaponComponent>(TEXT("Weapon"));
	Production = CreateDefaultSubobject<URTSProductionComponent>(TEXT("Production"));
}

void ABuildingBase::BeginPlay()
{
	AActor::BeginPlay();
	if (UWorld* World = GetWorld())
	{
		if (URTSEconomySubsystem* Econ = World->GetSubsystem<URTSEconomySubsystem>())
		{
			Econ->RegisterBuilding(this);
		}
	}
}

void ABuildingBase::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		if (URTSEconomySubsystem* Econ = World->GetSubsystem<URTSEconomySubsystem>())
		{
			Econ->UnregisterBuilding(this);
		}
	}
	AActor::EndPlay(EndPlayReason);
}

URTSDataSubsystem* ABuildingBase::GetData() const
{
	const UWorld* World = GetWorld();
	if (!World || !World->GetGameInstance())
	{
		return nullptr;
	}
	return World->GetGameInstance()->GetSubsystem<URTSDataSubsystem>();
}

void ABuildingBase::InitBuilding(ERTSBuildingKind InKind, uint8 InTeamId, bool bCompleted)
{
	BuildingKind = InKind;
	TeamId = InTeamId;

	URTSDataSubsystem* Data = GetData();
	UWorld* World = GetWorld();
	URTSEconomySubsystem* Econ = World ? World->GetSubsystem<URTSEconomySubsystem>() : nullptr;
	Faction = Econ ? Econ->GetFactionOf(TeamId)
	               : (TeamId == 0 ? ERTSFaction::Legion : ERTSFaction::Front);
	if (!Data)
	{
		return;
	}

	const FBuildingRow& Row = Data->GetBuilding(BuildingKind);
	const float MaxHp = Data->GetBuildingMaxHp(BuildingKind, Faction);
	Health->Init(MaxHp, ERTSArmor::BLD, TeamId);

	BuildProgress = bCompleted ? 1.f : 0.f;
	if (!bCompleted)
	{
		// площадка начинается с 10% прочности и добирает её в ходе стройки
		Health->AddHp(-(MaxHp * 0.9f));
	}

	if (Row.Damage > 0.f)
	{
		Weapon->Init(Row.Damage, Row.Range, Row.Cooldown, Row.DamageType,
		             0.f, 0.f, 0.f, TeamId, Faction);
	}
	Production->InitFor(this);

	// точка сбора — от здания к центру карты
	const FVector ToCenter = (FVector::ZeroVector - GetActorLocation()).GetSafeNormal2D();
	RallyPoint = GetActorLocation() + ToCenter * (GetFootprintExtents().Size() + 300.f);

	if (bCompleted && Econ)
	{
		Econ->StatsOf(TeamId).BuildingsBuilt++;
	}
	SetupVisual();
}

FVector2D ABuildingBase::GetFootprintExtents() const
{
	if (const URTSDataSubsystem* Data = GetData())
	{
		const FBuildingRow& Row = Data->GetBuilding(BuildingKind);
		return FVector2D(Row.SizeX * RTSCore::UUPerTile * 0.5f, Row.SizeY * RTSCore::UUPerTile * 0.5f);
	}
	return FVector2D(100.f, 100.f);
}

int32 ABuildingBase::GetPowerUse() const
{
	const URTSDataSubsystem* Data = GetData();
	return Data ? Data->GetBuilding(BuildingKind).PowerUse : 0;
}

int32 ABuildingBase::GetPowerProduce() const
{
	const URTSDataSubsystem* Data = GetData();
	return Data ? Data->GetBuilding(BuildingKind).PowerProduce : 0;
}

bool ABuildingBase::NeedsRepair() const
{
	return IsCompleted() && Health && Health->IsAlive() && Health->GetHp() < Health->GetMaxHp() - 0.5f;
}

void ABuildingBase::SetupVisual()
{
	URTSDataSubsystem* Data = GetData();
	if (!Data || !BodyMesh)
	{
		return;
	}
	const FBuildingRow& Row = Data->GetBuilding(BuildingKind);
	const float H = BuildingHeights[FMath::Clamp<int32>(int32(BuildingKind), 0, 4)];
	// куб 100×100×100 → скейл в клетки
	BodyMesh->SetRelativeScale3D(FVector(Row.SizeX, Row.SizeY, H / 100.f));
	UpdateConstructionVisual();
}

void ABuildingBase::UpdateConstructionVisual()
{
	URTSDataSubsystem* Data = GetData();
	if (!Data || !BodyMesh)
	{
		return;
	}
	if (UMaterialInstanceDynamic* Mid = BodyMesh->CreateAndSetMaterialInstanceDynamic(0))
	{
		const FLinearColor Base = Data->GetFaction(Faction).Color;
		// стройплощадка — приглушённый цвет, готовое — полный
		const float K = IsCompleted() ? 1.f : 0.25f + 0.5f * BuildProgress;
		Mid->SetVectorParameterValue(TEXT("Color"), Base * K);
	}
}

void ABuildingBase::AddConstructionProgress(float WorkSeconds)
{
	if (IsCompleted() || bDying)
	{
		return;
	}
	URTSDataSubsystem* Data = GetData();
	if (!Data)
	{
		return;
	}
	const float BuildTime = FMath::Max(1.f, Data->GetBuilding(BuildingKind).BuildTime);
	const float Delta = WorkSeconds / BuildTime;
	BuildProgress = FMath::Min(1.f, BuildProgress + Delta);
	// прочность добирается вместе с прогрессом (90% от макс — стройкой)
	Health->AddHp(Delta * Health->GetMaxHp() * 0.9f);

	if (IsCompleted())
	{
		Health->AddHp(Health->GetMaxHp()); // добить до полной
		if (UWorld* World = GetWorld())
		{
			if (URTSEconomySubsystem* Econ = World->GetSubsystem<URTSEconomySubsystem>())
			{
				Econ->StatsOf(TeamId).BuildingsBuilt++;
			}
		}
		UE_LOG(LogRubezh, Log, TEXT("Здание %d команды %d построено"), int32(BuildingKind), TeamId);
	}
	UpdateConstructionVisual();
}

void ABuildingBase::RepairTick(float DeltaSeconds)
{
	if (!NeedsRepair())
	{
		return;
	}
	if (const URTSDataSubsystem* Data = GetData())
	{
		const float BuildTime = FMath::Max(1.f, Data->GetBuilding(BuildingKind).BuildTime);
		Health->AddHp((DeltaSeconds / BuildTime) * Health->GetMaxHp() * RTSCore::Econ::RepairSpeedMul);
	}
}

void ABuildingBase::Tick(float DeltaSeconds)
{
	AActor::Tick(DeltaSeconds);
	if (bDying || !IsCompleted())
	{
		return;
	}

	// производство
	if (Production)
	{
		Production->Step(DeltaSeconds);
	}

	// турель
	if (Weapon && Weapon->IsArmed())
	{
		Weapon->TickCooldown(DeltaSeconds);
		TickTurret(DeltaSeconds);
	}
}

void ABuildingBase::TickTurret(float DeltaSeconds)
{
	UWorld* World = GetWorld();
	URTSEconomySubsystem* Econ = World ? World->GetSubsystem<URTSEconomySubsystem>() : nullptr;
	if (!Econ)
	{
		return;
	}

	TurretScanAccum += DeltaSeconds;
	AActor* Target = TurretTarget.Get();
	const bool bTargetInvalid = !IsValid(Target) || !Weapon->InRangeOf(Target);
	if (bTargetInvalid && TurretScanAccum >= 0.3f)
	{
		TurretScanAccum = 0.f;
		Target = Econ->FindNearestEnemyUnit(TeamId, GetActorLocation(), Weapon->GetRange());
		TurretTarget = Target;
	}
	if (IsValid(Target))
	{
		// энергодефицит: турель перезаряжается вдвое дольше
		const float CooldownScale = Econ->HasPowerDeficit(TeamId) ? 2.f : 1.f;
		Weapon->TryFireAt(Target, CooldownScale);
	}
}

void ABuildingBase::NotifyDamaged(AActor* Attacker)
{
	LastDamagedTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
	(void)Attacker;
}

void ABuildingBase::HandleDeath(AActor* Killer)
{
	if (bDying)
	{
		return;
	}
	bDying = true;

	if (UWorld* World = GetWorld())
	{
		if (URTSEconomySubsystem* Econ = World->GetSubsystem<URTSEconomySubsystem>())
		{
			Econ->StatsOf(TeamId).BuildingsLost++;
			if (AUnitBase* KillerUnit = Cast<AUnitBase>(Killer))
			{
				Econ->StatsOf(KillerUnit->TeamId).EnemiesKilled++;
			}
		}
		DrawDebugSphere(World, GetActorLocation(), GetFootprintExtents().Size() + 80.f, 16,
		                FColor(220, 120, 40), false, 0.8f, 0, 4.f);
	}
	Destroy();
}
