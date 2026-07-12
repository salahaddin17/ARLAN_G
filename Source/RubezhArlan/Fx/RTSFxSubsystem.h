#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "RTSFxSubsystem.generated.h"

/** Летящая цифра урона (мировая точка → экранная анимация в HUD). */
USTRUCT()
struct FRTSDamageNumber
{
	GENERATED_BODY()

	UPROPERTY() FVector WorldPos = FVector::ZeroVector;
	UPROPERTY() float Amount = 0.f;
	UPROPERTY() float Age = 0.f;
	UPROPERTY() FLinearColor Color = FLinearColor::White;
};

/**
 * Лента визуальных событий боя: сим складывает (урон), HUD рисует.
 * Чистые данные — ноль ассетов, порядок детерминированный.
 */
UCLASS()
class RUBEZHARLAN_API URTSFxSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	static constexpr float NumberLifetime = 0.9f;
	static constexpr int32 MaxNumbers = 96;

	void AddDamageNumber(const FVector& WorldPos, float Amount, const FLinearColor& Color);

	/** Старение и чистка; зовёт HUD раз в кадр. */
	void Advance(float DeltaSeconds);

	const TArray<FRTSDamageNumber>& GetNumbers() const { return Numbers; }

private:
	UPROPERTY() TArray<FRTSDamageNumber> Numbers;
};
