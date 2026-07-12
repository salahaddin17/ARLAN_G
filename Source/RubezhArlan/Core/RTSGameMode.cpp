#include "Core/RTSGameMode.h"
#include "RubezhArlan.h"
#include "Core/RTSCameraPawn.h"
#include "Core/RTSPlayerController.h"
#include "Core/RTSHUD.h"
#include "Units/UnitBase.h"
#include "Units/RTSAIController.h"
#include "Buildings/BuildingBase.h"
#include "Econ/RTSEconomySubsystem.h"
#include "Econ/SupplyDepot.h"
#include "Fog/RTSFogSubsystem.h"
#include "AI/EnemyCommander.h"
#include "Core/RTSGroundActor.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/DirectionalLight.h"
#include "Components/LightComponent.h"
#include "Engine/World.h"

ARTSGameMode::ARTSGameMode()
{
	PrimaryActorTick.bCanEverTick = true;
	DefaultPawnClass = ARTSCameraPawn::StaticClass();
	PlayerControllerClass = ARTSPlayerController::StaticClass();
	HUDClass = ARTSHUD::StaticClass();
}

void ARTSGameMode::StartPlay()
{
	AGameModeBase::StartPlay();
	EnsureWorldDressing();
	Phase = ERTSMatchPhase::Setup;
	if (bSkipSetupMenu)
	{
		ConfirmStart();
	}
	UE_LOG(LogRubezh, Log, TEXT("РУБЕЖ: АРЛАН — ожидание старта (меню)"));
}

/**
 * Самодостаточность на любом уровне: если сцена пустая (нет пола/света,
 * как на Untitled), игра создаёт «бумажную» землю и два направленных
 * источника света сама. TestMap из Python-скрипта добавляет NavMesh и
 * пост-процесс, но для игры больше не обязательна.
 */
void ARTSGameMode::EnsureWorldDressing()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	World->SpawnActor<ARTSGroundActor>(FVector::ZeroVector, FRotator::ZeroRotator, Params);

	// ключевой свет + мягкий заполняющий с другой стороны
	ADirectionalLight* Sun = World->SpawnActor<ADirectionalLight>(
		FVector(0, 0, 2000.f), FRotator(-55.f, 35.f, 0.f), Params);
	if (Sun && Sun->GetLightComponent())
	{
		Sun->GetLightComponent()->SetMobility(EComponentMobility::Movable);
		Sun->GetLightComponent()->SetIntensity(6.f);
	}
	ADirectionalLight* Fill = World->SpawnActor<ADirectionalLight>(
		FVector(0, 0, 2000.f), FRotator(-35.f, 215.f, 0.f), Params);
	if (Fill && Fill->GetLightComponent())
	{
		Fill->GetLightComponent()->SetMobility(EComponentMobility::Movable);
		Fill->GetLightComponent()->SetIntensity(1.5f);
		Fill->GetLightComponent()->SetCastShadows(false);
	}
}

void ARTSGameMode::SetPlayerFaction(ERTSFaction Faction)
{
	if (Phase == ERTSMatchPhase::Setup)
	{
		PlayerFaction = Faction;
	}
}

void ARTSGameMode::SetDifficulty(ERTSDifficulty InDifficulty)
{
	if (Phase == ERTSMatchPhase::Setup)
	{
		Difficulty = InDifficulty;
	}
}

void ARTSGameMode::ConfirmStart()
{
	if (Phase != ERTSMatchPhase::Setup)
	{
		return;
	}
	if (URTSEconomySubsystem* Econ = GetWorld()->GetSubsystem<URTSEconomySubsystem>())
	{
		Econ->SetupMatch(PlayerFaction, Difficulty);
	}
	SpawnWorldContent();
	Phase = ERTSMatchPhase::Playing;
	UE_LOG(LogRubezh, Log, TEXT("РУБЕЖ: АРЛАН — матч начат"));
}

void ARTSGameMode::RestartMatch()
{
	UGameplayStatics::OpenLevel(this, FName(TEXT("TestMap")));
}

void ARTSGameMode::Tick(float DeltaSeconds)
{
	AGameModeBase::Tick(DeltaSeconds);

	UWorld* World = GetWorld();
	if (!World || Phase != ERTSMatchPhase::Playing)
	{
		return;
	}
	URTSEconomySubsystem* Econ = World->GetSubsystem<URTSEconomySubsystem>();
	if (Econ)
	{
		Econ->Step(DeltaSeconds);
		if (!bInitialOrdersGiven && Econ->GetMatchTime() > 0.5f)
		{
			bInitialOrdersGiven = true;
			GiveInitialOrders();
		}
	}
	if (URTSFogSubsystem* Fog = World->GetSubsystem<URTSFogSubsystem>())
	{
		Fog->Step(DeltaSeconds);
	}
	if (Econ && Econ->IsMatchEnded())
	{
		Phase = ERTSMatchPhase::Ended;
	}
}

// --- спавн контента --------------------------------------------------------------

ABuildingBase* ARTSGameMode::SpawnBuildingAt(const FVector& Location, ERTSBuildingKind Kind,
                                             uint8 TeamId, bool bCompleted)
{
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ABuildingBase* Building = GetWorld()->SpawnActor<ABuildingBase>(Location, FRotator::ZeroRotator, Params);
	if (Building)
	{
		Building->InitBuilding(Kind, TeamId, bCompleted);
	}
	return Building;
}

AUnitBase* ARTSGameMode::SpawnUnitAt(const FVector& Location, ERTSUnitKind Kind, uint8 TeamId)
{
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
	AUnitBase* Unit = GetWorld()->SpawnActor<AUnitBase>(Location + FVector(0, 0, 100.f), FRotator::ZeroRotator, Params);
	if (Unit)
	{
		Unit->InitUnit(Kind, TeamId);
	}
	return Unit;
}

void ARTSGameMode::SpawnDepotAt(const FVector& Location, float Amount)
{
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ASupplyDepot* Depot = GetWorld()->SpawnActor<ASupplyDepot>(Location + FVector(0, 0, 45.f), FRotator::ZeroRotator, Params);
	if (Depot)
	{
		Depot->InitDepot(Amount);
	}
}

void ARTSGameMode::SpawnWorldContent()
{
	using namespace RTSCore;

	// Базы по диагонали: игрок — юго-запад, ИИ — северо-восток.
	const FVector PlayerBase(-2100.f, -2100.f, 0.f);
	const FVector EnemyBase(2100.f, 2100.f, 0.f);

	for (uint8 Team = 0; Team <= 1; ++Team)
	{
		const FVector Base = Team == 0 ? PlayerBase : EnemyBase;
		const FVector ToCenter = (FVector::ZeroVector - Base).GetSafeNormal2D();

		SpawnBuildingAt(Base, ERTSBuildingKind::HQ, Team, true);

		const FVector Muster = Base + ToCenter * 550.f;
		const FVector Side = FVector(-ToCenter.Y, ToCenter.X, 0.f);
		SpawnUnitAt(Muster - Side * 200.f, ERTSUnitKind::Technician, Team);
		SpawnUnitAt(Muster + Side * 200.f, ERTSUnitKind::Truck, Team);
		SpawnUnitAt(Muster - Side * 60.f, ERTSUnitKind::Rifleman, Team);
		SpawnUnitAt(Muster + Side * 60.f, ERTSUnitKind::Rifleman, Team);

		// склады у базы
		SpawnDepotAt(Base + ToCenter * 900.f + Side * 500.f, float(Econ::DepotNearBase));
		SpawnDepotAt(Base - ToCenter * 200.f + Side * 900.f, float(Econ::DepotNearBase));
	}

	// центральные и угловые склады
	SpawnDepotAt(FVector(-250.f, 250.f, 0.f), float(Econ::DepotCenter));
	SpawnDepotAt(FVector(300.f, -300.f, 0.f), float(Econ::DepotCenter));
	SpawnDepotAt(FVector(-2300.f, 2300.f, 0.f), float(Econ::DepotCenter));
	SpawnDepotAt(FVector(2300.f, -2300.f, 0.f), float(Econ::DepotCenter));

	// ИИ-командир
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	GetWorld()->SpawnActor<AEnemyCommander>(EnemyBase, FRotator::ZeroRotator, Params);
}

void ARTSGameMode::GiveInitialOrders()
{
	// стартовые грузовики обеих сторон — сразу в работу
	URTSEconomySubsystem* Econ = GetWorld()->GetSubsystem<URTSEconomySubsystem>();
	if (!Econ)
	{
		return;
	}
	for (AUnitBase* Unit : Econ->GetAllUnits())
	{
		if (!IsValid(Unit) || Unit->UnitKind != ERTSUnitKind::Truck)
		{
			continue;
		}
		ARTSAIController* Controller = Unit->GetRTSController();
		if (Controller && Controller->IsIdle())
		{
			if (ASupplyDepot* Depot = Econ->FindNearestDepotWithSupplies(Unit->GetActorLocation()))
			{
				Controller->SetOrder(FRTSOrder::MakeHarvest(Depot));
			}
		}
	}
}
