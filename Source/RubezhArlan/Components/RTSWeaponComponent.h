#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Data/RTSTypes.h"
#include "RTSWeaponComponent.generated.h"

class URTSHealthComponent;

UENUM()
enum class ERTSFireResult : uint8
{
	Fired,
	NotReady,      // перезарядка
	OutOfRange,
	TooClose,      // ближе минимальной дальности (артиллерия)
	NotVisible,    // цель в тумане войны
	Invalid,       // нет цели/оружия
};

/**
 * Оружие юнита или турели. Данные задаются Init'ом из таблиц.
 * Кулдаун тикается владельцем (TickCooldown) — детерминированный порядок.
 * Артиллерия (ProjectileSpeed>0 и Splash>0) стреляет снарядом ARTSProjectile,
 * остальные наносят мгновенный типизированный урон.
 */
UCLASS(ClassGroup = (RTS), meta = (BlueprintSpawnableComponent))
class RUBEZHARLAN_API URTSWeaponComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	void Init(float InDamage, float InRange, float InCooldown, ERTSDamageType InType,
	          float InSplash, float InMinRange, float InProjectileSpeed,
	          uint8 InTeamId, ERTSFaction InFaction);

	void TickCooldown(float DeltaSeconds) { CooldownRemaining = FMath::Max(0.f, CooldownRemaining - DeltaSeconds); }
	bool IsArmed() const { return Damage > 0.f; }
	bool IsReady() const { return CooldownRemaining <= 0.f; }
	float GetRange() const { return Range; }
	float GetMinRange() const { return MinRange; }

	/** Дистанция 2D от владельца до цели. */
	float DistanceTo(const AActor* Target) const;

	/** В окне [MinRange..Range]? */
	bool InRangeOf(const AActor* Target) const;

	/**
	 * Попытка выстрела. CooldownScale>1 замедляет перезарядку
	 * (турель при энергодефиците).
	 */
	ERTSFireResult TryFireAt(AActor* Target, float CooldownScale = 1.f);

	/** Прочность цели (юнита или здания); nullptr если цель не боевая. */
	static URTSHealthComponent* FindTargetHealth(AActor* Target);

	UPROPERTY(VisibleAnywhere, Category = "RTS") float Damage = 0.f;
	UPROPERTY(VisibleAnywhere, Category = "RTS") float Range = 0.f;
	UPROPERTY(VisibleAnywhere, Category = "RTS") float Cooldown = 1.f;
	UPROPERTY(VisibleAnywhere, Category = "RTS") ERTSDamageType DamageType = ERTSDamageType::Bullet;
	UPROPERTY(VisibleAnywhere, Category = "RTS") float SplashRadius = 0.f;
	UPROPERTY(VisibleAnywhere, Category = "RTS") float MinRange = 0.f;
	UPROPERTY(VisibleAnywhere, Category = "RTS") float ProjectileSpeed = 0.f;
	UPROPERTY(VisibleAnywhere, Category = "RTS") uint8 TeamId = 0;
	UPROPERTY(VisibleAnywhere, Category = "RTS") ERTSFaction Faction = ERTSFaction::Legion;

private:
	float CooldownRemaining = 0.f;
};
