#include "Components/RTSHealthComponent.h"
#include "RubezhArlan.h"
#include "Data/RTSDataSubsystem.h"
#include "Units/UnitBase.h"
#include "Buildings/BuildingBase.h"
#include "Fx/RTSFxSubsystem.h"
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

	// летящая цифра урона (цвет по типу боеприпаса)
	if (UWorld* World = GetWorld())
	{
		if (URTSFxSubsystem* Fx = World->GetSubsystem<URTSFxSubsystem>())
		{
			FLinearColor Color;
			switch (DamageType)
			{
			case ERTSDamageType::Rocket:  Color = FLinearColor(1.f, 0.62f, 0.25f); break;
			case ERTSDamageType::Shell:   Color = FLinearColor(0.95f, 0.4f, 0.25f); break;
			case ERTSDamageType::Defense: Color = FLinearColor(0.55f, 0.85f, 0.8f); break;
			default:                      Color = FLinearColor(0.92f, 0.9f, 0.82f); break;
			}
			if (GetOwner())
			{
				Fx->AddDamageNumber(GetOwner()->GetActorLocation() + FVector(0, 0, 120.f), Damage, Color);
			}
		}
	}

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
