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
 * Базовый юнит — «тактическая фишка»: пехота — цилиндр-жетон, техника —
 * прямоугольный бокс, цвет — фракционный. ACharacter ради готового
 * NavMesh-движения (CharacterMovement + RVO-разведение).
 * Вся логика приказов — в ARTSAIController.
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

	void SetSelected(bool bInSelected) { bSelected = bInSelected; }
	bool IsSelected() const { return bSelected; }
	bool IsCombatUnit() const;

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
	void SetupTokenVisual();

	UPROPERTY() TObjectPtr<UStaticMesh> CylinderMesh;
	UPROPERTY() TObjectPtr<UStaticMesh> CubeMesh;
};
