#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "Data/RTSTypes.h"
#include "RTSHUD.generated.h"

class ARTSPlayerController;

/** Прямоугольник миникарты в экранных координатах. */
struct FRTSMinimapLayout
{
	float X = 0.f;
	float Y = 0.f;
	float Size = 220.f;

	bool Contains(float PX, float PY) const
	{
		return PX >= X && PX <= X + Size && PY >= Y && PY <= Y + Size;
	}
};

/**
 * HUD «штабной консоли»: стартовое меню, рамка выделения, панель ресурсов,
 * панель выделения, миникарта с туманом, финальный баннер ПОБЕДА/РАЗГРОМ.
 * Чистый Canvas — ноль UMG-ассетов. Ориентация миникарты совпадает с
 * камерой: экранный «верх» = мировой +X.
 */
UCLASS()
class RUBEZHARLAN_API ARTSHUD : public AHUD
{
	GENERATED_BODY()

public:
	virtual void DrawHUD() override;

	static FRTSMinimapLayout GetMinimapLayout(float ViewX, float ViewY);
	/** Пиксель миникарты → мировая точка (Z=0). */
	static FVector MinimapToWorld(const FRTSMinimapLayout& Layout, float PX, float PY);
	/** Мировая точка → пиксель миникарты. */
	static FVector2D WorldToMinimap(const FRTSMinimapLayout& Layout, const FVector& World);

private:
	void DrawSetupMenu();
	void DrawMarquee(ARTSPlayerController* RTSController);
	void DrawTopBar(ARTSPlayerController* RTSController);
	void DrawSelectionPanel(ARTSPlayerController* RTSController);
	void DrawMinimap();
	void DrawWorldBars(ARTSPlayerController* RTSController);
	void DrawDamageNumbers(ARTSPlayerController* RTSController);
	void DrawEndBanner();

	void DrawPanelRect(float X, float Y, float W, float H);
	void DrawHpBar(float X, float Y, float W, float Fraction);
};
