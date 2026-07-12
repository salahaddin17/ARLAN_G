#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Data/RTSTypes.h"
#include "RTSFogSubsystem.generated.h"

/**
 * Туман войны игрока (команда 0), два уровня:
 *   0 — не разведано, 1 — разведано (память), 2 — видно сейчас.
 * Сетка 64×64 поверх карты ±MapHalfSize. Пересчёт 4 раза в секунду из
 * ARTSGameMode::Tick (Step). Скрывает вражеские акторы:
 *   юниты — видны только на «видно сейчас», здания — навсегда после разведки.
 * Команда 1 (ИИ) видит всё — как в эталонном прототипе.
 */
UCLASS()
class RUBEZHARLAN_API URTSFogSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	static constexpr int32 GridSize = 64;

	URTSFogSubsystem();

	void Step(float DeltaSeconds);

	/** Видимость точки для команды: ИИ видит всё, игрок — по сетке. */
	bool IsVisibleFor(uint8 ViewerTeam, const FVector& WorldPos) const;

	/** Разведана ли точка игроком (для размещения зданий). */
	bool IsExplored(const FVector& WorldPos) const;

	uint8 CellAt(int32 X, int32 Y) const
	{
		return (X >= 0 && Y >= 0 && X < GridSize && Y < GridSize) ? Cells[Y * GridSize + X] : 0;
	}

	/** Доля разведанной карты (для HUD/отладки). */
	float GetExploredFraction() const;

	static void WorldToTile(const FVector& WorldPos, int32& OutX, int32& OutY);

private:
	void Recompute();
	void StampVision(const FVector& Center, int32 VisionTiles);
	void ApplyActorVisibility();

	uint8 Cells[GridSize * GridSize] = {};
	float Accum = 0.f;
};
