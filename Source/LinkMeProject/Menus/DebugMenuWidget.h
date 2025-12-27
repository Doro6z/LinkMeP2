// Menus/DebugMenuWidget.h

#pragma once

#include "../CharacterRope.h" // Needed for EMonkeyGait
#include "Blueprint/UserWidget.h"
#include "CoreMinimal.h"
#include "DebugMenuWidget.generated.h"

/**
 * Base class for the Debug Menu
 */
UCLASS()
class LINKMEPROJECT_API UDebugMenuWidget : public UUserWidget {
  GENERATED_BODY()

public:
  // Master debug toggle - overrides individual settings when active
  UFUNCTION(BlueprintCallable, Category = "LinkMe|Debug")
  void ToggleAllDebug(bool bEnable);

  // Individual component debug toggles (disabled when AllDebug is active)
  UFUNCTION(BlueprintCallable, Category = "LinkMe|Debug")
  void ToggleRopeSystemDebug(bool bEnable);

  UFUNCTION(BlueprintCallable, Category = "LinkMe|Debug")
  void ToggleRopeRenderDebug(bool bEnable);

  UFUNCTION(BlueprintCallable, Category = "LinkMe|Debug")
  void ToggleAimingDebug(bool bEnable);

  UFUNCTION(BlueprintCallable, Category = "LinkMe|Debug")
  void ToggleHookChargeDebug(bool bEnable);

  UFUNCTION(BlueprintCallable, Category = "LinkMe|Debug")
  void ToggleGodMode(bool bEnable);

  // Toggles GEngine->AddOnScreenDebugMessage visibility
  UFUNCTION(BlueprintCallable, Category = "LinkMe|Debug")
  void ToggleScreenMessages(bool bEnable);

  UFUNCTION(BlueprintCallable, Category = "LinkMe|Debug")
  void ToggleRopeVisibility(bool bVisible);

  UFUNCTION(BlueprintCallable, Category = "LinkMe|Debug")
  void ToggleCameraDebug(bool bEnable);

  // --- TIME DILATION ---
  UFUNCTION(BlueprintCallable, Category = "LinkMe|Debug")
  void SetGlobalTimeDilation(float Value);

  UFUNCTION(BlueprintPure, Category = "LinkMe|Debug")
  float GetGlobalTimeDilation() const;

  // --- GRAVITY ---
  UFUNCTION(BlueprintCallable, Category = "LinkMe|Debug")
  void SetCharacterGravityScale(float Value);

  UFUNCTION(BlueprintPure, Category = "LinkMe|Debug")
  float GetCharacterGravityScale() const;

  // --- CHARACTER SPEED ---
  // Forward declare enum if needed, or include CharacterRope.h
  // Since EMonkeyGait is in CharacterRope.h, we need to make sure it's visible.
  // Ideally, we should use the same enum or valid integers if we can't include.
  // But CharacterRope.h is Project API, so we can include it or forward
  // declare. Actually, EMonkeyGait is a UENUM in CharacterRope.h. We need to
  // include "CharacterRope.h" in cpp or h. For header, forward declaration of
  // enum class is tricky if it's UENUM. Best to include "CharacterRope.h" or
  // use specific header if isolated. For now, let's assume we can include it or
  // use uint8. But to be clean in header, we'll try to use the enum if
  // included, or just forward declare the class? Wait, EMonkeyGait is needed
  // for signature. Let's include "CharacterRope.h" in header OR use int/uint8
  // as simple params if we want to avoid circular deps (though DebugMenu
  // depends on Character, not vice versa usually). I will check imports.
  // DebugMenuWidget.cpp includes DebugMenuWidget.h. DebugMenuWidget.h doesn't
  // include CharacterRope.h yet.

  UFUNCTION(BlueprintCallable, Category = "LinkMe|Debug")
  void SetQuadrupedSpeed(EMonkeyGait Gait, float Value);

  UFUNCTION(BlueprintPure, Category = "LinkMe|Debug")
  float GetQuadrupedSpeed(EMonkeyGait Gait) const;

  UFUNCTION(BlueprintCallable, Category = "LinkMe|Debug")
  void SetBipedSpeed(EMonkeyGait Gait, float Value);

  UFUNCTION(BlueprintPure, Category = "LinkMe|Debug")
  float GetBipedSpeed(EMonkeyGait Gait) const;

  // --- VISUALIZATION ---
  UFUNCTION(BlueprintCallable, Category = "LinkMe|Debug")
  void ToggleCollisionViewer(bool bEnable);

  // --- MAP SWITCHER ---
  UFUNCTION(BlueprintCallable, Category = "LinkMe|Debug")
  void OpenMap(FName MapName);

private:
  bool bAllDebugActive = false;
};
