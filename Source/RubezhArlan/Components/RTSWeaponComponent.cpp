#include "Components/RTSWeaponComponent.h"
#include "RubezhArlan.h"
#include "Components/RTSHealthComponent.h"
#include "Combat/RTSProjectile.h"
#include "Fog/RTSFogSubsystem.h"
#include "Units/UnitBase.h"
#include "Buildings/BuildingBase.h"
#include "Engine/World.h"
#include "DrawDebugHelpers.h"

void URTSWeaponComponent::Init(float InDamage, float InRange, float InCooldown, ERTSDamageType InType,
                               float InSplash, float InMinRange, float InProjectileSpeed,
                               uint8 InTeamId, ERTSFaction InFaction)
{
	Damage = InDamage;
	Range = InRange;
	Cooldown = FMath::Max(0.05f, InCooldown);
	DamageType = InType;
	SplashRadius = InSplash;
	MinRange = InMinRange;
	ProjectileSpeed = InProjectileSpeed;
	TeamId = InTeamId;
	Faction = InFaction;
	CooldownRemaining = 0.f;
}

float URTSWeaponComponent::DistanceTo(const AActor* Target) const
{
	if (!GetOwner() || !Target)
	{
		return 1e9f;
	}
	return float(FVector::Dist2D(GetOwner()->GetActorLocation(), Target->GetActorLocation()));
}

bool URTSWeaponComponent::InRangeOf(const AActor* Target) const
{
	const float D = DistanceTo(Target);
	return D <= Range && D >= MinRange;
}

URTSHealthComponent* URTSWeaponComponent::FindTargetHealth(AActor* Target)
{
	if (AUnitBase* Unit = Cast<AUnitBase>(Target))
	{
		return Unit->Health;
	}
	if (ABuildingBase* Building = Cast<ABuildingBase>(Target))
	{
		return Building->Health;
	}
	return nullptr;
}

ERTSFireResult URTSWeaponComponent::TryFireAt(AActor* Target, float CooldownScale)
{
	AActor* Owner = GetOwner();
	if (!IsArmed() || !Owner || !IsValid(Target))
	{
		return ERTSFireResult::Invalid;
	}
	URTSHealthComponent* TargetHealth = FindTargetHealth(Target);
	if (!TargetHealth || !TargetHealth->IsAlive() || TargetHealth->GetTeamId() == TeamId)
	{
		return ERTSFireResult::Invalid;
	}
	if (!IsReady())
	{
		return ERTSFireResult::NotReady;
	}

	const float Dist = DistanceTo(Target);
	if (Dist > Range)
	{
		return ERTSFireResult::OutOfRange;
	}
	if (Dist < MinRange)
	{
		return ERTSFireResult::TooClose;
	}

	// игрок не стреляет в туман
	UWorld* World = Owner->GetWorld();
	if (World && TeamId == 0)
	{
		if (const URTSFogSubsystem* Fog = World->GetSubsystem<URTSFogSubsystem>())
		{
			if (!Fog->IsVisibleFor(TeamId, Target->GetActorLocation()))
			{
				return ERTSFireResult::NotVisible;
			}
		}
	}

	CooldownRemaining = Cooldown * FMath::Max(0.1f, CooldownScale);

	const FVector From = Owner->GetActorLocation() + FVector(0, 0, 60);
	const FVector To = Target->GetActorLocation() + FVector(0, 0, 40);

	if (ProjectileSpeed > 0.f && SplashRadius > 0.f)
	{
		// артиллерийский снаряд летит в ТОЧКУ (упреждения нет — как в прототипе)
		if (World)
		{
			FActorSpawnParameters Params;
			Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			ARTSProjectile* Shell = World->SpawnActor<ARTSProjectile>(From, FRotator::ZeroRotator, Params);
			if (Shell)
			{
				Shell->InitShell(Target->GetActorLocation(), Damage, DamageType, SplashRadius,
				                 ProjectileSpeed, TeamId, Faction);
			}
		}
	}
	else
	{
		TargetHealth->ApplyTypedDamage(Damage, DamageType, Faction, Owner);
		if (World)
		{
			const FColor TracerColor = DamageType == ERTSDamageType::Rocket ? FColor::Orange : FColor(60, 52, 40);
			DrawDebugLine(World, From, To, TracerColor, false, 0.1f, 0, DamageType == ERTSDamageType::Rocket ? 5.f : 2.f);
		}
	}
	return ERTSFireResult::Fired;
}
