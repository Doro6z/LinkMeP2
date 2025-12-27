// RopeCameraManager.cpp
// Centralized camera management implementation

#include "RopeCameraManager.h"
#include "Components/CapsuleComponent.h"
#include "Curves/CurveFloat.h"
#include "GameFramework/Character.h"

URopeCameraManager::URopeCameraManager() {
  PrimaryComponentTick.bCanEverTick = true;
  PrimaryComponentTick.bStartWithTickEnabled = true;
}

void URopeCameraManager::BeginPlay() {
  Super::BeginPlay();

  // Create SpringArm if not already created
  AActor *Owner = GetOwner();
  if (!ensure(Owner))
    return;

  // Find or create SpringArm
  SpringArm = Owner->FindComponentByClass<USpringArmComponent>();
  if (!SpringArm) {
    SpringArm = NewObject<USpringArmComponent>(Owner, TEXT("CameraSpringArm"));
    if (SpringArm) {
      // Attach to capsule if character
      if (ACharacter *Character = Cast<ACharacter>(Owner)) {
        SpringArm->SetupAttachment(Character->GetCapsuleComponent());
      } else {
        SpringArm->SetupAttachment(Owner->GetRootComponent());
      }
      SpringArm->RegisterComponent();
    }
  }

  // Configure SpringArm
  if (SpringArm) {
    SpringArm->TargetArmLength = BaseArmLength;
    SpringArm->bUsePawnControlRotation = true;
    SpringArm->bEnableCameraLag = true;
    SpringArm->CameraLagSpeed = DefaultGroundedLag;
    CurrentLagSpeed = DefaultGroundedLag;
    SpringArm->bInheritPitch = true;
    SpringArm->bInheritYaw = true;
    SpringArm->bInheritRoll = false;
    SpringArm->SocketOffset = BaseSocketOffset;
    CurrentSocketOffset = BaseSocketOffset;
  }

  // Find or create Camera
  Camera = Owner->FindComponentByClass<UCameraComponent>();
  if (!Camera && SpringArm) {
    Camera = NewObject<UCameraComponent>(Owner, TEXT("Camera"));
    if (Camera) {
      Camera->SetupAttachment(SpringArm, USpringArmComponent::SocketName);
      Camera->bUsePawnControlRotation = false;
      Camera->RegisterComponent();
    }
  }

  // Store initial FOV
  if (Camera) {
    CurrentFOV = Camera->FieldOfView;
    BaseFOV = CurrentFOV;
  }
}

void URopeCameraManager::TickComponent(
    float DeltaTime, ELevelTick TickType,
    FActorComponentTickFunction *ThisTickFunction) {
  Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

  // Check for Hybrid Logic Switch
  if (bUseBlueprintCameraLogic) {
    // Delegate to Blueprint
    OnBlueprintUpdateCamera(DeltaTime);
    return;
  }

  UpdateCamera(DeltaTime);
  ApplyEffectLayers(DeltaTime);
  ClampPitch();

  // Update juice effects
  if (bEnableJuiceEffects) {
    UpdateJuiceEffects(DeltaTime);
  }

  // Draw Debug Info
  if (bShowJuiceDebug) {
    DrawDebugInfo();
  }
}

void URopeCameraManager::DrawDebugInfo() {
  if (!GEngine)
    return;

  FColor DebugColor = FColor::Cyan;
  float Duration = 0.f; // 1 frame

  // Camera State
  GEngine->AddOnScreenDebugMessage(
      1001, Duration, DebugColor,
      FString::Printf(TEXT("Camera State: %s"), *GetStateAsString()));

  // FOV Info
  GEngine->AddOnScreenDebugMessage(
      1002, Duration, DebugColor,
      FString::Printf(TEXT("FOV: %.1f (Base: %.1f | Delta: %.1f)"),
                      GetCurrentFOV(), BaseFOV, GetTotalFOVDelta()));

  // Lag Info
  GEngine->AddOnScreenDebugMessage(
      1003, Duration, DebugColor,
      FString::Printf(TEXT("Lag Speed: %.1f (Target: %.1f)"), CurrentLagSpeed,
                      (CurrentState == ECameraState::Swinging
                           ? DefaultSwingingLag
                           : DefaultGroundedLag)));

  // Active Layers
  FString LayersStr = TEXT("Layers: ");
  if (ActiveLayers.Num() == 0)
    LayersStr += TEXT("None");
  else {
    for (const FCameraEffectLayer &Layer : ActiveLayers) {
      LayersStr +=
          FString::Printf(TEXT("[%s: %.1f] "), *Layer.LayerID.ToString(),
                          Layer.FOVDelta * Layer.CurrentBlendAlpha);
    }
  }
  GEngine->AddOnScreenDebugMessage(1004, Duration, DebugColor, LayersStr);
}

void URopeCameraManager::SetState(ECameraState NewState) {
  if (CurrentState != NewState) {
    CurrentState = NewState;
    // State change could trigger different base camera settings in the future
  }
}

void URopeCameraManager::SetAiming(bool bAiming) { bIsAiming = bAiming; }

void URopeCameraManager::ToggleShoulderSwap() {
  bShoulderSwapped = !bShoulderSwapped;
}

void URopeCameraManager::AddEffect(const FCameraEffectLayer &Effect) {
  // Check if layer already exists
  for (FCameraEffectLayer &Layer : ActiveLayers) {
    if (Layer.LayerID == Effect.LayerID) {
      // Update existing layer
      Layer = Effect;
      return;
    }
  }

  // Add new layer
  ActiveLayers.Add(Effect);
}

void URopeCameraManager::RemoveEffect(FName LayerID) {
  ActiveLayers.RemoveAll([LayerID](const FCameraEffectLayer &Layer) {
    return Layer.LayerID == LayerID;
  });
}

bool URopeCameraManager::HasEffect(FName LayerID) const {
  for (const FCameraEffectLayer &Layer : ActiveLayers) {
    if (Layer.LayerID == LayerID) {
      return true;
    }
  }
  return false;
}

void URopeCameraManager::ApplyTransientEffect(FName LayerID, float FOVDelta,
                                              FVector PositionOffset,
                                              float Duration) {
  FCameraEffectLayer Effect;
  Effect.LayerID = LayerID;
  Effect.FOVDelta = FOVDelta;
  Effect.PositionOffset = PositionOffset;
  Effect.BlendWeight = 1.f;
  Effect.BlendSpeed = 20.f; // Fast blend-in

  AddEffect(Effect);

  // Timer to remove effect
  FTimerHandle TimerHandle;
  GetWorld()->GetTimerManager().SetTimer(
      TimerHandle, [this, LayerID]() { RemoveEffect(LayerID); }, Duration,
      false);
}

void URopeCameraManager::UpdateCamera(float DeltaTime) {
  if (!SpringArm || !Camera)
    return;

  // ========================================
  // State-Based Lag using Curves
  // ========================================
  float CurrentSpeed = 0.f;
  if (AActor *Owner = GetOwner()) {
    CurrentSpeed = Owner->GetVelocity().Size();
  }

  // Evaluate target lag from curve based on state
  float TargetLag = DefaultGroundedLag;
  float LagTransSpeed = LagTransitionToGroundSpeed;

  if (CurrentState == ECameraState::Swinging) {
    if (SwingingLagCurve) {
      TargetLag = SwingingLagCurve->GetFloatValue(CurrentSpeed);
    } else {
      TargetLag = DefaultSwingingLag;
    }
    LagTransSpeed = LagTransitionToSwingSpeed;
  } else {
    if (GroundedLagCurve) {
      TargetLag = GroundedLagCurve->GetFloatValue(CurrentSpeed);
    } else {
      TargetLag = DefaultGroundedLag;
    }
    LagTransSpeed = LagTransitionToGroundSpeed;
  }

  // Blend current lag to target
  CurrentLagSpeed =
      FMath::FInterpTo(CurrentLagSpeed, TargetLag, DeltaTime, LagTransSpeed);
  SpringArm->CameraLagSpeed = CurrentLagSpeed;

  // ========================================
  // Determine target values based on state and aiming
  // ========================================
  FVector TargetSocketOffset =
      bIsAiming ? AimingSocketOffset : BaseSocketOffset;
  float TargetFOV = bIsAiming ? AimingFOV : BaseFOV;

  // Apply shoulder swap
  if (bShoulderSwapped) {
    TargetSocketOffset.Y = -TargetSocketOffset.Y;
  }

  // Interpolate socket offset
  CurrentSocketOffset = FMath::VInterpTo(
      CurrentSocketOffset, TargetSocketOffset, DeltaTime, TransitionSpeed);

  // Interpolate FOV
  CurrentFOV =
      FMath::FInterpTo(CurrentFOV, TargetFOV, DeltaTime, TransitionSpeed);

  // Apply base values (before effects)
  SpringArm->SocketOffset = CurrentSocketOffset;
  Camera->FieldOfView = CurrentFOV;
}

void URopeCameraManager::ApplyEffectLayers(float DeltaTime) {
  if (!SpringArm || !Camera)
    return;

  // Calculate totals from all active layers
  float TotalFOVDelta = 0.f;
  FVector TotalPositionOffset = FVector::ZeroVector;
  FRotator TotalRotationOffset = FRotator::ZeroRotator;

  for (FCameraEffectLayer &Layer : ActiveLayers) {
    // Interpolate blend alpha
    Layer.CurrentBlendAlpha =
        FMath::FInterpTo(Layer.CurrentBlendAlpha, Layer.BlendWeight, DeltaTime,
                         Layer.BlendSpeed);

    // Accumulate effects (simple additive blend)
    TotalFOVDelta += Layer.FOVDelta * Layer.CurrentBlendAlpha;
    TotalPositionOffset += Layer.PositionOffset * Layer.CurrentBlendAlpha;
    TotalRotationOffset += Layer.RotationOffset * Layer.CurrentBlendAlpha;
  }

  // Apply deltas on top of base values
  Camera->FieldOfView = CurrentFOV + TotalFOVDelta;
  SpringArm->SocketOffset = CurrentSocketOffset + TotalPositionOffset;

  // Rotation offset applied as additional rotation
  if (!TotalRotationOffset.IsNearlyZero()) {
    Camera->SetRelativeRotation(TotalRotationOffset);
  } else {
    Camera->SetRelativeRotation(FRotator::ZeroRotator);
  }
}

void URopeCameraManager::ClampPitch() {
  APawn *OwnerPawn = Cast<APawn>(GetOwner());
  if (!OwnerPawn)
    return;

  AController *Controller = OwnerPawn->GetController();
  if (!Controller)
    return;

  FRotator ControlRot = Controller->GetControlRotation();
  ControlRot.Pitch = FMath::ClampAngle(ControlRot.Pitch, MinPitch, MaxPitch);
  Controller->SetControlRotation(ControlRot);
}

void URopeCameraManager::UpdateJuiceEffects(float DeltaTime) {
  UpdateHighSpeedEffect(DeltaTime);
  UpdateSwingApexEffect(DeltaTime);
}

void URopeCameraManager::UpdateHighSpeedEffect(float DeltaTime) {
  APawn *Owner = Cast<APawn>(GetOwner());
  if (!Owner)
    return;

  float Speed = Owner->GetVelocity().Size();
  float FOVBoost = 0.f;

  // Curve-based mapping (priority)
  if (bUseFOVCurve && SpeedToFOVCurve) {
    FOVBoost = SpeedToFOVCurve->GetFloatValue(Speed);
  }
  // Linear mapping (fallback)
  else if (Speed > SpeedThresholdForFOV) {
    float Alpha = FMath::GetMappedRangeValueClamped(
        FVector2D(SpeedThresholdForFOV, MaxSpeedForFOV), FVector2D(0.f, 1.f),
        Speed);
    FOVBoost = HighSpeedFOVBoost * Alpha;
  }

  // Apply or remove effect
  if (FOVBoost > 0.1f) {
    FCameraEffectLayer Effect(FName("HighSpeed"));
    Effect.FOVDelta = FOVBoost;
    Effect.BlendSpeed = 8.f;
    AddEffect(Effect);
  } else if (HasEffect(FName("HighSpeed"))) {
    RemoveEffect(FName("HighSpeed"));
  }
}

void URopeCameraManager::UpdateSwingApexEffect(float DeltaTime) {
  APawn *Owner = Cast<APawn>(GetOwner());
  if (!Owner)
    return;

  float CurrentZ = Owner->GetVelocity().Z;

  // Detect apex: velocity was positive (going up), now negative (falling)
  if (CurrentState == ECameraState::Swinging) {
    if (PreviousVerticalVelocity > 100.f && CurrentZ < -50.f) {
      // Apex detected - apply transient effect
      ApplyTransientEffect(FName("SwingApex"), -5.f, FVector(0, 0, 10.f), 0.3f);
    }
  }

  PreviousVerticalVelocity = CurrentZ;
}

// ===================================================================
// DEBUG API - Getters Implementation
// ===================================================================

float URopeCameraManager::GetCurrentFOV() const {
  return Camera ? Camera->FieldOfView : CurrentFOV;
}

FVector URopeCameraManager::GetCurrentSocketOffset() const {
  return CurrentSocketOffset;
}

TArray<FName> URopeCameraManager::GetActiveLayerIDs() const {
  TArray<FName> IDs;
  for (const FCameraEffectLayer &Layer : ActiveLayers) {
    IDs.Add(Layer.LayerID);
  }
  return IDs;
}

float URopeCameraManager::GetTotalFOVDelta() const {
  float Total = 0.f;
  for (const FCameraEffectLayer &Layer : ActiveLayers) {
    Total += Layer.FOVDelta * Layer.CurrentBlendAlpha;
  }
  return Total;
}

FString URopeCameraManager::GetStateAsString() const {
  switch (CurrentState) {
  case ECameraState::Grounded:
    return TEXT("Grounded");
  case ECameraState::Swinging:
    return TEXT("Swinging");
  default:
    return TEXT("Unknown");
  }
}
