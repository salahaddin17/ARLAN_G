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
	CylinderMesh = CylinderFinder.Succeeded() ? CylinderFinder.Object : nullptr;
	CubeMesh = CubeFinder.Succeeded() ? CubeFinder.Object : nullptr;

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

void AUnitBase::SetupTokenVisual()
{
	URTSDataSubsystem* Data = GetData();
	if (!Data || !TokenMesh)
	{
		return;
	}
	const FUnitRow& Row = Data->GetUnit(UnitKind);

	if (Row.bInfantry)
	{
		// круглый жетон
		if (CylinderMesh)
		{
			TokenMesh->SetStaticMesh(CylinderMesh);
		}
		TokenMesh->SetRelativeScale3D(FVector(0.7f, 0.7f, 0.35f));
		TokenMesh->SetRelativeLocation(FVector(0, 0, -42.f));
	}
	else
	{
		// прямоугольная фишка техники; грузовик/арта длиннее
		if (CubeMesh)
		{
			TokenMesh->SetStaticMesh(CubeMesh);
		}
		const bool bLong = UnitKind == ERTSUnitKind::Artillery || UnitKind == ERTSUnitKind::Truck;
		TokenMesh->SetRelativeScale3D(FVector(bLong ? 1.15f : 0.95f, 0.62f, 0.4f));
		TokenMesh->SetRelativeLocation(FVector(0, 0, -38.f));
	}

	if (UMaterialInstanceDynamic* Mid = TokenMesh->CreateAndSetMaterialInstanceDynamic(0))
	{
		Mid->SetVectorParameterValue(TEXT("Color"), Data->GetFaction(Faction).Color);
	}
}

void AUnitBase::Tick(float DeltaSeconds)
{
	ACharacter::Tick(DeltaSeconds);

	if (Weapon)
	{
		Weapon->TickCooldown(DeltaSeconds);
	}

	// кольцо выделения — плоский отладочный цилиндр у ног
	if (bSelected && GetWorld())
	{
		const FVector Base = GetActorLocation() - FVector(0, 0, 55.f);
		DrawDebugCylinder(GetWorld(), Base, Base + FVector(0, 0, 4.f), 62.f, 18,
		                  FColor(240, 236, 218), false, -1.f, 0, 2.5f);
	}
}

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

	if (UWorld* World = GetWorld())
	{
		if (URTSEconomySubsystem* Econ = World->GetSubsystem<URTSEconomySubsystem>())
		{
			Econ->StatsOf(TeamId).UnitsLost++;
			if (AUnitBase* KillerUnit = Cast<AUnitBase>(Killer))
			{
				Econ->StatsOf(KillerUnit->TeamId).EnemiesKilled++;
			}
		}
		DrawDebugSphere(World, GetActorLocation(), 90.f, 12, FColor(220, 150, 60), false, 0.5f, 0, 3.f);
	}
	Destroy();
}
