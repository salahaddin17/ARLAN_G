#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Data/RTSTypes.h"
#include "BuildingBase.generated.h"

class URTSHealthComponent;
class URTSWeaponComponent;
class URTSProductionComponent;
class URTSDataSubsystem;
class UStaticMeshComponent;

/**
 * Здание: бокс с фракционным цветом. Спавнится либо готовым (стартовые КЦ),
 * либо стройплощадкой (BuildProgress 0 → 1, строит техник через
 * AddConstructionProgress). Турель — единственное здание с оружием.
 */
UCLASS()
class RUBEZHARLAN_API ABuildingBase : public AActor
{
	GENERATED_BODY()

public:
	ABuildingBase();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void Tick(float DeltaSeconds) override;

	void InitBuilding(ERTSBuildingKind InKind, uint8 InTeamId, bool bCompleted);

	/** Техник добавляет WorkSeconds секунд труда; при 100% здание вступает в строй. */
	void AddConstructionProgress(float WorkSeconds);

	/** Ремонт достроенного здания (техник), медленнее стройки. */
	void RepairTick(float DeltaSeconds);

	bool IsCompleted() const { return BuildProgress >= 1.f; }
	bool NeedsRepair() const;

	int32 GetPowerUse() const;
	int32 GetPowerProduce() const;
	FVector2D GetFootprintExtents() const;

	void HandleDeath(AActor* Killer);
	void NotifyDamaged(AActor* Attacker);

	/** Оружие сообщает точку прицеливания (доворот башни турели). */
	void SetAimPoint(const FVector& WorldPoint);
	/** Оружие сообщает о выстреле (отдача ствола). */
	void OnWeaponFired();

	URTSDataSubsystem* GetData() const;

	UPROPERTY(VisibleAnywhere, Category = "RTS") ERTSBuildingKind BuildingKind = ERTSBuildingKind::Power;
	UPROPERTY(VisibleAnywhere, Category = "RTS") uint8 TeamId = 0;
	UPROPERTY(VisibleAnywhere, Category = "RTS") ERTSFaction Faction = ERTSFaction::Legion;
	UPROPERTY(VisibleAnywhere, Category = "RTS") float BuildProgress = 1.f;

	/** Точка сбора произведённых юнитов. */
	UPROPERTY(VisibleAnywhere, Category = "RTS") FVector RallyPoint = FVector::ZeroVector;

	/** Время последнего полученного урона (оборона ИИ, миникарта). */
	UPROPERTY(VisibleAnywhere, Category = "RTS") float LastDamagedTime = -1000.f;

	/** Видел ли игрок это здание (туман: здания-«призраки»). */
	bool bSeenByPlayer = false;

	UPROPERTY(VisibleAnywhere, Category = "RTS") TObjectPtr<URTSHealthComponent> Health;
	UPROPERTY(VisibleAnywhere, Category = "RTS") TObjectPtr<URTSWeaponComponent> Weapon;
	UPROPERTY(VisibleAnywhere, Category = "RTS") TObjectPtr<URTSProductionComponent> Production;
	UPROPERTY(VisibleAnywhere, Category = "RTS") TObjectPtr<UStaticMeshComponent> BodyMesh;

private:
	bool bDying = false;
	float TurretScanAccum = 0.f;
	TWeakObjectPtr<AActor> TurretTarget;

	// процедурный риг турели и анимации
	UPROPERTY() TObjectPtr<UStaticMeshComponent> HeadPart;
	UPROPERTY() TObjectPtr<UStaticMeshComponent> BarrelPart;
	UPROPERTY() TObjectPtr<class UStaticMesh> CylinderMesh;
	UPROPERTY() TObjectPtr<class UStaticMesh> CubeMesh;
	float HeadYaw = 0.f;
	float RecoilOffset = 0.f;
	FVector AimPoint = FVector::ZeroVector;
	float AimFreshness = 1e9f;
	float CompletePop = 0.f;      // «отскок» масштаба при завершении стройки
	FVector BodyBaseScale = FVector(1, 1, 1);

	void SetupVisual();
	void UpdateConstructionVisual();
	void TickTurret(float DeltaSeconds);
	void TickVisuals(float DeltaSeconds);
};
