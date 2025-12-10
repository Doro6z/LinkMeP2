// Menus/RespawnMenuWidget.h

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "RespawnMenuWidget.generated.h"

/**
 * Base class for the Respawn Menu
 */
UCLASS()
class LINKMEPROJECT_API URespawnMenuWidget : public UUserWidget
{
	GENERATED_BODY()

public:
    // Called when the user clicks "Respawn" button
	UFUNCTION(BlueprintCallable, Category = "LinkMe|UI")
	void RequestRespawn();

protected:
    // Server RPC to respawn the player
    UFUNCTION(Server, Reliable)
    void ServerRequestRespawn();
};
