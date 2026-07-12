#include "Units/UnitBase.h"
#include "RubezhArlan.h"
#include "Components/RTSHealthComponent.h"
#include "Components/RTSWeaponComponent.h"
#include "Units/RTSAIController.h"
#include "Data/RTSDataSubsystem.h"
#include "Econ/RTSEconomySubsystem.h"
#include "Components/StaticMeshComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "UObject/ConstructorHelpers.h"
#include "DrawDebugHelpers.h"

namespace
{
	const FLinearColor InkColor(0.10f, 0.09f, 0.075f);
	const FLinearColor FlashColor(1.f, 0.92f, 0.6f);

	float ApproachAngle(float Current, float Target, float MaxStep)
	{
		float Diff = Target - Current;
		while (Diff > 180.f) { Diff -= 360.f; }
		while (Diff < -180.f) { Diff += 360.f; }
		return Current + FMath::Clamp(Diff, -MaxStep, MaxStep);
	}
}

AUnitBase::AUnitBase()
{
	PrimaryActorTick.bCanEverTick = true;

	Health = CreateDefaultSubobject<URTSHealthComponent>(TEXT("Health"));
	Weapon = CreateDefaultSubobject<URTSWeaponComponent>(TEXT("Weapon"));

	TokenMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("TokenMesh"));
	TokenMesh->SetupAttachment(RootComponent);
	TokenMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	TokenMesh->SetCanEverAffectNavigation(false);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderFinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeFinder(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereFinder(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	CylinderMesh = CylinderFinder.Succeeded() ? CylinderFinder.Object : nullptr;
	CubeMesh = CubeFinder.Succeeded() ? CubeFinder.Object : nullptr;
	SphereMesh = SphereFinder.Succeeded() ? SphereFinder.Object : nullptr;

	AIControllerClass = ARTSAIController::StaticClass();
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;

	if (UCharacterMovementComponent* Move = GetCharacterMovement())
	{
		Move->bOrientRotationToMovement = true;
		Move->RotationRate = FRotator(0.f, 480.f, 0.f);
		Move->bUseRVOAvoidance = true;
		Move->AvoidanceConsiderationRadius = 250.f;
	}
	if (UCapsuleComponent* Capsule = GetCapsuleComponent())
	{
		Capsule->SetCapsuleSize(40.f, 60.f);
	}
	if (USkeletalMeshComponent* Skeletal = GetMesh())
	{
		Skeletal->SetVisibility(false);
	}
}

void AUnitBase::BeginPlay()
{
	ACharacter::BeginPlay();
	if (UWorld* World = GetWorld())
	{
		if (URTSEconomySubsystem* Econ = World->GetSubsystem<URTSEconomySubsystem>())
		{
			Econ->RegisterUnit(this);
		}
	}
}

void AUnitBase::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		if (URTSEconomySubsystem* Econ = World->GetSubsystem<URTSEconomySubsystem>())
		{
			Econ->UnregisterUnit(this);
		}
	}
	ACharacter::EndPlay(EndPlayReason);
}

URTSDataSubsystem* AUnitBase::GetData() const
{
	const UWorld* World = GetWorld();
	if (!World || !World->GetGameInstance())
	{
		return nullptr;
	}
	return World->GetGameInstance()->GetSubsystem<URTSDataSubsystem>();
}

ARTSAIController* AUnitBase::GetRTSController() const
{
	return Cast<ARTSAIController>(GetController());
}

bool AUnitBase::IsCombatUnit() const
{
	return Weapon && Weapon->IsArmed();
}

void AUnitBase::InitUnit(ERTSUnitKind InKind, uint8 InTeamId)
{
	UnitKind = InKind;
	TeamId = InTeamId;

	URTSDataSubsystem* Data = GetData();
	if (!Data)
	{
		UE_LOG(LogRubezh, Warning, TEXT("InitUnit без DataSubsystem"));
		return;
	}
	UWorld* World = GetWorld();
	URTSEconomySubsystem* Econ = World ? World->GetSubsystem<URTSEconomySubsystem>() : nullptr;
	Faction = Econ ? Econ->GetFactionOf(TeamId)
	               : (TeamId == 0 ? ERTSFaction::Legion : ERTSFaction::Front);

	const FUnitRow& Row = Data->GetUnit(UnitKind);
	Health->Init(Data->GetUnitMaxHp(UnitKind, Faction), Row.Armor, TeamId);
	Weapon->Init(Row.Damage, Row.Range, Row.Cooldown, Row.DamageType,
	             Row.SplashRadius, Row.MinRange, Row.ProjectileSpeed, TeamId, Faction);

	if (UCharacterMovementComponent* Move = GetCharacterMovement())
	{
		Move->MaxWalkSpeed = Data->GetUnitSpeed(UnitKind, Faction);
	}
	if (Econ)
	{
		Econ->StatsOf(TeamId).UnitsBuilt++;
	}
	SetupTokenVisual();
}

// --- процедурный риг ------------------------------------------------------------

UStaticMeshComponent* AUnitBase::MakePart(UStaticMesh* PartMesh, USceneComponent* Parent,
                                          const FVector& RelLocation, const FVector& RelScale,
                                          const FLinearColor& Color)
{
	UStaticMeshComponent* Part = NewObject<UStaticMeshComponent>(this);
	if (!Part)
	{
		return nullptr;
	}
	Part->SetupAttachment(Parent);
	Part->RegisterComponent();
	if (PartMesh)
	{
		Part->SetStaticMesh(PartMesh);
	}
	Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Part->SetCanEverAffectNavigation(false);
	Part->SetRelativeLocation(RelLocation);
	Part->SetRelativeScale3D(RelScale);
	if (UMaterialInstanceDynamic* Mid = Part->CreateAndSetMaterialInstanceDynamic(0))
	{
		Mid->SetVectorParameterValue(TEXT("Color"), Color);
	}
	return Part;
}

void AUnitBase::SetupTokenVisual()
{
	URTSDataSubsystem* Data = GetData();
	if (!Data || !TokenMesh)
	{
		return;
	}
	const FUnitRow& Row = Data->GetUnit(UnitKind);
	const FLinearColor FactionColor = Data->GetFaction(Faction).Color;

	if (UMaterialInstanceDynamic* Mid = TokenMesh->CreateAndSetMaterialInstanceDynamic(0))
	{
		Mid->SetVectorParameterValue(TEXT("Color"), FactionColor);
	}

	if (Row.bInfantry)
	{
		// жетон-цилиндр + чернильная эмблема
		if (CylinderMesh)
		{
			TokenMesh->SetStaticMesh(CylinderMesh);
		}
		TokenBaseZ = -42.f;
		TokenMesh->SetRelativeScale3D(FVector(0.7f, 0.7f, 0.35f));
		TokenMesh->SetRelativeLocation(FVector(0, 0, TokenBaseZ));

		TurretPart = MakePart(CylinderMesh, TokenMesh, FVector(0, 0, 55.f),
		                      FVector(0.55f, 0.55f, 0.35f), InkColor);
		if (UnitKind == ERTSUnitKind::Rocketeer)
		{
			// труба РПГ на плече
			BarrelPart = MakePart(CubeMesh, TurretPart, FVector(45.f, 0, 40.f),
			                      FVector(1.3f, 0.22f, 0.22f), InkColor);
			BarrelBaseX = 45.f;
			FlashPart = MakePart(SphereMesh, BarrelPart, FVector(60.f, 0, 0),
			                     FVector(0.9f, 0.9f, 0.9f), FlashColor);
		}
	}
	else
	{
		// техника: корпус-плита
		if (CubeMesh)
		{
			TokenMesh->SetStaticMesh(CubeMesh);
		}
		const bool bLong = UnitKind == ERTSUnitKind::Artillery || UnitKind == ERTSUnitKind::Truck;
		TokenBaseZ = -38.f;
		TokenMesh->SetRelativeScale3D(FVector(bLong ? 1.15f : 0.95f, 0.62f, 0.4f));
		TokenMesh->SetRelativeLocation(FVector(0, 0, TokenBaseZ));

		switch (UnitKind)
		{
		case ERTSUnitKind::Tank:
		{
			TurretPart = MakePart(CylinderMesh, TokenMesh, FVector(-0.05f, 0, 60.f),
			                      FVector(0.5f, 0.75f, 0.5f), FactionColor * 0.75f);
			BarrelPart = MakePart(CubeMesh, TurretPart, FVector(0.9f, 0, 0.25f),
			                      FVector(1.4f, 0.16f, 0.16f), InkColor);
			BarrelBaseX = 0.9f;
			FlashPart = MakePart(SphereMesh, BarrelPart, FVector(0.62f, 0, 0),
			                     FVector(0.5f, 1.6f, 1.6f), FlashColor);
			break;
		}
		case ERTSUnitKind::Artillery:
		{
			TurretPart = MakePart(CubeMesh, TokenMesh, FVector(-0.15f, 0, 55.f),
			                      FVector(0.45f, 0.5f, 0.45f), FactionColor * 0.7f);
			BarrelPart = MakePart(CubeMesh, TurretPart, FVector(1.15f, 0, 0.55f),
			                      FVector(2.1f, 0.14f, 0.14f), InkColor);
			BarrelBaseX = 1.15f;
			FlashPart = MakePart(SphereMesh, BarrelPart, FVector(0.55f, 0, 0),
			                     FVector(0.4f, 1.8f, 1.8f), FlashColor);
			break;
		}
		case ERTSUnitKind::Scout:
		{
			TurretPart = MakePart(CubeMesh, TokenMesh, FVector(0.18f, 0, 55.f),
			                      FVector(0.4f, 0.55f, 0.4f), FactionColor * 0.75f);
			// антенна разведчика
			MakePart(CubeMesh, TokenMesh, FVector(-0.35f, 0.25f, 90.f),
			         FVector(0.04f, 0.04f, 0.9f), InkColor);
			BarrelPart = MakePart(CubeMesh, TurretPart, FVector(0.75f, 0, 0.2f),
			                      FVector(0.9f, 0.12f, 0.12f), InkColor);
			BarrelBaseX = 0.75f;
			FlashPart = MakePart(SphereMesh, BarrelPart, FVector(0.6f, 0, 0),
			                     FVector(0.5f, 1.4f, 1.4f), FlashColor);
			break;
		}
		case ERTSUnitKind::Truck:
		{
			// кабина спереди, кузов-платформа сзади (растёт с грузом)
			TurretPart = MakePart(CubeMesh, TokenMesh, FVector(0.33f, 0, 62.f),
			                      FVector(0.28f, 0.8f, 0.55f), FactionColor * 0.7f);
			CargoPart = MakePart(CubeMesh, TokenMesh, FVector(-0.22f, 0, 55.f),
			                     FVector(0.5f, 0.85f, 0.12f), FLinearColor(0.85f, 0.78f, 0.45f));
			break;
		}
		default:
			break;
		}
	}

	if (FlashPart)
	{
		FlashPart->SetVisibility(false);
	}
}

// --- хуки оружия -------------------------------------------------------------------

void AUnitBase::SetAimPoint(const FVector& WorldPoint)
{
	AimPoint = WorldPoint;
	AimFreshness = 0.f;
}

void AUnitBase::OnWeaponFired()
{
	RecoilOffset = 1.f;   // в долях базового смещения ствола
	FlashTimer = 0.07f;
}

// --- тик ---------------------------------------------------------------------------

void AUnitBase::Tick(float DeltaSeconds)
{
	ACharacter::Tick(DeltaSeconds);

	if (bDying)
	{
		TickDeath(DeltaSeconds);
		return;
	}

	if (Weapon)
	{
		Weapon->TickCooldown(DeltaSeconds);
	}
	TickAnimations(DeltaSeconds);

	// пульсирующее кольцо выделения
	if (bSelected && GetWorld())
	{
		const float Pulse = 3.f * FMath::Sin(GetWorld()->GetTimeSeconds() * 5.f);
		const FVector Base = GetActorLocation() - FVector(0, 0, 55.f);
		DrawDebugCylinder(GetWorld(), Base, Base + FVector(0, 0, 4.f), 62.f + Pulse, 18,
		                  FColor(240, 236, 218), false, -1.f, 0, 2.5f);
	}
}

void AUnitBase::TickAnimations(float DeltaSeconds)
{
	AimFreshness += DeltaSeconds;
	const float Speed2D = float(GetVelocity().Size2D());

	// шаг пехоты: лёгкий боб жетона
	if (TokenMesh)
	{
		URTSDataSubsystem* Data = GetData();
		const bool bInfantry = Data ? Data->GetUnit(UnitKind).bInfantry : true;
		if (bInfantry && Speed2D > 20.f)
		{
			BobPhase += DeltaSeconds * 11.f;
			TokenMesh->SetRelativeLocation(FVector(0, 0, TokenBaseZ + FMath::Abs(FMath::Sin(BobPhase)) * 5.f));
		}
		else if (BobPhase != 0.f)
		{
			BobPhase = 0.f;
			TokenMesh->SetRelativeLocation(FVector(0, 0, TokenBaseZ));
		}
	}

	// башня доворачивается на цель; без цели — возвращается по курсу
	if (TurretPart)
	{
		float DesiredRelYaw = 0.f;
		if (AimFreshness < 1.2f)
		{
			const FVector To = AimPoint - GetActorLocation();
			const float WorldYaw = FMath::RadiansToDegrees(FMath::Atan2(float(To.Y), float(To.X)));
			DesiredRelYaw = WorldYaw - float(GetActorRotation().Yaw);
		}
		TurretRelYaw = ApproachAngle(TurretRelYaw, DesiredRelYaw, DeltaSeconds * 360.f);
		TurretPart->SetRelativeRotation(FRotator(0.f, TurretRelYaw, 0.f));
	}

	// отдача ствола и вспышка
	if (BarrelPart && RecoilOffset > 0.001f)
	{
		RecoilOffset = FMath::Max(0.f, RecoilOffset - DeltaSeconds * 6.f);
		const float Back = RecoilOffset * 0.35f; // доля от базового X
		BarrelPart->SetRelativeLocation(FVector(BarrelBaseX * (1.f - Back), 0,
			UnitKind == ERTSUnitKind::Artillery ? 0.55f :
			UnitKind == ERTSUnitKind::Tank ? 0.25f :
			UnitKind == ERTSUnitKind::Rocketeer ? 40.f : 0.2f));
	}
	if (FlashPart)
	{
		if (FlashTimer > 0.f)
		{
			FlashTimer -= DeltaSeconds;
			FlashPart->SetVisibility(FlashTimer > 0.f);
		}
	}

	// кузов грузовика растёт с грузом
	if (CargoPart)
	{
		const float Fill = FMath::Clamp(Cargo / RTSCore::Econ::TruckCapacity, 0.f, 1.f);
		CargoPart->SetRelativeScale3D(FVector(0.5f, 0.85f, 0.12f + 0.38f * Fill));
	}
}

void AUnitBase::TickDeath(float DeltaSeconds)
{
	DeathTimer += DeltaSeconds;
	const float T = FMath::Clamp(DeathTimer / 0.45f, 0.f, 1.f);
	// фишка проседает в землю и сжимается
	if (TokenMesh)
	{
		TokenMesh->SetRelativeLocation(FVector(0, 0, TokenBaseZ - T * 55.f));
	}
	SetActorRotation(FRotator(0.f, float(GetActorRotation().Yaw) + DeltaSeconds * 90.f, T * 14.f));
	if (DeathTimer >= 0.5f)
	{
		Destroy();
	}
}

// --- урон и смерть -------------------------------------------------------------------

void AUnitBase::NotifyDamaged(AActor* Attacker)
{
	// ответка: свободный боец разворачивается на обидчика
	if (bDying || !IsCombatUnit() || !Attacker)
	{
		return;
	}
	if (ARTSAIController* RTSController = GetRTSController())
	{
		if (RTSController->GetOrderType() == ERTSOrderType::Idle)
		{
			RTSController->SetOrder(FRTSOrder::MakeAttackTarget(Attacker));
		}
	}
}

void AUnitBase::HandleDeath(AActor* Killer)
{
	if (bDying)
	{
		return;
	}
	bDying = true;
	DeathTimer = 0.f;

	// мгновенно выходим из игры как цель/препятствие; тело доигрывает анимацию
	SetActorEnableCollision(false);
	if (ARTSAIController* RTSController = GetRTSController())
	{
		RTSController->StopMovement();
		RTSController->SetActorTickEnabled(false);
	}

	if (UWorld* World = GetWorld())
	{
		if (URTSEconomySubsystem* Econ = World->GetSubsystem<URTSEconomySubsystem>())
		{
			Econ->StatsOf(TeamId).UnitsLost++;
			if (AUnitBase* KillerUnit = Cast<AUnitBase>(Killer))
			{
				Econ->StatsOf(KillerUnit->TeamId).EnemiesKilled++;
			}
			Econ->UnregisterUnit(this); // живые не целятся в умирающих
		}
		DrawDebugSphere(World, GetActorLocation(), 90.f, 12, FColor(220, 150, 60), false, 0.5f, 0, 3.f);
	}
}
