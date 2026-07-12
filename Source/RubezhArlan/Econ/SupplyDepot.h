#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SupplyDepot.generated.h"

class UStaticMeshComponent;

/** Склад припасов на карте. Конечный запас; грузовики забирают партиями. */
UCLASS()
class RUBEZHARLAN_API ASupplyDepot : public AActor
{
	GENERATED_BODY()

public:
	ASupplyDepot();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	void InitDepot(float InAmount);

	/** Забрать до Want припасов; возвращает фактически забранное. */
	float TakeSupplies(float Want);

	bool HasSupplies() const { return Amount > 0.f; }
	float GetAmount() const { return Amount; }
	float GetFraction() const { return InitialAmount > 0.f ? Amount / InitialAmount : 0.f; }

	UPROPERTY(EditAnywhere, Category = "RTS")
	float Amount = 2600.f;

	UPROPERTY(VisibleAnywhere, Category = "RTS")
	float InitialAmount = 2600.f;

	UPROPERTY(VisibleAnywhere, Category = "RTS")
	TObjectPtr<UStaticMeshComponent> CrateMesh;
};
