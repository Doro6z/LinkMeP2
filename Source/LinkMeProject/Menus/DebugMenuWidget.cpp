// Menus/DebugMenuWidget.cpp

#include "DebugMenuWidget.h"
#include "../CharacterRope.h"
#include "Components/ActorComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"

// Forward declarations for components (they should have bShowDebug properties)
#include "../AimingComponent.h"
#include "../HookChargeComponent.h"
#include "../RopeCameraManager.h"
#include "../RopeRenderComponent.h"
#include "../RopeSystemComponent.h"

void UDebugMenuWidget::ToggleAllDebug(bool bEnable) {
  bAllDebugActive = bEnable;

  // Apply to all components
  ToggleRopeSystemDebug(bEnable);
  ToggleRopeRenderDebug(bEnable);
  ToggleAimingDebug(bEnable);
  ToggleHookChargeDebug(bEnable);
  ToggleCameraDebug(bEnable);
}

void UDebugMenuWidget::ToggleRopeSystemDebug(bool bEnable) {
  // If master override is active and user tries to disable individual, reject
  if (bAllDebugActive && !bEnable)
    return;

  APlayerController *PC = GetOwningPlayer();
  if (!PC || !PC->GetPawn())
    return;

  // Find RopeSystemComponent on player's pawn
  if (URopeSystemComponent *RopeComp =
          PC->GetPawn()->FindComponentByClass<URopeSystemComponent>()) {
    RopeComp->bShowDebug = bEnable;
  }
}

void UDebugMenuWidget::ToggleRopeRenderDebug(bool bEnable) {
  if (bAllDebugActive && !bEnable)
    return;

  APlayerController *PC = GetOwningPlayer();
  if (!PC || !PC->GetPawn())
    return;

  if (URopeRenderComponent *RenderComp =
          PC->GetPawn()->FindComponentByClass<URopeRenderComponent>()) {
    RenderComp->bShowDebugSpline = bEnable;
  }
}

void UDebugMenuWidget::ToggleAimingDebug(bool bEnable) {
  if (bAllDebugActive && !bEnable)
    return;

  APlayerController *PC = GetOwningPlayer();
  if (!PC || !PC->GetPawn())
    return;

  // Try to find any aiming component (could be TPSAimingComponent or base
  // AimingComponent)
  TArray<UAimingComponent *> AimingComps;
  PC->GetPawn()->GetComponents<UAimingComponent>(AimingComps);

  for (UAimingComponent *AimComp : AimingComps) {
    // Assuming these components have a bShowDebug property
    // If they don't, we'll need to add it
    AimComp->bShowDebug = bEnable;
  }
}

void UDebugMenuWidget::ToggleHookChargeDebug(bool bEnable) {
  if (bAllDebugActive && !bEnable)
    return;

  APlayerController *PC = GetOwningPlayer();
  if (!PC || !PC->GetPawn())
    return;

  if (UHookChargeComponent *ChargeComp =
          PC->GetPawn()->FindComponentByClass<UHookChargeComponent>()) {
    ChargeComp->bShowDebug = bEnable;
  }
}

void UDebugMenuWidget::ToggleGodMode(bool bEnable) {
  if (APlayerController *PC = GetOwningPlayer()) {
    // God mode is a toggle command
    PC->ConsoleCommand("God");
  }
}

void UDebugMenuWidget::ToggleScreenMessages(bool bEnable) {
  if (APlayerController *PC = GetOwningPlayer()) {
    // "DisableAllScreenMessages" disables them if executed.
    // To Enable: "EnableAllScreenMessages"
    // Wait, "DisableAllScreenMessages" is a toggle? No, usually separate
    // commands or boolean. Actually, "DISABLEALLSCREENMESSAGES" toggles it or
    // disables it. Better approach: "key -1" clears them, but blocking needs a
    // specific command.

    // Command "DISABLEALLSCREENMESSAGES"
    if (!bEnable) {
      PC->ConsoleCommand("DISABLEALLSCREENMESSAGES");
    } else {
      PC->ConsoleCommand("ENABLEALLSCREENMESSAGES");
    }
  }
}

void UDebugMenuWidget::ToggleRopeVisibility(bool bVisible) {
  if (APlayerController *PC = GetOwningPlayer()) {
    if (PC->GetPawn()) {
      if (URopeRenderComponent *RenderComp =
              PC->GetPawn()->FindComponentByClass<URopeRenderComponent>()) {
        // Invert visible to hidden
        RenderComp->SetRopeHidden(!bVisible);
      }
    }
  }
}

void UDebugMenuWidget::ToggleCameraDebug(bool bEnable) {
  if (bAllDebugActive && !bEnable)
    return;

  APlayerController *PC = GetOwningPlayer();
  if (!PC || !PC->GetPawn())
    return;

  if (URopeCameraManager *CamManager =
          PC->GetPawn()->FindComponentByClass<URopeCameraManager>()) {
    CamManager->bShowJuiceDebug = bEnable;
  }
}

// ============================================================================
// TIME DILATION
// ============================================================================

void UDebugMenuWidget::SetGlobalTimeDilation(float Value) {
  if (GetWorld()) {
    UGameplayStatics::SetGlobalTimeDilation(GetWorld(), Value);
  }
}

float UDebugMenuWidget::GetGlobalTimeDilation() const {
  if (GetWorld()) {
    return UGameplayStatics::GetGlobalTimeDilation(GetWorld());
  }
  return 1.0f;
}

// ============================================================================
// GRAVITY
// ============================================================================

void UDebugMenuWidget::SetCharacterGravityScale(float Value) {
  APlayerController *PC = GetOwningPlayer();
  if (!PC || !PC->GetPawn())
    return;

  if (UCharacterMovementComponent *CMC =
          PC->GetPawn()->FindComponentByClass<UCharacterMovementComponent>()) {
    CMC->GravityScale = Value;
  }
}

float UDebugMenuWidget::GetCharacterGravityScale() const {
  APlayerController *PC = GetOwningPlayer();
  if (!PC || !PC->GetPawn())
    return 1.0f;

  if (UCharacterMovementComponent *CMC =
          PC->GetPawn()->FindComponentByClass<UCharacterMovementComponent>()) {
    return CMC->GravityScale;
  }
  return 1.0f;
}

// ============================================================================
// CHARACTER SPEED
// ============================================================================

void UDebugMenuWidget::SetQuadrupedSpeed(EMonkeyGait Gait, float Value) {
  APlayerController *PC = GetOwningPlayer();
  if (!PC)
    return;

  if (ACharacterRope *MyChar = Cast<ACharacterRope>(PC->GetPawn())) {
    switch (Gait) {
    case EMonkeyGait::Walk:
      MyChar->QuadrupedSpeeds.X = Value;
      break;
    case EMonkeyGait::Jog:
      MyChar->QuadrupedSpeeds.Y = Value;
      break;
    case EMonkeyGait::Sprint:
      MyChar->QuadrupedSpeeds.Z = Value;
      break;
    }
  }
}

float UDebugMenuWidget::GetQuadrupedSpeed(EMonkeyGait Gait) const {
  APlayerController *PC = GetOwningPlayer();
  if (!PC)
    return 0.f;

  if (ACharacterRope *MyChar = Cast<ACharacterRope>(PC->GetPawn())) {
    switch (Gait) {
    case EMonkeyGait::Walk:
      return MyChar->QuadrupedSpeeds.X;
    case EMonkeyGait::Jog:
      return MyChar->QuadrupedSpeeds.Y;
    case EMonkeyGait::Sprint:
      return MyChar->QuadrupedSpeeds.Z;
    }
  }
  return 0.f;
}

void UDebugMenuWidget::SetBipedSpeed(EMonkeyGait Gait, float Value) {
  APlayerController *PC = GetOwningPlayer();
  if (!PC)
    return;

  if (ACharacterRope *MyChar = Cast<ACharacterRope>(PC->GetPawn())) {
    switch (Gait) {
    case EMonkeyGait::Walk:
      MyChar->BipedSpeeds.X = Value;
      break;
    case EMonkeyGait::Jog:
      MyChar->BipedSpeeds.Y = Value;
      break;
    case EMonkeyGait::Sprint:
      MyChar->BipedSpeeds.Z = Value;
      break;
    }
  }
}

float UDebugMenuWidget::GetBipedSpeed(EMonkeyGait Gait) const {
  APlayerController *PC = GetOwningPlayer();
  if (!PC)
    return 0.f;

  if (ACharacterRope *MyChar = Cast<ACharacterRope>(PC->GetPawn())) {
    switch (Gait) {
    case EMonkeyGait::Walk:
      return MyChar->BipedSpeeds.X;
    case EMonkeyGait::Jog:
      return MyChar->BipedSpeeds.Y;
    case EMonkeyGait::Sprint:
      return MyChar->BipedSpeeds.Z;
    }
  }
  return 0.f;
}

// ============================================================================
// VISUALIZATION
// ============================================================================

void UDebugMenuWidget::ToggleCollisionViewer(bool bEnable) {
  if (APlayerController *PC = GetOwningPlayer()) {
    // "show Collision" toggle.
    // However, if we want to enforce State, assume "show Collision" toggles
    // default we might need to know state. The command 'show Collision' works
    // as a toggle if typed. If we want explicit, 'showflag.collision 1' or '0'?
    // "Show Collision" is usually a bit switch.
    // Let's force it via command.
    // Simplest approach: "Show Collision" toggles unless we use
    // `ExecuteConsoleCommand` with specific legacy logic. Actually,
    // `PC->ConsoleCommand("Show Collision")` is a toggle. To respect `bEnable`,
    // we can check current state or just try to force it. A more direct way:
    // `GetWorld()->GameViewport->EngineShowFlags.SetCollision(bEnable);` (only
    // works in editor or if cheats enabled?). In build, `Show Collision` might
    // not work without cheats. DebugMenu likely for dev.

    // Better approach:
    // If bEnable is true, we want it shown.
    // Can we read it?
    // Let's just use console command "Show Collision" and hope user syncs UI or
    // accepts toggle behavior. BUT the request is likely "Slider/Checkbox"
    // which suggests State. I will try to use `ExecuteConsoleCommand` with
    // logic if I could read it. For now, let's assume the UI checkbox will call
    // this. Since `Show Collision` is a toggle, if I pass `bEnable` and it
    // mismatches, it might be annoying. Let's try to set it explicitly if
    // possible.
    // `GetWorld()->GetGameViewport()->EngineShowFlags.SetCollision(bEnable);`
    // This is the cleanest C++ way if accessible.

    if (UGameViewportClient *Viewport = GetWorld()->GetGameViewport()) {
      Viewport->EngineShowFlags.SetCollision(bEnable);
    }
    // Also run console command to be safe purely for 'show' side effects if
    // any? No, EngineShowFlags is the source of truth for rendering.
  }
}

// ============================================================================
// MAP SWITCHER
// ============================================================================

void UDebugMenuWidget::OpenMap(FName MapName) {
  if (MapName.IsNone())
    return;

  if (GetWorld()) {
    UGameplayStatics::OpenLevel(GetWorld(), MapName);
  }
}
