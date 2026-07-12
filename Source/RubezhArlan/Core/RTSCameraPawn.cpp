#include "Core/RTSCameraPawn.h"
#include "RubezhArlan.h"
#include "Core/RTSBalanceCore.h"
#include "Components/SceneComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Camera/CameraComponent.h"

ARTSCameraPawn::ARTSCameraPawn()
{
	PrimaryActorTick.bCanEverTick = true;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	SpringArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("SpringArm"));
	SpringArm->SetupAttachment(SceneRoot);
	SpringArm->TargetArmLength = 3200.f;
	SpringArm->SetRelativeRotation(FRotator(-58.f, 0.f, 0.f));
	SpringArm->bDoCollisionTest = false;
	SpringArm->bEnableCameraLag = true;
	SpringArm->CameraLagSpeed = 12.f;

	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(SpringArm, USpringArmComponent::SocketName);
	Camera->SetFieldOfView(55.f);
}

void ARTSCameraPawn::PanWorld(const FVector2D& Delta)
{
	FVector Loc = GetActorLocation() + FVector(Delta.X, Delta.Y, 0.f);
	const float Bound = RTSCore::MapHalfSize + 300.f;
	Loc.X = FMath::Clamp(float(Loc.X), -Bound, Bound);
	Loc.Y = FMath::Clamp(float(Loc.Y), -Bound, Bound);
	Loc.Z = 0.f;
	SetActorLocation(Loc);
}

void ARTSCameraPawn::Tick(float DeltaSeconds)
{
	APawn::Tick(DeltaSeconds);
	// плавный зум: стрела тянется к целевой длине
	const float Alpha = FMath::Clamp(DeltaSeconds * 9.f, 0.f, 1.f);
	SpringArm->TargetArmLength = FMath::Lerp(SpringArm->TargetArmLength, TargetArmLength, Alpha);
}

void ARTSCameraPawn::Zoom(float Delta)
{
	TargetArmLength = FMath::Clamp(TargetArmLength - Delta * ZoomStep, MinArm, MaxArm);
}

void ARTSCameraPawn::CenterOn(const FVector& WorldPos)
{
	SetActorLocation(FVector(WorldPos.X, WorldPos.Y, 0.f));
}

float ARTSCameraPawn::GetArmLength() const
{
	return SpringArm->TargetArmLength;
}
