// Menus/RespawnMenuWidget.cpp

#include "RespawnMenuWidget.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Pawn.h"

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
        // Find a valid spawn point
        AActor* StartSpot = GM->FindPlayerStart(PC);
        if (StartSpot)
        {
            // If player has a pawn, just teleport them (User Request)
            if (APawn* MyPawn = PC->GetPawn())
            {
                MyPawn->TeleportTo(StartSpot->GetActorLocation(), StartSpot->GetActorRotation());
                
                // Reset velocity to prevent weird momentum preservation
                if (auto* MoveComp = MyPawn->FindComponentByClass<UCharacterMovementComponent>())
                {
                    MoveComp->StopMovementImmediately();
                }
            }
            else
            {
                // If no pawn (dead/unpossessed), do a full restart
                GM->RestartPlayer(PC);
            }
        }
    }
}
