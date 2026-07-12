#include "Core/RTSPlayerController.h"
#include "RubezhArlan.h"
#include "Core/RTSCameraPawn.h"
#include "Units/UnitBase.h"
#include "Units/RTSAIController.h"
#include "Buildings/BuildingBase.h"
#include "Components/RTSHealthComponent.h"
#include "Components/RTSProductionComponent.h"
#include "Data/RTSDataSubsystem.h"
#include "Econ/RTSEconomySubsystem.h"
#include "Econ/SupplyDepot.h"
#include "Fog/RTSFogSubsystem.h"
#include "Components/InputComponent.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "DrawDebugHelpers.h"

namespace
{
	constexpr float EdgeScrollMargin = 14.f;   // пикселей от края
	constexpr float CameraSpeedBase = 2400.f;  // UU/с на среднем зуме
	constexpr float ClickPickRadius = 90.f;    // UU
}

ARTSPlayerController::ARTSPlayerController()
{
	bShowMouseCursor = true;
	bEnableClickEvents = true;
	bEnableMouseOverEvents = true;
}

void ARTSPlayerController::BeginPlay()
{
	APlayerController::BeginPlay();
	FInputModeGameAndUI Mode;
	Mode.SetHideCursorDuringCapture(false);
	Mode.SetLockMouseToViewportBehavior(EMouseLockMode::LockAlways);
	SetInputMode(Mode);
}

void ARTSPlayerController::SetupInputComponent()
{
	APlayerController::SetupInputComponent();
	if (!InputComponent)
	{
		return;
	}
	InputComponent->BindKey(EKeys::LeftMouseButton, IE_Pressed, this, &ARTSPlayerController::OnLeftPressed);
	InputComponent->BindKey(EKeys::LeftMouseButton, IE_Released, this, &ARTSPlayerController::OnLeftReleased);
	InputComponent->BindKey(EKeys::RightMouseButton, IE_Pressed, this, &ARTSPlayerController::OnRightPressed);
	InputComponent->BindKey(EKeys::MouseScrollUp, IE_Pressed, this, &ARTSPlayerController::OnWheelUp);
	InputComponent->BindKey(EKeys::MouseScrollDown, IE_Pressed, this, &ARTSPlayerController::OnWheelDown);
	InputComponent->BindKey(EKeys::S, IE_Pressed, this, &ARTSPlayerController::OnStopKey);
	InputComponent->BindKey(EKeys::A, IE_Pressed, this, &ARTSPlayerController::OnAttackMoveKey);
	InputComponent->BindKey(EKeys::Escape, IE_Pressed, this, &ARTSPlayerController::OnEscapeKey);
	InputComponent->BindKey(EKeys::H, IE_Pressed, this, &ARTSPlayerController::OnFocusBaseKey);
	InputComponent->BindKey(EKeys::Q, IE_Pressed, this, &ARTSPlayerController::OnActionQ);
	InputComponent->BindKey(EKeys::W, IE_Pressed, this, &ARTSPlayerController::OnActionW);
	InputComponent->BindKey(EKeys::E, IE_Pressed, this, &ARTSPlayerController::OnActionE);
	InputComponent->BindKey(EKeys::R, IE_Pressed, this, &ARTSPlayerController::OnActionR);
	InputComponent->BindKey(EKeys::T, IE_Pressed, this, &ARTSPlayerController::OnActionT);
	InputComponent->BindKey(EKeys::One, IE_Pressed, this, &ARTSPlayerController::OnDigit1);
	InputComponent->BindKey(EKeys::Two, IE_Pressed, this, &ARTSPlayerController::OnDigit2);
	InputComponent->BindKey(EKeys::Three, IE_Pressed, this, &ARTSPlayerController::OnDigit3);
	InputComponent->BindKey(EKeys::Four, IE_Pressed, this, &ARTSPlayerController::OnDigit4);
	InputComponent->BindKey(EKeys::Five, IE_Pressed, this, &ARTSPlayerController::OnDigit5);
	InputComponent->BindKey(EKeys::Six, IE_Pressed, this, &ARTSPlayerController::OnDigit6);
	InputComponent->BindKey(EKeys::Seven, IE_Pressed, this, &ARTSPlayerController::OnDigit7);
	InputComponent->BindKey(EKeys::Eight, IE_Pressed, this, &ARTSPlayerController::OnDigit8);
	InputComponent->BindKey(EKeys::Nine, IE_Pressed, this, &ARTSPlayerController::OnDigit9);
}

void ARTSPlayerController::OnDigit1() { HandleGroupDigit(1); }
void ARTSPlayerController::OnDigit2() { HandleGroupDigit(2); }
void ARTSPlayerController::OnDigit3() { HandleGroupDigit(3); }
void ARTSPlayerController::OnDigit4() { HandleGroupDigit(4); }
void ARTSPlayerController::OnDigit5() { HandleGroupDigit(5); }
void ARTSPlayerController::OnDigit6() { HandleGroupDigit(6); }
void ARTSPlayerController::OnDigit7() { HandleGroupDigit(7); }
void ARTSPlayerController::OnDigit8() { HandleGroupDigit(8); }
void ARTSPlayerController::OnDigit9() { HandleGroupDigit(9); }
void ARTSPlayerController::OnActionQ() { HandleActionSlot(0); }
void ARTSPlayerController::OnActionW() { HandleActionSlot(1); }
void ARTSPlayerController::OnActionE() { HandleActionSlot(2); }
void ARTSPlayerController::OnActionR() { HandleActionSlot(3); }
void ARTSPlayerController::OnActionT() { HandleActionSlot(4); }

// --- каждый кадр -----------------------------------------------------------------

void ARTSPlayerController::PlayerTick(float DeltaSeconds)
{
	APlayerController::PlayerTick(DeltaSeconds);

	// стартовый фокус на своей базе
	if (!bInitialCameraSet)
	{
		if (URTSEconomySubsystem* Econ = GetWorld() ? GetWorld()->GetSubsystem<URTSEconomySubsystem>() : nullptr)
		{
			if (ABuildingBase* HQ = Econ->FindNearestHQ(0, FVector::ZeroVector))
			{
				if (ARTSCameraPawn* Cam = GetCameraPawn())
				{
					Cam->CenterOn(HQ->GetActorLocation());
					bInitialCameraSet = true;
				}
			}
		}
	}

	TickCamera(DeltaSeconds);
	PruneSelection();
	if (bPlacing)
	{
		DrawPlacementGhost();
	}
}

ARTSCameraPawn* ARTSPlayerController::GetCameraPawn() const
{
	return Cast<ARTSCameraPawn>(GetPawn());
}

void ARTSPlayerController::TickCamera(float DeltaSeconds)
{
	ARTSCameraPawn* Cam = GetCameraPawn();
	if (!Cam)
	{
		return;
	}

	FVector2D Dir = FVector2D::ZeroVector;

	// WASD и стрелки. A/S заняты приказами ТОЛЬКО при активном выделении.
	const bool bOrdersContext = SelectedUnits.Num() > 0;
	if (IsInputKeyDown(EKeys::W) || IsInputKeyDown(EKeys::Up)) { Dir.X += 1.f; }
	if (IsInputKeyDown(EKeys::Down)) { Dir.X -= 1.f; }
	if (IsInputKeyDown(EKeys::S) && !bOrdersContext) { Dir.X -= 1.f; }
	if (IsInputKeyDown(EKeys::D) || IsInputKeyDown(EKeys::Right)) { Dir.Y += 1.f; }
	if (IsInputKeyDown(EKeys::Left)) { Dir.Y -= 1.f; }
	if (IsInputKeyDown(EKeys::A) && !bOrdersContext) { Dir.Y -= 1.f; }

	// края экрана
	float MouseX, MouseY;
	int32 ViewX, ViewY;
	GetViewportSize(ViewX, ViewY);
	if (GetMousePosition(MouseX, MouseY) && ViewX > 0 && ViewY > 0)
	{
		if (MouseX <= EdgeScrollMargin) { Dir.Y -= 1.f; }
		if (MouseX >= ViewX - EdgeScrollMargin) { Dir.Y += 1.f; }
		if (MouseY <= EdgeScrollMargin) { Dir.X += 1.f; }
		if (MouseY >= ViewY - EdgeScrollMargin) { Dir.X -= 1.f; }
	}

	if (!FMath::IsNearlyZero(Dir.Size()))
	{
		// скорость растёт с высотой камеры
		const float ZoomK = Cam->GetArmLength() / 3200.f;
		const float Speed = CameraSpeedBase * FMath::Clamp(ZoomK, 0.5f, 2.2f);
		// X экрана-верх — это мировой +X (камера смотрит вдоль +X при yaw 0)
		Cam->PanWorld(FVector2D(Dir.X, Dir.Y) * Speed * DeltaSeconds);
	}
}

// --- курсор → мир ------------------------------------------------------------------

FVector2D ARTSPlayerController::GetMouseScreen() const
{
	float X = 0.f, Y = 0.f;
	GetMousePosition(X, Y);
	return FVector2D(X, Y);
}

bool ARTSPlayerController::CursorToGround(FVector& OutPoint) const
{
	FVector Origin, Dir;
	if (!DeprojectMousePositionToWorld(Origin, Dir))
	{
		return false;
	}
	if (FMath::Abs(float(Dir.Z)) < 1e-4f)
	{
		return false;
	}
	const float T = float(-Origin.Z / Dir.Z); // пересечение с плоскостью Z=0
	if (T < 0.f)
	{
		return false;
	}
	OutPoint = Origin + Dir * T;
	OutPoint.Z = 0.f;
	return true;
}

AUnitBase* ARTSPlayerController::FindOwnUnitNear(const FVector& Point, float MaxDist) const
{
	URTSEconomySubsystem* Econ = GetWorld() ? GetWorld()->GetSubsystem<URTSEconomySubsystem>() : nullptr;
	if (!Econ)
	{
		return nullptr;
	}
	AUnitBase* Best = nullptr;
	float BestDist = MaxDist;
	for (AUnitBase* Unit : Econ->GetAllUnits())
	{
		if (!IsValid(Unit) || Unit->TeamId != 0)
		{
			continue;
		}
		const float D = float(FVector::Dist2D(Point, Unit->GetActorLocation()));
		if (D < BestDist)
		{
			BestDist = D;
			Best = Unit;
		}
	}
	return Best;
}

AUnitBase* ARTSPlayerController::FindEnemyUnitNear(const FVector& Point, float MaxDist) const
{
	UWorld* World = GetWorld();
	URTSEconomySubsystem* Econ = World ? World->GetSubsystem<URTSEconomySubsystem>() : nullptr;
	URTSFogSubsystem* Fog = World ? World->GetSubsystem<URTSFogSubsystem>() : nullptr;
	if (!Econ)
	{
		return nullptr;
	}
	AUnitBase* Best = nullptr;
	float BestDist = MaxDist;
	for (AUnitBase* Unit : Econ->GetAllUnits())
	{
		if (!IsValid(Unit) || Unit->TeamId == 0)
		{
			continue;
		}
		if (Fog && !Fog->IsVisibleFor(0, Unit->GetActorLocation()))
		{
			continue;
		}
		const float D = float(FVector::Dist2D(Point, Unit->GetActorLocation()));
		if (D < BestDist)
		{
			BestDist = D;
			Best = Unit;
		}
	}
	return Best;
}

ABuildingBase* ARTSPlayerController::FindBuildingAt(const FVector& Point, int32 TeamFilter) const
{
	URTSEconomySubsystem* Econ = GetWorld() ? GetWorld()->GetSubsystem<URTSEconomySubsystem>() : nullptr;
	if (!Econ)
	{
		return nullptr;
	}
	for (ABuildingBase* Building : Econ->GetAllBuildings())
	{
		if (!IsValid(Building))
		{
			continue;
		}
		if (TeamFilter >= 0 && Building->TeamId != uint8(TeamFilter))
		{
			continue;
		}
		// вражеские здания кликабельны, только если разведаны
		if (Building->TeamId != 0 && !Building->bSeenByPlayer)
		{
			continue;
		}
		const FVector2D Extents = Building->GetFootprintExtents();
		const FVector Delta = Point - Building->GetActorLocation();
		if (FMath::Abs(float(Delta.X)) <= Extents.X + 30.f && FMath::Abs(float(Delta.Y)) <= Extents.Y + 30.f)
		{
			return Building;
		}
	}
	return nullptr;
}

ASupplyDepot* ARTSPlayerController::FindDepotNear(const FVector& Point, float MaxDist) const
{
	URTSEconomySubsystem* Econ = GetWorld() ? GetWorld()->GetSubsystem<URTSEconomySubsystem>() : nullptr;
	if (!Econ)
	{
		return nullptr;
	}
	for (ASupplyDepot* Depot : Econ->GetAllDepots())
	{
		if (IsValid(Depot) && Depot->HasSupplies() &&
			FVector::Dist2D(Point, Depot->GetActorLocation()) <= MaxDist)
		{
			return Depot;
		}
	}
	return nullptr;
}

// --- ЛКМ: выделение / подтверждение -----------------------------------------------

void ARTSPlayerController::OnLeftPressed()
{
	if (bPlacing)
	{
		ConfirmPlacement();
		return;
	}
	if (bAttackMoveArmed)
	{
		FVector Point;
		if (CursorToGround(Point))
		{
			IssueAttackMove(Point);
		}
		bAttackMoveArmed = false;
		return;
	}
	bSelecting = true;
	SelectStartScreen = GetMouseScreen();
}

void ARTSPlayerController::OnLeftReleased()
{
	if (!bSelecting)
	{
		return;
	}
	bSelecting = false;
	const FVector2D End = GetMouseScreen();
	const bool bAdditive = IsInputKeyDown(EKeys::LeftShift) || IsInputKeyDown(EKeys::RightShift);
	if ((End - SelectStartScreen).Size() > 8.f)
	{
		SelectInMarquee(bAdditive);
	}
	else
	{
		SelectAtCursor(bAdditive);
	}
}

void ARTSPlayerController::SelectAtCursor(bool bAdditive)
{
	FVector Point;
	if (!CursorToGround(Point))
	{
		return;
	}
	if (!bAdditive)
	{
		ClearSelection();
	}

	if (AUnitBase* Unit = FindOwnUnitNear(Point, ClickPickRadius))
	{
		if (bAdditive && SelectedUnits.Contains(TWeakObjectPtr<AUnitBase>(Unit)))
		{
			SelectedUnits.Remove(TWeakObjectPtr<AUnitBase>(Unit));
			Unit->SetSelected(false);
		}
		else
		{
			SelectedUnits.AddUnique(TWeakObjectPtr<AUnitBase>(Unit));
			Unit->SetSelected(true);
		}
		SelectedBuilding.Reset();
		return;
	}
	if (ABuildingBase* Building = FindBuildingAt(Point, 0))
	{
		SelectedBuilding = Building;
		return;
	}
}

void ARTSPlayerController::SelectInMarquee(bool bAdditive)
{
	if (!bAdditive)
	{
		ClearSelection();
	}
	URTSEconomySubsystem* Econ = GetWorld() ? GetWorld()->GetSubsystem<URTSEconomySubsystem>() : nullptr;
	if (!Econ)
	{
		return;
	}
	const FVector2D End = GetMouseScreen();
	const float MinX = FMath::Min(SelectStartScreen.X, End.X);
	const float MaxX = FMath::Max(SelectStartScreen.X, End.X);
	const float MinY = FMath::Min(SelectStartScreen.Y, End.Y);
	const float MaxY = FMath::Max(SelectStartScreen.Y, End.Y);

	for (AUnitBase* Unit : Econ->GetAllUnits())
	{
		if (!IsValid(Unit) || Unit->TeamId != 0)
		{
			continue;
		}
		FVector2D Screen;
		if (ProjectWorldLocationToScreen(Unit->GetActorLocation(), Screen) &&
			Screen.X >= MinX && Screen.X <= MaxX && Screen.Y >= MinY && Screen.Y <= MaxY)
		{
			SelectedUnits.AddUnique(TWeakObjectPtr<AUnitBase>(Unit));
			Unit->SetSelected(true);
		}
	}
	if (SelectedUnits.Num() > 0)
	{
		SelectedBuilding.Reset();
	}
}

void ARTSPlayerController::ClearSelection()
{
	for (const TWeakObjectPtr<AUnitBase>& Weak : SelectedUnits)
	{
		if (AUnitBase* Unit = Weak.Get())
		{
			Unit->SetSelected(false);
		}
	}
	SelectedUnits.Reset();
	SelectedBuilding.Reset();
}

void ARTSPlayerController::PruneSelection()
{
	SelectedUnits.RemoveAll([](const TWeakObjectPtr<AUnitBase>& Weak) { return !Weak.IsValid(); });
}

bool ARTSPlayerController::HasTechnicianSelected() const
{
	for (const TWeakObjectPtr<AUnitBase>& Weak : SelectedUnits)
	{
		if (const AUnitBase* Unit = Weak.Get())
		{
			if (Unit->UnitKind == ERTSUnitKind::Technician)
			{
				return true;
			}
		}
	}
	return false;
}

// --- ПКМ: контекстные приказы ---------------------------------------------------------

void ARTSPlayerController::OnRightPressed()
{
	if (bPlacing)
	{
		bPlacing = false; // отмена стройки
		return;
	}
	if (bAttackMoveArmed)
	{
		bAttackMoveArmed = false;
		return;
	}
	FVector Point;
	if (CursorToGround(Point))
	{
		IssueContextOrder(Point);
	}
}

TArray<FVector> ARTSPlayerController::MakeFormationOffsets(int32 Count) const
{
	TArray<FVector> Out;
	Out.Add(FVector::ZeroVector);
	const float Spacing = 160.f;
	int32 Ring = 1;
	while (Out.Num() < Count)
	{
		const int32 PerRing = Ring * 6;
		for (int32 I = 0; I < PerRing && Out.Num() < Count; ++I)
		{
			const float Angle = (float(I) / PerRing) * 6.2831853f + Ring * 0.5f;
			Out.Add(FVector(FMath::Cos(Angle) * Ring * Spacing, FMath::Sin(Angle) * Ring * Spacing, 0.f));
		}
		++Ring;
	}
	return Out;
}

void ARTSPlayerController::IssueContextOrder(const FVector& Point)
{
	// только здание выделено → точка сбора
	if (SelectedUnits.Num() == 0)
	{
		if (ABuildingBase* Building = SelectedBuilding.Get())
		{
			if (Building->Production && Building->Production->CanProduceAnything())
			{
				Building->RallyPoint = Point;
				DrawDebugCylinder(GetWorld(), Point, Point + FVector(0, 0, 6), 40.f, 12,
				                  FColor(120, 170, 80), false, 0.6f, 0, 3.f);
			}
		}
		return;
	}

	TArray<AUnitBase*> Units;
	for (const TWeakObjectPtr<AUnitBase>& Weak : SelectedUnits)
	{
		if (AUnitBase* Unit = Weak.Get())
		{
			Units.Add(Unit);
		}
	}
	if (Units.Num() == 0)
	{
		return;
	}

	// 1) враг под курсором → атака
	AActor* Enemy = FindEnemyUnitNear(Point, ClickPickRadius);
	if (!Enemy)
	{
		ABuildingBase* EnemyBuilding = FindBuildingAt(Point, 1);
		Enemy = EnemyBuilding;
	}
	if (Enemy)
	{
		for (AUnitBase* Unit : Units)
		{
			if (ARTSAIController* C = Unit->GetRTSController())
			{
				C->SetOrder(Unit->IsCombatUnit()
					? FRTSOrder::MakeAttackTarget(Enemy)
					: FRTSOrder::MakeMove(Point));
			}
		}
		DrawDebugCylinder(GetWorld(), Point, Point + FVector(0, 0, 6), 45.f, 12, FColor(200, 70, 45), false, 0.6f, 0, 3.f);
		return;
	}

	// 2) склад → грузовики возят
	if (ASupplyDepot* Depot = FindDepotNear(Point, 220.f))
	{
		bool bAnyTruck = false;
		for (AUnitBase* Unit : Units)
		{
			if (Unit->UnitKind == ERTSUnitKind::Truck)
			{
				if (ARTSAIController* C = Unit->GetRTSController())
				{
					C->SetOrder(FRTSOrder::MakeHarvest(Depot));
					bAnyTruck = true;
				}
			}
		}
		if (bAnyTruck)
		{
			for (AUnitBase* Unit : Units)
			{
				if (Unit->UnitKind != ERTSUnitKind::Truck)
				{
					if (ARTSAIController* C = Unit->GetRTSController())
					{
						C->SetOrder(FRTSOrder::MakeMove(Point));
					}
				}
			}
			return;
		}
	}

	// 3) своё недостроенное/повреждённое здание → техники строят/чинят
	if (ABuildingBase* Own = FindBuildingAt(Point, 0))
	{
		if (!Own->IsCompleted() || Own->NeedsRepair())
		{
			bool bAnyTech = false;
			for (AUnitBase* Unit : Units)
			{
				if (Unit->UnitKind == ERTSUnitKind::Technician)
				{
					if (ARTSAIController* C = Unit->GetRTSController())
					{
						C->SetOrder(FRTSOrder::MakeBuild(Own));
						bAnyTech = true;
					}
				}
			}
			if (bAnyTech)
			{
				return;
			}
		}
	}

	// 4) обычное движение «коробочкой»
	const TArray<FVector> Offsets = MakeFormationOffsets(Units.Num());
	for (int32 I = 0; I < Units.Num(); ++I)
	{
		if (ARTSAIController* C = Units[I]->GetRTSController())
		{
			C->SetOrder(FRTSOrder::MakeMove(Point + Offsets[I]));
		}
	}
	DrawDebugCylinder(GetWorld(), Point, Point + FVector(0, 0, 6), 40.f, 12, FColor(120, 170, 80), false, 0.6f, 0, 3.f);
}

void ARTSPlayerController::IssueAttackMove(const FVector& Point)
{
	const TArray<FVector> Offsets = MakeFormationOffsets(SelectedUnits.Num());
	int32 I = 0;
	for (const TWeakObjectPtr<AUnitBase>& Weak : SelectedUnits)
	{
		AUnitBase* Unit = Weak.Get();
		if (Unit && Unit->IsCombatUnit())
		{
			if (ARTSAIController* C = Unit->GetRTSController())
			{
				C->SetOrder(FRTSOrder::MakeAttackMove(Point + Offsets[FMath::Min(I, Offsets.Num() - 1)]));
			}
		}
		++I;
	}
	DrawDebugCylinder(GetWorld(), Point, Point + FVector(0, 0, 6), 50.f, 12, FColor(200, 70, 45), false, 0.7f, 0, 3.f);
}

// --- клавиши-приказы --------------------------------------------------------------------

void ARTSPlayerController::OnStopKey()
{
	for (const TWeakObjectPtr<AUnitBase>& Weak : SelectedUnits)
	{
		if (AUnitBase* Unit = Weak.Get())
		{
			if (ARTSAIController* C = Unit->GetRTSController())
			{
				C->SetOrder(FRTSOrder{});
			}
		}
	}
	bAttackMoveArmed = false;
}

void ARTSPlayerController::OnAttackMoveKey()
{
	// арм только если есть боевые юниты в выделении
	for (const TWeakObjectPtr<AUnitBase>& Weak : SelectedUnits)
	{
		if (const AUnitBase* Unit = Weak.Get())
		{
			if (Unit->IsCombatUnit())
			{
				bAttackMoveArmed = true;
				bPlacing = false;
				return;
			}
		}
	}
}

void ARTSPlayerController::OnEscapeKey()
{
	if (bPlacing)
	{
		bPlacing = false;
	}
	else if (bAttackMoveArmed)
	{
		bAttackMoveArmed = false;
	}
	else
	{
		ClearSelection();
	}
}

void ARTSPlayerController::OnFocusBaseKey()
{
	if (URTSEconomySubsystem* Econ = GetWorld() ? GetWorld()->GetSubsystem<URTSEconomySubsystem>() : nullptr)
	{
		if (ABuildingBase* HQ = Econ->FindNearestHQ(0, FVector::ZeroVector))
		{
			if (ARTSCameraPawn* Cam = GetCameraPawn())
			{
				Cam->CenterOn(HQ->GetActorLocation());
			}
		}
	}
}

void ARTSPlayerController::OnWheelUp()
{
	if (ARTSCameraPawn* Cam = GetCameraPawn())
	{
		Cam->Zoom(1.f);
	}
}

void ARTSPlayerController::OnWheelDown()
{
	if (ARTSCameraPawn* Cam = GetCameraPawn())
	{
		Cam->Zoom(-1.f);
	}
}

// --- группы Ctrl+1..9 ------------------------------------------------------------------

void ARTSPlayerController::HandleGroupDigit(int32 Digit)
{
	const bool bCtrl = IsInputKeyDown(EKeys::LeftControl) || IsInputKeyDown(EKeys::RightControl);
	if (bCtrl)
	{
		TArray<TWeakObjectPtr<AUnitBase>> Group = SelectedUnits;
		ControlGroups.Add(Digit, Group);
		return;
	}
	TArray<TWeakObjectPtr<AUnitBase>>* Group = ControlGroups.Find(Digit);
	if (!Group)
	{
		return;
	}
	Group->RemoveAll([](const TWeakObjectPtr<AUnitBase>& Weak) { return !Weak.IsValid(); });
	if (Group->Num() == 0)
	{
		return;
	}
	ClearSelection();
	FVector Center = FVector::ZeroVector;
	for (const TWeakObjectPtr<AUnitBase>& Weak : *Group)
	{
		if (AUnitBase* Unit = Weak.Get())
		{
			SelectedUnits.AddUnique(Weak);
			Unit->SetSelected(true);
			Center += Unit->GetActorLocation();
		}
	}
	// повторный вызов той же группы центрирует камеру (простая версия: всегда)
	if (SelectedUnits.Num() > 0)
	{
		Center = Center / float(SelectedUnits.Num());
		if (IsInputKeyDown(EKeys::SpaceBar))
		{
			if (ARTSCameraPawn* Cam = GetCameraPawn())
			{
				Cam->CenterOn(Center);
			}
		}
	}
}

// --- контекстные действия Q/W/E/R/T ---------------------------------------------------------

void ARTSPlayerController::HandleActionSlot(int32 Slot)
{
	// производство выделенного здания
	if (ABuildingBase* Building = SelectedBuilding.Get())
	{
		if (Building->IsCompleted() && Building->Production)
		{
			static const ERTSUnitKind HQSlots[] = {ERTSUnitKind::Technician, ERTSUnitKind::Truck};
			static const ERTSUnitKind BarracksSlots[] = {ERTSUnitKind::Rifleman, ERTSUnitKind::Rocketeer};
			static const ERTSUnitKind FactorySlots[] = {ERTSUnitKind::Scout, ERTSUnitKind::Tank, ERTSUnitKind::Artillery};

			const ERTSUnitKind* Slots = nullptr;
			int32 SlotCount = 0;
			switch (Building->BuildingKind)
			{
			case ERTSBuildingKind::HQ:       Slots = HQSlots; SlotCount = 2; break;
			case ERTSBuildingKind::Barracks: Slots = BarracksSlots; SlotCount = 2; break;
			case ERTSBuildingKind::Factory:  Slots = FactorySlots; SlotCount = 3; break;
			default: break;
			}
			if (Slots && Slot < SlotCount)
			{
				Building->Production->TryEnqueue(Slots[Slot]);
			}
			return;
		}
	}

	// стройменю техника
	if (HasTechnicianSelected())
	{
		static const ERTSBuildingKind BuildSlots[] = {
			ERTSBuildingKind::Power, ERTSBuildingKind::Barracks, ERTSBuildingKind::Factory,
			ERTSBuildingKind::Turret, ERTSBuildingKind::HQ,
		};
		if (Slot < 5)
		{
			StartPlacement(BuildSlots[Slot]);
		}
	}
}

// --- размещение зданий -------------------------------------------------------------------------

void ARTSPlayerController::StartPlacement(ERTSBuildingKind Kind)
{
	PlacementKind = Kind;
	bPlacing = true;
	bAttackMoveArmed = false;
}

FVector ARTSPlayerController::SnapToTileCenter(const FVector& Point, int32 SizeX, int32 SizeY) const
{
	const float Tile = RTSCore::UUPerTile;
	// у чётных размеров центр на узле сетки, у нечётных — в центре клетки
	auto Snap = [Tile](float V, int32 Size) -> float
	{
		if (Size % 2 == 0)
		{
			return FMath::RoundToInt(V / Tile) * Tile;
		}
		return FMath::FloorToInt(V / Tile) * Tile + Tile * 0.5f;
	};
	return FVector(Snap(float(Point.X), SizeX), Snap(float(Point.Y), SizeY), 0.f);
}

void ARTSPlayerController::DrawPlacementGhost()
{
	UWorld* World = GetWorld();
	URTSDataSubsystem* Data = World && World->GetGameInstance()
		? World->GetGameInstance()->GetSubsystem<URTSDataSubsystem>() : nullptr;
	URTSEconomySubsystem* Econ = World ? World->GetSubsystem<URTSEconomySubsystem>() : nullptr;
	URTSFogSubsystem* Fog = World ? World->GetSubsystem<URTSFogSubsystem>() : nullptr;
	FVector Point;
	if (!Data || !Econ || !CursorToGround(Point))
	{
		return;
	}
	const FBuildingRow& Row = Data->GetBuilding(PlacementKind);
	const FVector Center = SnapToTileCenter(Point, Row.SizeX, Row.SizeY);
	const FVector2D HalfExtents(Row.SizeX * RTSCore::UUPerTile * 0.5f, Row.SizeY * RTSCore::UUPerTile * 0.5f);

	const bool bFree = Econ->IsPlacementFree(Center, HalfExtents) &&
		(!Fog || Fog->IsExplored(Center)) &&
		Econ->GetCredits(0) >= Data->GetBuildingCost(PlacementKind, Econ->GetFactionOf(0));

	DrawDebugBox(World, Center + FVector(0, 0, 60), FVector(HalfExtents.X, HalfExtents.Y, 60.f),
	             bFree ? FColor(90, 180, 80) : FColor(200, 60, 40), false, -1.f, 0, 6.f);
}

void ARTSPlayerController::ConfirmPlacement()
{
	UWorld* World = GetWorld();
	URTSDataSubsystem* Data = World && World->GetGameInstance()
		? World->GetGameInstance()->GetSubsystem<URTSDataSubsystem>() : nullptr;
	URTSEconomySubsystem* Econ = World ? World->GetSubsystem<URTSEconomySubsystem>() : nullptr;
	URTSFogSubsystem* Fog = World ? World->GetSubsystem<URTSFogSubsystem>() : nullptr;
	FVector Point;
	if (!Data || !Econ || !CursorToGround(Point))
	{
		bPlacing = false;
		return;
	}

	const FBuildingRow& Row = Data->GetBuilding(PlacementKind);
	const FVector Center = SnapToTileCenter(Point, Row.SizeX, Row.SizeY);
	const FVector2D HalfExtents(Row.SizeX * RTSCore::UUPerTile * 0.5f, Row.SizeY * RTSCore::UUPerTile * 0.5f);

	if (!Econ->IsPlacementFree(Center, HalfExtents) || (Fog && !Fog->IsExplored(Center)))
	{
		return; // клик по красному призраку — остаёмся в режиме
	}
	const int32 Cost = Data->GetBuildingCost(PlacementKind, Econ->GetFactionOf(0));
	if (!Econ->TrySpend(0, Cost))
	{
		return;
	}

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ABuildingBase* Site = World->SpawnActor<ABuildingBase>(Center, FRotator::ZeroRotator, Params);
	if (!Site)
	{
		Econ->AddCredits(0, Cost);
		return;
	}
	Site->InitBuilding(PlacementKind, 0, false);

	// все выделенные техники — на стройку
	for (const TWeakObjectPtr<AUnitBase>& Weak : SelectedUnits)
	{
		AUnitBase* Unit = Weak.Get();
		if (Unit && Unit->UnitKind == ERTSUnitKind::Technician)
		{
			if (ARTSAIController* C = Unit->GetRTSController())
			{
				C->SetOrder(FRTSOrder::MakeBuild(Site));
			}
		}
	}

	// Shift — серийное строительство
	if (!(IsInputKeyDown(EKeys::LeftShift) || IsInputKeyDown(EKeys::RightShift)))
	{
		bPlacing = false;
	}
}
