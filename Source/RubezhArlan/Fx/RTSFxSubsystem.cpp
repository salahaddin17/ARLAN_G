#include "Fx/RTSFxSubsystem.h"

void URTSFxSubsystem::AddDamageNumber(const FVector& WorldPos, float Amount, const FLinearColor& Color)
{
	if (Amount < 0.5f)
	{
		return;
	}
	if (Numbers.Num() >= MaxNumbers)
	{
		Numbers.RemoveAt(0);
	}
	FRTSDamageNumber Number;
	// лёгкий разброс, чтобы серия попаданий не сливалась в одну точку
	Number.WorldPos = WorldPos + FVector(FMath::FRandRange(-30.f, 30.f), FMath::FRandRange(-30.f, 30.f), 0.f);
	Number.Amount = Amount;
	Number.Color = Color;
	Numbers.Add(Number);
}

void URTSFxSubsystem::Advance(float DeltaSeconds)
{
	for (FRTSDamageNumber& Number : Numbers)
	{
		Number.Age += DeltaSeconds;
	}
	Numbers.RemoveAll([](const FRTSDamageNumber& Number) { return Number.Age >= NumberLifetime; });
}
