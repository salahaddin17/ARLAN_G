#include "Econ/SupplyDepot.h"
#include "RubezhArlan.h"
#include "Econ/RTSEconomySubsystem.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"
#include "Engine/World.h"

ASupplyDepot::ASupplyDepot()
{
	PrimaryActorTick.bCanEverTick = false;

	CrateMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("CrateMesh"));
	SetRootComponent(CrateMesh);
	CrateMesh->SetCollisionProfileName(TEXT("BlockAll"));
	CrateMesh->SetCanEverAffectNavigation(false);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeFinder(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeFinder.Succeeded())
	{
		CrateMesh->SetStaticMesh(CubeFinder.Object);
	}
	CrateMesh->SetRelativeScale3D(FVector(1.6f, 1.6f, 0.9f));
}

void ASupplyDepot::BeginPlay()
{
	AActor::BeginPlay();
	if (UWorld* World = GetWorld())
	{
		if (URTSEconomySubsystem* Econ = World->GetSubsystem<URTSEconomySubsystem>())
		{
			Econ->RegisterDepot(this);
		}
	}
	if (UMaterialInstanceDynamic* Mid = CrateMesh->CreateAndSetMaterialInstanceDynamic(0))
	{
		Mid->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.85f, 0.78f, 0.45f));
	}
}

void ASupplyDepot::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		if (URTSEconomySubsystem* Econ = World->GetSubsystem<URTSEconomySubsystem>())
		{
			Econ->UnregisterDepot(this);
		}
	}
	AActor::EndPlay(EndPlayReason);
}

void ASupplyDepot::InitDepot(float InAmount)
{
	Amount = InAmount;
	InitialAmount = InAmount;
}

float ASupplyDepot::TakeSupplies(float Want)
{
	const float Taken = FMath::Clamp(Want, 0.f, Amount);
	Amount -= Taken;
	if (Amount <= 0.f)
	{
		// пустой склад визуально «сдувается»
		if (CrateMesh)
		{
			CrateMesh->SetRelativeScale3D(FVector(1.0f, 1.0f, 0.15f));
		}
	}
	return Taken;
}
