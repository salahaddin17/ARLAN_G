#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Data/RTSTypes.h"
#include "RTSHealthComponent.generated.h"

/**
 * Прочность + тип брони. Урон принимает ТОЛЬКО типизированный
 * (матрица «тип урона × тип брони» + фракционный бонус атакующего).
 * О смерти сообщает владельцу (AUnitBase / ABuildingBase) напрямую.
 */
UCLASS(ClassGroup = (RTS), meta = (BlueprintSpawnableComponent))
class RUBEZHARLAN_API URTSHealthComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	void Init(float InMaxHp, ERTSArmor InArmor, uint8 InTeamId);

	/** Полный конвейер урона; возвращает фактически снятые очки прочности. */
	float ApplyTypedDamage(float BaseDamage, ERTSDamageType DamageType,
	                       ERTSFaction AttackerFaction, AActor* Attacker);

	/** Прямое лечение/достройка (стройка техником, ремонт). */
	void AddHp(float Amount);

	bool IsAlive() const { return bAlive; }
	float GetHp() const { return CurrentHp; }
	float GetMaxHp() const { return MaxHp; }
	float GetFraction() const { return MaxHp > 0.f ? CurrentHp / MaxHp : 0.f; }
	ERTSArmor GetArmor() const { return Armor; }
	uint8 GetTeamId() const { return TeamId; }

	UPROPERTY(VisibleAnywhere, Category = "RTS")
	float MaxHp = 100.f;

	UPROPERTY(VisibleAnywhere, Category = "RTS")
	float CurrentHp = 100.f;

	UPROPERTY(VisibleAnywhere, Category = "RTS")
	ERTSArmor Armor = ERTSArmor::INF;

	UPROPERTY(VisibleAnywhere, Category = "RTS")
	uint8 TeamId = 0;

private:
	bool bAlive = true;
	void NotifyOwnerOfDeath(AActor* Killer);
	void NotifyOwnerOfDamage(AActor* Attacker);
};
