#include "Core/RTSGroundActor.h"
#include "RubezhArlan.h"
#include "Core/RTSBalanceCore.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"

ARTSGroundActor::ARTSGroundActor()
{
	PrimaryActorTick.bCanEverTick = false;

	GroundMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("GroundMesh"));
	SetRootComponent(GroundMesh);
	GroundMesh->SetCollisionProfileName(TEXT("BlockAll"));

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeFinder(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeFinder.Succeeded())
	{
		GroundMesh->SetStaticMesh(CubeFinder.Object);
	}
	// куб 100×100×100: верхняя грань на Z=0, запас за краями карты
	const float Tiles = (RTSCore::MapHalfSize * 2.f + 600.f) / 100.f;
	GroundMesh->SetRelativeScale3D(FVector(Tiles, Tiles, 1.f));
	GroundMesh->SetRelativeLocation(FVector(0.f, 0.f, -50.f));
}

void ARTSGroundActor::BeginPlay()
{
	AActor::BeginPlay();
	// бумажно-песчаный тон «штабной карты» (#c8bb8f)
	if (UMaterialInstanceDynamic* Mid = GroundMesh->CreateAndSetMaterialInstanceDynamic(0))
	{
		Mid->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.784f, 0.733f, 0.561f));
	}
}
