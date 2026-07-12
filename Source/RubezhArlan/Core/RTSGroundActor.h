#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RTSGroundActor.generated.h"

class UStaticMeshComponent;

/**
 * «Бумажный» пол карты, создаваемый кодом при старте матча, если на уровне
 * нет своей земли. Позволяет играть даже на полностью пустом уровне —
 * TestMap из Python-скрипта не обязательна.
 */
UCLASS()
class RUBEZHARLAN_API ARTSGroundActor : public AActor
{
	GENERATED_BODY()

public:
	ARTSGroundActor();

	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, Category = "RTS")
	TObjectPtr<UStaticMeshComponent> GroundMesh;
};
