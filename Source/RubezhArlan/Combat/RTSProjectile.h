#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Data/RTSTypes.h"
#include "RTSProjectile.generated.h"

class UStaticMeshComponent;

/**
 * Артиллерийский снаряд: летит в ТОЧКУ (без самонаведения), у цели
 * взрывается сплешем с затуханием RTSCore::SplashFalloff. Бьёт только
 * врагов стрелявшей команды (как в эталонном прототипе).
 */
UCLASS()
class RUBEZHARLAN_API ARTSProjectile : public AActor
{
	GENERATED_BODY()

public:
	ARTSProjectile();

	virtual void Tick(float DeltaSeconds) override;

	void InitShell(const FVector& InTarget, float InDamage, ERTSDamageType InType,
	               float InSplash, float InSpeed, uint8 InTeamId, ERTSFaction InFaction);

	UPROPERTY(VisibleAnywhere, Category = "RTS") TObjectPtr<UStaticMeshComponent> ShellMesh;

private:
	FVector TargetPoint = FVector::ZeroVector;
	float Damage = 0.f;
	ERTSDamageType DamageType = ERTSDamageType::Shell;
	float SplashRadius = 0.f;
	float Speed = 600.f;
	uint8 TeamId = 0;
	ERTSFaction Faction = ERTSFaction::Legion;
	bool bArmed = false;

	void Explode();
};
