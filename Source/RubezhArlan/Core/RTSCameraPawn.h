#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "RTSCameraPawn.generated.h"

class USceneComponent;
class USpringArmComponent;
class UCameraComponent;

/** Камера RTS: точка на земле + наклонная стрела. Пан/зум — методами. */
UCLASS()
class RUBEZHARLAN_API ARTSCameraPawn : public APawn
{
	GENERATED_BODY()

public:
	ARTSCameraPawn();

	/** Сдвиг точки взгляда в плоскости карты (мировые UU), с клампом границ. */
	void PanWorld(const FVector2D& Delta);

	/** Зум: положительный Delta — приближение. */
	void Zoom(float Delta);

	void CenterOn(const FVector& WorldPos);

	float GetArmLength() const;

	UPROPERTY(VisibleAnywhere, Category = "RTS") TObjectPtr<USceneComponent> SceneRoot;
	UPROPERTY(VisibleAnywhere, Category = "RTS") TObjectPtr<USpringArmComponent> SpringArm;
	UPROPERTY(VisibleAnywhere, Category = "RTS") TObjectPtr<UCameraComponent> Camera;

	static constexpr float MinArm = 900.f;
	static constexpr float MaxArm = 6500.f;
	static constexpr float ZoomStep = 400.f;
};
