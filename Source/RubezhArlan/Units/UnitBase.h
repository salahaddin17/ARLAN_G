#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Data/RTSTypes.h"
#include "UnitBase.generated.h"

class URTSHealthComponent;
class URTSWeaponComponent;
class URTSDataSubsystem;
class ARTSAIController;
class UStaticMeshComponent;

/**
 * Базовый юнит — «тактическая фишка» из составных примитивов:
 * пехота — жетон-цилиндр с эмблемой и шагающим бобом, техника — корпус
 * с поворотной башней/стволом, отдачей и вспышкой выстрела. Смерть —
 * короткая анимация проседания. Вся логика приказов — в ARTSAIController.
 */
UCLASS()
class RUBEZHARLAN_API AUnitBase : public ACharacter
{
	GENERATED_BODY()

public:
	AUnitBase();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void Tick(float DeltaSeconds) override;

	/** Вызывать сразу после SpawnActor: статы из таблиц, визуал, команда. */
	void InitUnit(ERTSUnitKind InKind, uint8 InTeamId);

	void HandleDeath(AActor* Killer);
	void NotifyDamaged(AActor* Attacker);

	/** Оружие сообщает, куда целимся (для поворота башни). */
	void SetAimPoint(const FVector& WorldPoint);
	/** Оружие сообщает о состоявшемся выстреле (отдача + вспышка). */
	void OnWeaponFired();

	void SetSelected(bool bInSelected) { bSelected = bInSelected; }
	bool IsSelected() const { return bSelected; }
	bool IsCombatUnit() const;
	bool IsDying() const { return bDying; }

	ARTSAIController* GetRTSController() const;
	URTSDataSubsystem* GetData() const;

	UPROPERTY(VisibleAnywhere, Category = "RTS") ERTSUnitKind UnitKind = ERTSUnitKind::Rifleman;
	UPROPERTY(VisibleAnywhere, Category = "RTS") uint8 TeamId = 0;
	UPROPERTY(VisibleAnywhere, Category = "RTS") ERTSFaction Faction = ERTSFaction::Legion;

	UPROPERTY(VisibleAnywhere, Category = "RTS") TObjectPtr<URTSHealthComponent> Health;
	UPROPERTY(VisibleAnywhere, Category = "RTS") TObjectPtr<URTSWeaponComponent> Weapon;
	UPROPERTY(VisibleAnywhere, Category = "RTS") TObjectPtr<UStaticMeshComponent> TokenMesh;

	/** Груз грузовика (припасы). */
	UPROPERTY(VisibleAnywhere, Category = "RTS") float Cargo = 0.f;

private:
	bool bSelected = false;
	bool bDying = false;

	// --- процедурный риг ------------------------------------------------------
	UPROPERTY() TObjectPtr<UStaticMeshComponent> TurretPart;  // башня/кабина/эмблема
	UPROPERTY() TObjectPtr<UStaticMeshComponent> BarrelPart;  // ствол (у башни)
	UPROPERTY() TObjectPtr<UStaticMeshComponent> FlashPart;   // вспышка выстрела
	UPROPERTY() TObjectPtr<UStaticMeshComponent> CargoPart;   // кузов грузовика

	UPROPERTY() TObjectPtr<UStaticMesh> CylinderMesh;
	UPROPERTY() TObjectPtr<UStaticMesh> CubeMesh;
	UPROPERTY() TObjectPtr<UStaticMesh> SphereMesh;

	// состояние анимаций
	float TokenBaseZ = -42.f;
	float BarrelBaseX = 0.f;
	float RecoilOffset = 0.f;
	float FlashTimer = 0.f;
	float BobPhase = 0.f;
	float TurretRelYaw = 0.f;
	FVector AimPoint = FVector::ZeroVector;
	float AimFreshness = 1e9f; // сек с последнего прицеливания
	float DeathTimer = 0.f;

	void SetupTokenVisual();
	UStaticMeshComponent* MakePart(UStaticMesh* PartMesh, USceneComponent* Parent,
	                               const FVector& RelLocation, const FVector& RelScale,
	                               const FLinearColor& Color);
	void TickAnimations(float DeltaSeconds);
	void TickDeath(float DeltaSeconds);
};
