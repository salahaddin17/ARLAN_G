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
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderFinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	CubeMesh = CubeFinder.Succeeded() ? CubeFinder.Object : nullptr;
	CylinderMesh = CylinderFinder.Succeeded() ? CylinderFinder.Object : nullptr;
	if (CubeMesh)
	{
		BodyMesh->SetStaticMesh(CubeMesh);
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
	BodyBaseScale = FVector(Row.SizeX, Row.SizeY, H / 100.f);
	BodyMesh->SetRelativeScale3D(BodyBaseScale);

	// турель: вращающаяся голова со стволом поверх основания
	if (BuildingKind == ERTSBuildingKind::Turret && !HeadPart)
	{
		auto MakePart = [this](UStaticMesh* Mesh, USceneComponent* Parent, const FVector& Loc,
		                       const FVector& Scale, const FLinearColor& Color) -> UStaticMeshComponent*
		{
			UStaticMeshComponent* Part = NewObject<UStaticMeshComponent>(this);
			if (!Part)
			{
				return nullptr;
			}
			Part->SetupAttachment(Parent);
			Part->RegisterComponent();
			if (Mesh)
			{
				Part->SetStaticMesh(Mesh);
			}
			Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			Part->SetCanEverAffectNavigation(false);
			Part->SetRelativeLocation(Loc);
			Part->SetRelativeScale3D(Scale);
			if (UMaterialInstanceDynamic* Mid = Part->CreateAndSetMaterialInstanceDynamic(0))
			{
				Mid->SetVectorParameterValue(TEXT("Color"), Color);
			}
			return Part;
		};
		const FLinearColor FactionColor = Data->GetFaction(Faction).Color;
		HeadPart = MakePart(CylinderMesh, BodyMesh, FVector(0, 0, 60.f),
		                    FVector(0.55f, 0.55f, 0.4f), FactionColor * 0.7f);
		if (HeadPart)
		{
			BarrelPart = MakePart(CubeMesh, HeadPart, FVector(0.85f, 0, 0.2f),
			                      FVector(1.5f, 0.14f, 0.14f), FLinearColor(0.10f, 0.09f, 0.075f));
		}
	}
	UpdateConstructionVisual();
}

void ABuildingBase::SetAimPoint(const FVector& WorldPoint)
{
	AimPoint = WorldPoint;
	AimFreshness = 0.f;
}

void ABuildingBase::OnWeaponFired()
{
	RecoilOffset = 1.f;
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
	// корпус «вырастает» из земли по мере стройки
	if (!IsCompleted())
	{
		const float GrowZ = 0.15f + 0.85f * BuildProgress;
		BodyMesh->SetRelativeScale3D(FVector(BodyBaseScale.X, BodyBaseScale.Y, BodyBaseScale.Z * GrowZ));
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
		CompletePop = 1.f;                 // «отскок» масштаба при вводе в строй
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
	if (bDying)
	{
		return;
	}
	TickVisuals(DeltaSeconds);
	if (!IsCompleted())
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

void ABuildingBase::TickVisuals(float DeltaSeconds)
{
	AimFreshness += DeltaSeconds;
	if (!BodyMesh)
	{
		return;
	}

	// «отскок» при завершении стройки + лёгкий пульс работающего производства
	if (IsCompleted())
	{
		float Scale = 1.f;
		if (CompletePop > 0.f)
		{
			CompletePop = FMath::Max(0.f, CompletePop - DeltaSeconds * 3.f);
			Scale += 0.08f * FMath::Sin(CompletePop * 3.1415926f);
		}
		else if (Production && Production->GetQueue().Num() > 0 && GetWorld())
		{
			Scale += 0.012f * FMath::Sin(GetWorld()->GetTimeSeconds() * 4.f);
		}
		BodyMesh->SetRelativeScale3D(FVector(BodyBaseScale.X * Scale, BodyBaseScale.Y * Scale, BodyBaseScale.Z));
	}

	// голова турели: доворот на цель / медленное сканирование
	if (HeadPart)
	{
		float Desired = HeadYaw;
		float TurnSpeed = 40.f; // сканирование
		if (AimFreshness < 1.5f)
		{
			const FVector To = AimPoint - GetActorLocation();
			Desired = FMath::RadiansToDegrees(FMath::Atan2(float(To.Y), float(To.X)));
			TurnSpeed = 300.f;
		}
		else if (GetWorld())
		{
			Desired = FMath::Sin(GetWorld()->GetTimeSeconds() * 0.5f) * 60.f +
				(TeamId == 0 ? 45.f : 225.f); // «смотрит» в сторону фронта
		}
		float Diff = Desired - HeadYaw;
		while (Diff > 180.f) { Diff -= 360.f; }
		while (Diff < -180.f) { Diff += 360.f; }
		HeadYaw += FMath::Clamp(Diff, -TurnSpeed * DeltaSeconds, TurnSpeed * DeltaSeconds);
		HeadPart->SetRelativeRotation(FRotator(0.f, HeadYaw, 0.f));
	}
	if (BarrelPart && RecoilOffset > 0.001f)
	{
		RecoilOffset = FMath::Max(0.f, RecoilOffset - DeltaSeconds * 6.f);
		BarrelPart->SetRelativeLocation(FVector(0.85f * (1.f - RecoilOffset * 0.3f), 0, 0.2f));
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
