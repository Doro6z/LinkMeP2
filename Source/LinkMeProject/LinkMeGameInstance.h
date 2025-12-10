// LinkMeGameInstance.h

#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "LinkMeGameInstance.generated.h"

/**
 * GameInstance for global state management (Menus, Loading, etc.)
 */
UCLASS()
class LINKMEPROJECT_API ULinkMeGameInstance : public UGameInstance
{
	GENERATED_BODY()

public:
	// --- Menus ---
	
	// Event to request showing the Respawn Menu (Implemented in BP)
	UFUNCTION(BlueprintImplementableEvent, BlueprintCallable, Category = "LinkMe|UI")
	void OnShowRespawnMenu();

	// Event to request showing the Debug Menu (Implemented in BP)
	UFUNCTION(BlueprintImplementableEvent, BlueprintCallable, Category = "LinkMe|UI")
	void OnShowDebugMenu();

    // Event to Hide all menus
    UFUNCTION(BlueprintImplementableEvent, BlueprintCallable, Category = "LinkMe|UI")
    void OnHideAllMenus();
};
