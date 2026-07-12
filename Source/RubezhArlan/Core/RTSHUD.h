#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "Data/RTSTypes.h"
#include "RTSHUD.generated.h"

class ARTSPlayerController;

/**
 * HUD «штабной консоли»: рамка выделения, верхняя панель ресурсов,
 * панель выделения с контекстными подсказками, финальный баннер
 * ПОБЕДА/РАЗГРОМ со статистикой. Чистый Canvas — ноль UMG-ассетов.
 */
UCLASS()
class RUBEZHARLAN_API ARTSHUD : public AHUD
{
	GENERATED_BODY()

public:
	virtual void DrawHUD() override;

private:
	void DrawMarquee(ARTSPlayerController* RTSController);
	void DrawTopBar(ARTSPlayerController* RTSController);
	void DrawSelectionPanel(ARTSPlayerController* RTSController);
	void DrawEndBanner();

	void DrawPanelRect(float X, float Y, float W, float H);
};
