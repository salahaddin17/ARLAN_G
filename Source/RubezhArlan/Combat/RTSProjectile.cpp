#include "Combat/RTSProjectile.h"
#include "RubezhArlan.h"
#include "Components/RTSHealthComponent.h"
#include "Components/RTSWeaponComponent.h"
#include "Econ/RTSEconomySubsystem.h"
#include "Units/UnitBase.h"
#include "Buildings/BuildingBase.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"
#include "Engine/World.h"
#include "DrawDebugHelpers.h"

ARTSProjectile::ARTSProjectile()
{
	PrimaryActorTick.bCanEverTick = true;

	ShellMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ShellMesh"));
	SetRootComponent(ShellMesh);
	ShellMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	ShellMesh->SetCanEverAffectNavigation(false);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereFinder(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	if (SphereFinder.Succeeded())
	{
		ShellMesh->SetStaticMesh(SphereFinder.Object);
	}
	ShellMesh->SetRelativeScale3D(FVector(0.22f, 0.22f, 0.22f));
}

void ARTSProjectile::InitShell(const FVector& InTarget, float InDamage, ERTSDamageType InType,
                               float InSplash, float InSpeed, uint8 InTeamId, ERTSFaction InFaction)
{
	TargetPoint = InTarget;
	Damage = InDamage;
	DamageType = InType;
	SplashRadius = InSplash;
	Speed = FMath::Max(100.f, InSpeed);
	TeamId = InTeamId;
	Faction = InFaction;
	bArmed = true;

	if (UMaterialInstanceDynamic* Mid = ShellMesh->CreateAndSetMaterialInstanceDynamic(0))
	{
		Mid->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.12f, 0.1f, 0.08f));
	}
	SetLifeSpan(12.f); // страховка от вечных снарядов
}

void ARTSProjectile::Tick(float DeltaSeconds)
{
	AActor::Tick(DeltaSeconds);
	if (!bArmed)
	{
		return;
	}

	const FVector Loc = GetActorLocation();
	FVector Flat = TargetPoint - Loc;
	Flat.Z = 0;
	const float Remaining = float(Flat.Size());
	const float StepLen = Speed * DeltaSeconds;

	if (Remaining <= StepLen)
	{
		Explode();
		return;
	}

	// плоский полёт + лёгкая дуга по Z
	const FVector Dir = Flat.GetSafeNormal();
	FVector NewLoc = Loc + Dir * StepLen;
	NewLoc.Z = TargetPoint.Z + 60.f + FMath::Min(300.f, Remaining * 0.25f);
	SetActorLocation(NewLoc);
}

void ARTSProjectile::Explode()
{
	bArmed = false;
	UWorld* World = GetWorld();
	if (!World)
	{
		Destroy();
		return;
	}

	DrawDebugSphere(World, TargetPoint, SplashRadius, 16, FColor(220, 150, 60), false, 0.6f, 0, 4.f);

	if (URTSEconomySubsystem* Econ = World->GetSubsystem<URTSEconomySubsystem>())
	{
		const float Zone = SplashRadius * 1.2f;

		// собираем цели заранее: урон может убивать и менять реестры
		TArray<AActor*> Targets;
		for (AUnitBase* Unit : Econ->GetAllUnits())
		{
			if (IsValid(Unit) && Unit->TeamId != TeamId &&
				FVector::Dist2D(TargetPoint, Unit->GetActorLocation()) <= Zone)
			{
				Targets.Add(Unit);
			}
		}
		for (ABuildingBase* Building : Econ->GetAllBuildings())
		{
			if (IsValid(Building) && Building->TeamId != TeamId &&
				FVector::Dist2D(TargetPoint, Building->GetActorLocation()) <=
					Zone + Building->GetFootprintExtents().Size() * 0.5f)
			{
				Targets.Add(Building);
			}
		}

		for (AActor* Target : Targets)
		{
			if (URTSHealthComponent* TargetHealth = URTSWeaponComponent::FindTargetHealth(Target))
			{
				float Dist = float(FVector::Dist2D(TargetPoint, Target->GetActorLocation()));
				// у зданий урон считаем до края корпуса, а не до центра
				if (const ABuildingBase* Building = Cast<ABuildingBase>(Target))
				{
					Dist = FMath::Max(0.f, Dist - float(Building->GetFootprintExtents().Size()) * 0.5f);
				}
				const float Falloff = RTSCore::SplashFalloff(Dist, SplashRadius);
				if (Falloff > 0.f)
				{
					TargetHealth->ApplyTypedDamage(Damage * Falloff, DamageType, Faction, this);
				}
			}
		}
	}
	Destroy();
}
