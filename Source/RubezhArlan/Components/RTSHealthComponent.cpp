#include "Components/RTSHealthComponent.h"
#include "RubezhArlan.h"
#include "Data/RTSDataSubsystem.h"
#include "Units/UnitBase.h"
#include "Buildings/BuildingBase.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"

void URTSHealthComponent::Init(float InMaxHp, ERTSArmor InArmor, uint8 InTeamId)
{
	MaxHp = InMaxHp;
	CurrentHp = InMaxHp;
	Armor = InArmor;
	TeamId = InTeamId;
	bAlive = true;
}

float URTSHealthComponent::ApplyTypedDamage(float BaseDamage, ERTSDamageType DamageType,
                                            ERTSFaction AttackerFaction, AActor* Attacker)
{
	if (!bAlive || BaseDamage <= 0.f)
	{
		return 0.f;
	}

	float Mul = RTSCore::DamageMultiplier(ToCore(DamageType), ToCore(Armor)) *
	            RTSCore::Mods(ToCore(AttackerFaction)).DamageMul;
	if (const UWorld* World = GetWorld())
	{
		if (const UGameInstance* GI = World->GetGameInstance())
		{
			if (const URTSDataSubsystem* Data = GI->GetSubsystem<URTSDataSubsystem>())
			{
				Mul = Data->GetDamageMultiplier(DamageType, Armor) *
				      Data->GetFaction(AttackerFaction).DamageMul;
			}
		}
	}

	const float Damage = BaseDamage * Mul;
	CurrentHp -= Damage;
	NotifyOwnerOfDamage(Attacker);

	if (CurrentHp <= 0.f)
	{
		CurrentHp = 0.f;
		bAlive = false;
		NotifyOwnerOfDeath(Attacker);
	}
	return Damage;
}

void URTSHealthComponent::AddHp(float Amount)
{
	if (bAlive)
	{
		CurrentHp = FMath::Clamp(CurrentHp + Amount, 0.f, MaxHp);
	}
}

void URTSHealthComponent::NotifyOwnerOfDamage(AActor* Attacker)
{
	if (AUnitBase* Unit = Cast<AUnitBase>(GetOwner()))
	{
		Unit->NotifyDamaged(Attacker);
	}
	else if (ABuildingBase* Building = Cast<ABuildingBase>(GetOwner()))
	{
		Building->NotifyDamaged(Attacker);
	}
}

void URTSHealthComponent::NotifyOwnerOfDeath(AActor* Killer)
{
	if (AUnitBase* Unit = Cast<AUnitBase>(GetOwner()))
	{
		Unit->HandleDeath(Killer);
	}
	else if (ABuildingBase* Building = Cast<ABuildingBase>(GetOwner()))
	{
		Building->HandleDeath(Killer);
	}
}
