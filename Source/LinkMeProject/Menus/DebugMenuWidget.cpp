// Menus/DebugMenuWidget.cpp

#include "DebugMenuWidget.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "Components/ActorComponent.h"

// Forward declarations for components (they should have bShowDebug properties)
#include "../RopeSystemComponent.h"
#include "../RopeRenderComponent.h"
#include "../AimingComponent.h"
#include "../HookChargeComponent.h"

void UDebugMenuWidget::ToggleAllDebug(bool bEnable)
{
    bAllDebugActive = bEnable;
    
    // Apply to all components
    ToggleRopeSystemDebug(bEnable);
    ToggleRopeRenderDebug(bEnable);
    ToggleAimingDebug(bEnable);
    ToggleHookChargeDebug(bEnable);
}

void UDebugMenuWidget::ToggleRopeSystemDebug(bool bEnable)
{
    // If master override is active and user tries to disable individual, reject
    if (bAllDebugActive && !bEnable) return;
    
    APlayerController* PC = GetOwningPlayer();
    if (!PC || !PC->GetPawn()) return;
    
    // Find RopeSystemComponent on player's pawn
    if (URopeSystemComponent* RopeComp = PC->GetPawn()->FindComponentByClass<URopeSystemComponent>())
    {
        RopeComp->bShowDebug = bEnable;
    }
}

void UDebugMenuWidget::ToggleRopeRenderDebug(bool bEnable)
{
    if (bAllDebugActive && !bEnable) return;
    
    APlayerController* PC = GetOwningPlayer();
    if (!PC || !PC->GetPawn()) return;
    
    if (URopeRenderComponent* RenderComp = PC->GetPawn()->FindComponentByClass<URopeRenderComponent>())
    {
        RenderComp->bShowDebugSpline = bEnable;
    }
}

void UDebugMenuWidget::ToggleAimingDebug(bool bEnable)
{
    if (bAllDebugActive && !bEnable) return;
    
    APlayerController* PC = GetOwningPlayer();
    if (!PC || !PC->GetPawn()) return;
    
    // Try to find any aiming component (could be TPSAimingComponent or base AimingComponent)
    TArray<UAimingComponent*> AimingComps;
    PC->GetPawn()->GetComponents<UAimingComponent>(AimingComps);
    
    for (UAimingComponent* AimComp : AimingComps)
    {
        // Assuming these components have a bShowDebug property
        // If they don't, we'll need to add it
        AimComp->bShowDebug = bEnable;
    }
}

void UDebugMenuWidget::ToggleHookChargeDebug(bool bEnable)
{
    if (bAllDebugActive && !bEnable) return;
    
    APlayerController* PC = GetOwningPlayer();
    if (!PC || !PC->GetPawn()) return;
    
    if (UHookChargeComponent* ChargeComp = PC->GetPawn()->FindComponentByClass<UHookChargeComponent>())
    {
        ChargeComp->bShowDebug = bEnable;
    }
}

void UDebugMenuWidget::ToggleGodMode(bool bEnable)
{
    if (APlayerController* PC = GetOwningPlayer())
    {
        // God mode is a toggle command
        PC->ConsoleCommand("God");
    }
}
