// Menus/RespawnMenuWidget.cpp

#include "RespawnMenuWidget.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/GameModeBase.h"

void URespawnMenuWidget::RequestRespawn()
{
    // Call server to handle respawn
    ServerRequestRespawn();
    
    // Hide menu immediately on client
    RemoveFromParent();
}

void URespawnMenuWidget::ServerRequestRespawn_Implementation()
{
    APlayerController* PC = GetOwningPlayer();
    if (!PC) return;

    // Get the GameMode (only exists on server)
    if (AGameModeBase* GM = GetWorld()->GetAuthGameMode())
    {
        // Respawn the player at a PlayerStart
        GM->RestartPlayer(PC);
    }
}
