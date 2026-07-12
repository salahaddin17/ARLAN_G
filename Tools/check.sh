#!/usr/bin/env bash
# ============================================================================
# Псевдосборка модуля RubezhArlan clang'ом через мини-шим UE (Tools/shim).
# НЕ заменяет UnrealBuildTool — ловит ошибки C++ в нашем коде до реальной
# сборки. Использование: Tools/check.sh [файл.cpp ...]
# ============================================================================
set -u
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
SHIM="$ROOT/Tools/shim"
UE="$SHIM/ue"
GEN="$SHIM/gen"
SRC="$ROOT/Source/RubezhArlan"

mkdir -p "$GEN"

# 1. Стабы include-путей UE: каждый просто включает UEShimCore.h
STUBS=(
  "CoreMinimal.h"
  "Modules/ModuleManager.h"
  "GameFramework/Actor.h"
  "GameFramework/Pawn.h"
  "GameFramework/Character.h"
  "GameFramework/PlayerController.h"
  "GameFramework/Controller.h"
  "GameFramework/HUD.h"
  "GameFramework/GameModeBase.h"
  "GameFramework/PlayerStart.h"
  "GameFramework/CharacterMovementComponent.h"
  "GameFramework/SpringArmComponent.h"
  "GameFramework/PlayerInput.h"
  "Components/ActorComponent.h"
  "Components/SceneComponent.h"
  "Components/PrimitiveComponent.h"
  "Components/StaticMeshComponent.h"
  "Components/CapsuleComponent.h"
  "Components/SkeletalMeshComponent.h"
  "Components/InputComponent.h"
  "Camera/CameraComponent.h"
  "Engine/World.h"
  "Engine/GameInstance.h"
  "Engine/DataTable.h"
  "Engine/StaticMesh.h"
  "Engine/Font.h"
  "Engine/Canvas.h"
  "Engine/EngineTypes.h"
  "Engine/DirectionalLight.h"
  "Engine/SkyLight.h"
  "Components/LightComponent.h"
  "Materials/Material.h"
  "Materials/MaterialInterface.h"
  "Materials/MaterialInstanceDynamic.h"
  "Subsystems/GameInstanceSubsystem.h"
  "Subsystems/WorldSubsystem.h"
  "Kismet/GameplayStatics.h"
  "Misc/FileHelper.h"
  "Misc/Paths.h"
  "Misc/AutomationTest.h"
  "UObject/ConstructorHelpers.h"
  "AIController.h"
  "InputCoreTypes.h"
  "DrawDebugHelpers.h"
  "TimerManager.h"
  "EngineUtils.h"
)
for S in "${STUBS[@]}"; do
  mkdir -p "$UE/$(dirname "$S")"
  F="$UE/$S"
  if [ ! -f "$F" ]; then
    printf '#pragma once\n#include "UEShimCore.h"\n' > "$F"
  fi
done

# 2. Пустые *.generated.h для каждого заголовка модуля
find "$SRC" -name '*.h' | while read -r H; do
  BASE="$(basename "$H" .h)"
  G="$GEN/$BASE.generated.h"
  [ -f "$G" ] || printf '#pragma once\n' > "$G"
done

# 3. Компиляция
CXX="${CXX:-clang++}"
FLAGS=(-std=c++17 -fsyntax-only -x c++
  -Wall -Wextra
  -Werror=shadow -Werror=shadow-field
  -Wno-unused-parameter -Wno-unused-variable -Wno-unused-private-field
  -Wno-unused-function -Wno-missing-field-initializers
  -I "$SRC" -I "$UE" -I "$GEN"
  -DWITH_DEV_AUTOMATION_TESTS=1 -DWITH_EDITOR=0 -DUE_BUILD_DEVELOPMENT=1)

if [ "$#" -gt 0 ]; then
  FILES=("$@")
else
  mapfile -t FILES < <(find "$SRC" -name '*.cpp' | sort)
fi

FAIL=0
for F in "${FILES[@]}"; do
  OUT="$("$CXX" "${FLAGS[@]}" "$F" 2>&1)"
  if [ -n "$OUT" ]; then
    echo "== $F"
    echo "$OUT"
    if echo "$OUT" | grep -q 'error:'; then FAIL=1; fi
  fi
done

if [ "$FAIL" -eq 0 ]; then
  echo "SHIM-CHECK OK (${#FILES[@]} файлов)"
else
  echo "SHIM-CHECK FAILED"
  exit 1
fi
