// Menus/DebugMenuWidget.h

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "DebugMenuWidget.generated.h"

/**
 * Base class for the Debug Menu
 */
UCLASS()
class LINKMEPROJECT_API UDebugMenuWidget : public UUserWidget
{
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

private:
    bool bAllDebugActive = false;
};
