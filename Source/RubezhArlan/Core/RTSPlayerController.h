#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "Data/RTSTypes.h"
#include "RTSPlayerController.generated.h"

class AUnitBase;
class ABuildingBase;
class ARTSCameraPawn;
class ASupplyDepot;

/**
 * Управление RTS: рамка ЛКМ, контекстные приказы ПКМ, A — атака-движение,
 * S — стоп, Ctrl+1..9 группы, WASD/стрелки/края — камера, колесо — зум,
 * Q/W/E/R/T — контекстные действия (производство/стройменю техника).
 * Ввод — прямой опрос клавиш + BindKey: ноль ассетов Enhanced Input.
 */
UCLASS()
class RUBEZHARLAN_API ARTSPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	ARTSPlayerController();

	virtual void BeginPlay() override;
	virtual void SetupInputComponent() override;
	virtual void PlayerTick(float DeltaSeconds) override;

	// --- чтение состояния (для HUD) -----------------------------------------
	bool IsSelecting() const { return bSelecting; }
	FVector2D GetSelectStart() const { return SelectStartScreen; }
	FVector2D GetMouseScreen() const;
	const TArray<TWeakObjectPtr<AUnitBase>>& GetSelectedUnits() const { return SelectedUnits; }
	ABuildingBase* GetSelectedBuilding() const { return SelectedBuilding.Get(); }
	bool IsAttackMoveArmed() const { return bAttackMoveArmed; }
	bool IsPlacing() const { return bPlacing; }
	ERTSBuildingKind GetPlacementKind() const { return PlacementKind; }
	bool HasTechnicianSelected() const;

private:
	// выделение
	TArray<TWeakObjectPtr<AUnitBase>> SelectedUnits;
	TWeakObjectPtr<ABuildingBase> SelectedBuilding;
	TMap<int32, TArray<TWeakObjectPtr<AUnitBase>>> ControlGroups;
	bool bSelecting = false;
	FVector2D SelectStartScreen = FVector2D::ZeroVector;

	// режимы
	bool bAttackMoveArmed = false;
	bool bPlacing = false;
	ERTSBuildingKind PlacementKind = ERTSBuildingKind::Power;
	bool bInitialCameraSet = false;
	bool bMinimapDrag = false;

	// --- ввод -----------------------------------------------------------------
	void OnLeftPressed();
	void OnLeftReleased();
	void OnRightPressed();
	void OnWheelUp();
	void OnWheelDown();
	void OnStopKey();
	void OnAttackMoveKey();
	void OnEscapeKey();
	void OnFocusBaseKey();
	void OnConfirmKey();
	void OnSelectArmyKey();
	void OnActionQ(); void OnActionW(); void OnActionE(); void OnActionR(); void OnActionT();
	void OnDigit1(); void OnDigit2(); void OnDigit3(); void OnDigit4(); void OnDigit5();
	void OnDigit6(); void OnDigit7(); void OnDigit8(); void OnDigit9();

	void HandleActionSlot(int32 Slot);
	void HandleGroupDigit(int32 Digit);

	// --- камера ------------------------------------------------------------------
	void TickCamera(float DeltaSeconds);
	ARTSCameraPawn* GetCameraPawn() const;

	// --- фаза матча и миникарта ------------------------------------------------
	class ARTSGameMode* GetRTSGameMode() const;
	bool IsMatchPlaying() const;
	/** true, если точка экрана в миникарте; OutWorld — мировая точка. */
	bool MinimapHit(float ScreenX, float ScreenY, FVector& OutWorld) const;

	// --- курсор/мир -----------------------------------------------------------------
	bool CursorToGround(FVector& OutPoint) const;
	AUnitBase* FindOwnUnitNear(const FVector& Point, float MaxDist) const;
	AUnitBase* FindEnemyUnitNear(const FVector& Point, float MaxDist) const;
	ABuildingBase* FindBuildingAt(const FVector& Point, int32 TeamFilter) const;
	ASupplyDepot* FindDepotNear(const FVector& Point, float MaxDist) const;

	// --- команды ----------------------------------------------------------------------
	void SelectAtCursor(bool bAdditive);
	void SelectInMarquee(bool bAdditive);
	void ClearSelection();
	void PruneSelection();
	void IssueContextOrder(const FVector& Point);
	void IssueAttackMove(const FVector& Point);
	TArray<FVector> MakeFormationOffsets(int32 Count) const;

	// --- стройка ------------------------------------------------------------------------
	void StartPlacement(ERTSBuildingKind Kind);
	void ConfirmPlacement();
	void DrawPlacementGhost();
	FVector SnapToTileCenter(const FVector& Point, int32 SizeX, int32 SizeY) const;
};
