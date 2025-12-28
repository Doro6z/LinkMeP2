#include "Components/InertialMovementComponent.h"
#include "../CharacterRope.h"
#include "DrawDebugHelpers.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/KismetMathLibrary.h"
#include "Net/UnrealNetwork.h"

UInertialMovementComponent::UInertialMovementComponent() {
  PrimaryComponentTick.bCanEverTick = true;
  SetIsReplicatedByDefault(true);

  // Defaults
  LeanMultiplier = 0.05f;
  AccelerationTiltScale = 0.1f;
  BankingScale = 0.5f;
  LeanSpringStiffness = 50.0f;
  LeanSpringDamping = 0.8f; // Slightly under-damped for organic feel
}

FLookAtLimits
UInertialMovementComponent::GetCurrentLimits(bool bIsQuadruped) const {
  return bIsQuadruped ? QuadLimits : BipedLimits;
}

void UInertialMovementComponent::GetLifetimeReplicatedProps(
    TArray<FLifetimeProperty> &OutLifetimeProps) const {
  Super::GetLifetimeReplicatedProps(OutLifetimeProps);
  DOREPLIFETIME(UInertialMovementComponent, CurrentBodyInertia);
  DOREPLIFETIME(UInertialMovementComponent, CurrentHeadLook);
}

void UInertialMovementComponent::BeginPlay() {
  Super::BeginPlay();

  OwnerCharacter = Cast<ACharacter>(GetOwner());
  if (OwnerCharacter) {
    MovementComp = OwnerCharacter->GetCharacterMovement();
    PreviousVelocity = OwnerCharacter->GetVelocity();
    PreviousYaw = OwnerCharacter->GetActorRotation().Yaw;
    MeshYaw = PreviousYaw; // Initialize mesh yaw to match capsule
  }
}

void UInertialMovementComponent::TickComponent(
    float DeltaTime, ELevelTick TickType,
    FActorComponentTickFunction *ThisTickFunction) {
  Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

  // Authority check: Only calculate on owning client or server authority
  // Non-owning clients use replicated values from FInertiaState
  if (OwnerCharacter && !OwnerCharacter->IsLocallyControlled() &&
      !OwnerCharacter->HasAuthority()) {
    // We are a client looking at another player's character
    // Skip calculation - use replicated CurrentInertiaState values
    return;
  }

  UpdateInertiaPhysics(DeltaTime);
  UpdateInertiaPhysics(DeltaTime);
  UpdateProceduralTurn(DeltaTime);
  UpdateHeadLookAt(DeltaTime);

  // Debug display on screen
  if (bShowDebug && GEngine && OwnerCharacter) {
    const FVector Velocity = OwnerCharacter->GetVelocity();
    const FVector LocalVel =
        OwnerCharacter->GetActorTransform().InverseTransformVector(Velocity);
    const float Speed = Velocity.Size2D();
    const float Direction =
        FMath::RadiansToDegrees(FMath::Atan2(LocalVel.Y, LocalVel.X));

    GEngine->AddOnScreenDebugMessage(
        -1, 0.0f, FColor::Cyan, FString::Printf(TEXT("=== INERTIA DEBUG ===")));
    GEngine->AddOnScreenDebugMessage(
        -1, 0.0f, FColor::White,
        FString::Printf(TEXT("Speed: %.1f cm/s"), Speed));
    GEngine->AddOnScreenDebugMessage(
        -1, 0.0f, FColor::White,
        FString::Printf(TEXT("Direction: %.1f°"), Direction));
    GEngine->AddOnScreenDebugMessage(
        -1, 0.0f, FColor::Yellow,
        FString::Printf(TEXT("LocalVel: X=%.1f Y=%.1f Z=%.1f"), LocalVel.X,
                        LocalVel.Y, LocalVel.Z));
    GEngine->AddOnScreenDebugMessage(
        -1, 0.0f, FColor::Green,
        FString::Printf(TEXT("LeanRoll: %.2f°"), CurrentBodyInertia.LeanRoll));
    GEngine->AddOnScreenDebugMessage(
        -1, 0.0f, FColor::Orange,
        FString::Printf(TEXT("LeanPitch: %.2f°"),
                        CurrentBodyInertia.LeanPitch));
    GEngine->AddOnScreenDebugMessage(
        -1, 0.0f, FColor::Magenta,
        FString::Printf(TEXT("TorsoTwist: %.2f°"),
                        CurrentBodyInertia.TorsoTwistYaw));
    GEngine->AddOnScreenDebugMessage(
        -1, 0.0f, FColor::Cyan,
        FString::Printf(TEXT("HeadLookAtRot: P=%.1f Y=%.1f R=%.1f"),
                        CurrentHeadLook.HeadLookAtRotation.Pitch,
                        CurrentHeadLook.HeadLookAtRotation.Yaw,
                        CurrentHeadLook.HeadLookAtRotation.Roll));
    // Debug camera rotation to verify input
    GEngine->AddOnScreenDebugMessage(
        -1, 0.0f, FColor::White,
        FString::Printf(TEXT("CamPitch: %.2f° | CamYaw: %.2f°"),
                        OwnerCharacter->GetControlRotation().Pitch,
                        OwnerCharacter->GetControlRotation().Yaw));

    if (USkeletalMeshComponent *Mesh = OwnerCharacter->GetMesh()) {
      if (Mesh->DoesSocketExist(FName("head"))) {
        FRotator HeadBoneRot = Mesh->GetSocketRotation(FName("head"));
        GEngine->AddOnScreenDebugMessage(
            -1, 0.0f, FColor::Magenta,
            FString::Printf(TEXT("[BONE] Head World Rot: P=%.1f Y=%.1f R=%.1f"),
                            HeadBoneRot.Pitch, HeadBoneRot.Yaw,
                            HeadBoneRot.Roll));
      }
    }

    // === Debug Arrows (Phase 4.1) ===
    // Red arrow: Capsule forward direction
    DrawDebugDirectionalArrow(GetWorld(), OwnerCharacter->GetActorLocation(),
                              OwnerCharacter->GetActorLocation() +
                                  OwnerCharacter->GetActorForwardVector() *
                                      100.f,
                              10.f, FColor::Red, false, -1.f, 0, 2.f);

    // Blue arrow: Control rotation (camera direction)
    const FVector CamForwardDebug =
        OwnerCharacter->GetControlRotation().Vector();
    DrawDebugDirectionalArrow(
        GetWorld(), OwnerCharacter->GetActorLocation() + FVector(0, 0, 50),
        OwnerCharacter->GetActorLocation() + FVector(0, 0, 50) +
            CamForwardDebug * 100.f,
        10.f, FColor::Blue, false, -1.f, 0, 2.f);
  }
}

void UInertialMovementComponent::UpdateInertiaPhysics(float DeltaTime) {
  // Protect against very small DeltaTime (window focus loss, etc.)
  // ISSUE 1 FIX: Reset spring velocities on abnormal frames to prevent
  // accumulation
  if (!OwnerCharacter || DeltaTime <= 0.001f || DeltaTime > 0.5f) {
    // Reset spring velocities to avoid accumulation from bad frames
    CurrentRollVelocity = 0.f;
    CurrentPitchVelocity = 0.f;
    // Store current state to avoid derivative spike on resume
    if (OwnerCharacter) {
      PreviousVelocity = OwnerCharacter->GetVelocity();
      PreviousYaw = OwnerCharacter->GetActorRotation().Yaw;
    }
    return;
  }

  const FVector CurrentVelocity = OwnerCharacter->GetVelocity();
  const float CurrentYaw = OwnerCharacter->GetActorRotation().Yaw;

  // ---------------------------------------------------------
  // 1. Calculate Derivatives (Acceleration & Yaw Rate)
  // ---------------------------------------------------------

  const FVector AccelerationVector =
      (CurrentVelocity - PreviousVelocity) / DeltaTime;
  const FVector LocalAcceleration =
      OwnerCharacter->GetActorTransform().InverseTransformVector(
          AccelerationVector);

  float YawRate = (CurrentYaw - PreviousYaw) / DeltaTime;

  // Handle Rotator Wrap-around (e.g. 359 -> 1 creates a huge negative delta,
  // need to fix) If jump is > 180, it means we wrapped.
  const float WrapThreshold = 180.0f / DeltaTime;
  if (FMath::Abs(YawRate) > WrapThreshold) {
    if (YawRate > 0)
      YawRate -= 360.0f / DeltaTime;
    else
      YawRate += 360.0f / DeltaTime;
  }

  // ---------------------------------------------------------
  // 2. Target Calculation
  // ---------------------------------------------------------

  float TargetPitch = 0.f;
  float TargetRoll = 0.f;

  // Pitch: DotProduct-based Accel/Brake detection
  // Dot > 0 = Accelerating (pushing in velocity direction)
  // Dot < 0 = Braking (friction/input opposing velocity)
  float AccelBrakeDot = 0.f;
  bool bIsBraking = false;

  if (CurrentVelocity.SizeSquared() > 1.f &&
      AccelerationVector.SizeSquared() > 1.f) {
    AccelBrakeDot = FVector::DotProduct(CurrentVelocity.GetSafeNormal(),
                                        AccelerationVector.GetSafeNormal());
    bIsBraking = (AccelBrakeDot < -0.1f);
  }

  // Select multiplier based on state
  const float LeanMultiplierSelected =
      bIsBraking ? BrakingLeanMultiplier : AccelerationLeanMultiplier;

  // Apply lean: Negative pitch = lean forward, Positive = lean back
  // LocalAcceleration.X > 0 means accelerating forward
  TargetPitch =
      -LocalAcceleration.X * LeanMultiplierSelected * AccelerationTiltScale;

  // Clamp to max pitch
  TargetPitch = FMath::Clamp(TargetPitch, -MaxLeanPitch, MaxLeanPitch);

  // Debug: Show brake state
  if (bShowDebug && GEngine) {
    GEngine->AddOnScreenDebugMessage(
        -1, 0.0f, bIsBraking ? FColor::Red : FColor::Green,
        FString::Printf(TEXT("Brake: %s (Dot: %.2f)"),
                        bIsBraking ? TEXT("YES") : TEXT("NO"), AccelBrakeDot));
  }

  // Roll: Banking (Turn) + Strafe (Lateral Move)
  // Banking - Centripetal force logic: Turn Right -> Lean Right (into turn)
  const float Speed = CurrentVelocity.Size();
  // Normalize speed ref (e.g., at 600 units/s (Running), we have full banking)
  const float BankingAmount = YawRate * (Speed / 600.0f) * BankingScale;

  // Strafing - Move Right -> Lean Right
  const float LocalVelocityY = OwnerCharacter->GetActorTransform()
                                   .InverseTransformVector(CurrentVelocity)
                                   .Y;
  const float StrafeAmount = LocalVelocityY * LeanMultiplier;

  TargetRoll = BankingAmount + StrafeAmount;

  // ---------------------------------------------------------
  // 3. Spring Damper Physics Simulation
  // ---------------------------------------------------------

  // Mass-Spring-Damper equation: a = (Target - Current) * k - Velocity * c
  // Damping Coefficient c = 2 * sqrt(k) * DampingRatio

  const float CriticalDamping = 2.0f * FMath::Sqrt(LeanSpringStiffness);
  const float DampingValue = CriticalDamping * LeanSpringDamping;

  // Simulate Pitch
  {
    const float Displacement = TargetPitch - CurrentBodyInertia.LeanPitch;
    const float Force = (Displacement * LeanSpringStiffness) -
                        (CurrentPitchVelocity * DampingValue);
    CurrentPitchVelocity += Force * DeltaTime;
    CurrentBodyInertia.LeanPitch += CurrentPitchVelocity * DeltaTime;
  }

  // Simulate Roll
  {
    const float Displacement = TargetRoll - CurrentBodyInertia.LeanRoll;
    const float Force = (Displacement * LeanSpringStiffness) -
                        (CurrentRollVelocity * DampingValue);
    CurrentRollVelocity += Force * DeltaTime;
    CurrentBodyInertia.LeanRoll += CurrentRollVelocity * DeltaTime;
  }

  // Clamp to prevent extreme values
  CurrentBodyInertia.LeanPitch =
      FMath::Clamp(CurrentBodyInertia.LeanPitch, -45.f, 45.f);
  CurrentBodyInertia.LeanRoll =
      FMath::Clamp(CurrentBodyInertia.LeanRoll, -45.f, 45.f);

  // Store previous state
  PreviousVelocity = CurrentVelocity;
  PreviousYaw = CurrentYaw;
}

void UInertialMovementComponent::UpdateProceduralTurn(float DeltaTime) {
  // DeltaTime protection (same as UpdateInertiaPhysics)
  if (!OwnerCharacter || DeltaTime <= 0.001f || DeltaTime > 0.5f) {
    return;
  }

  const float Speed = OwnerCharacter->GetVelocity().Size2D();

  // ============================================================
  // CASE 1: MOVING - Let MovementComponent handle capsule rotation
  // We just reset TorsoTwist to 0 (body aligns with legs while running)
  // ============================================================
  if (Speed > IdleSpeedThreshold) {
    CurrentBodyInertia.TorsoTwistYaw =
        FMath::FInterpTo(CurrentBodyInertia.TorsoTwistYaw, 0.f, DeltaTime,
                         TorsoResetInterpSpeed);
    // MeshYaw follows capsule when moving
    MeshYaw = OwnerCharacter->GetActorRotation().Yaw;
    return; // EXIT - Do NOT manipulate capsule!
  }

  // ============================================================
  // CASE 2: IDLE - Enable Mouse-to-Torso control
  // ============================================================
  const FRotator CamRot = OwnerCharacter->GetControlRotation();
  const FRotator ActorRot = OwnerCharacter->GetActorRotation();

  // Use FindDeltaAngleDegrees for proper -180/+180 wrap handling
  const float DeltaYaw = FMath::FindDeltaAngleDegrees(ActorRot.Yaw, CamRot.Yaw);

  // ============================================================
  // SPINE DISTRIBUTION (DEADZONE) logic:
  // - If Angle < HeadMaxYaw: Spine stays at 0 (Head handles it)
  // - If Angle > HeadMaxYaw: Spine takes the excess
  // ============================================================

  // Determine Stance for Limits
  ACharacterRope *RopeChar = Cast<ACharacterRope>(OwnerCharacter);
  bool bIsQuad = RopeChar && RopeChar->GetStance() == EMonkeyStance::Quadruped;
  FLookAtLimits CurrentLimits = GetCurrentLimits(bIsQuad);

  float TargetTwist = 0.f;
  if (FMath::Abs(DeltaYaw) > CurrentLimits.MaxYaw) {
    // We exceeded head limit, twist the spine by the excess amount
    TargetTwist = DeltaYaw - (FMath::Sign(DeltaYaw) * CurrentLimits.MaxYaw);
  }

  // Clamp torso twist to prevent neck-breaking (physical limit)
  TargetTwist = FMath::Clamp(TargetTwist, -MaxTorsoAngle, MaxTorsoAngle);

  CurrentBodyInertia.TorsoTwistYaw =
      FMath::FInterpTo(CurrentBodyInertia.TorsoTwistYaw, TargetTwist, DeltaTime,
                       TorsoTwistInterpSpeed);

  // ============================================================
  // CASE 2B: Turn In Place with INERTIA (Proposition 1)
  // Use angular velocity with acceleration/damping instead of instant
  // rotation
  // ============================================================

  const float TurnDirection = FMath::Sign(DeltaYaw);
  const float AbsDeltaYaw = FMath::Abs(DeltaYaw);

  if (AbsDeltaYaw > CapsuleTurnThreshold) {
    // Calculate target velocity based on excess angle
    const float TargetVelocity = TurnDirection * MaxTurnInPlaceAngularVelocity;

    // Accelerate toward target velocity
    const float VelocityDelta = TargetVelocity - CurrentTurnVelocity;
    const float AccelThisFrame = TurnInPlaceAcceleration * DeltaTime;
    CurrentTurnVelocity +=
        FMath::Sign(VelocityDelta) *
        FMath::Min(FMath::Abs(VelocityDelta), AccelThisFrame);

    // Fire delegate when starting a turn
    if (!bWasTurning && FMath::Abs(CurrentTurnVelocity) > 10.f) {
      OnTurnInPlaceStarted.Broadcast(TurnDirection);
      bWasTurning = true;
    }
  } else {
    // Damp velocity when within threshold (settling)
    CurrentTurnVelocity = FMath::FInterpTo(CurrentTurnVelocity, 0.f, DeltaTime,
                                           TurnInPlaceDamping);

    // Reset turning state when settled
    if (FMath::Abs(CurrentTurnVelocity) < 5.f) {
      bWasTurning = false;
    }
  }

  // Apply rotation from velocity
  if (FMath::Abs(CurrentTurnVelocity) > 0.1f) {
    FRotator NewRot = ActorRot;
    NewRot.Yaw =
        FMath::UnwindDegrees(ActorRot.Yaw + CurrentTurnVelocity * DeltaTime);
    OwnerCharacter->SetActorRotation(NewRot);
  }

  // ============================================================
  // ANTICIPATION TWIST (Proposition 2)
  // Add extra torso twist during turn acceleration for weight shift feel
  // ============================================================
  const float NormalizedVelocity =
      CurrentTurnVelocity / MaxTurnInPlaceAngularVelocity;
  const float AnticipationTwist =
      NormalizedVelocity * TurnAnticipationStrength * MaxTorsoAngle;
  CurrentBodyInertia.TorsoTwistYaw =
      FMath::Clamp(CurrentBodyInertia.TorsoTwistYaw + AnticipationTwist,
                   -MaxTorsoAngle, MaxTorsoAngle);

  // TurnVelocity removed from public struct (Animation driven by Event)
  // CurrentInertiaState.TurnVelocity = CurrentTurnVelocity; (DEPRECATED)
}

void UInertialMovementComponent::UpdateHeadLookAt(float DeltaTime) {
  if (!OwnerCharacter || DeltaTime <= 0.001f || DeltaTime > 0.5f) {
    return;
  }

  const float Speed = OwnerCharacter->GetVelocity().Size2D();

  // ============================================================
  // HEAD LOOK AT - Calculate target point for AnimBP Look At node
  // For TPS camera: trace from camera position to find what player is looking
  // at
  // ============================================================

  // Get actual camera location and direction
  APlayerController *PC =
      Cast<APlayerController>(OwnerCharacter->GetController());
  if (PC) {
    FVector CameraLocation;
    FRotator CameraRotation;
    PC->GetPlayerViewPoint(CameraLocation, CameraRotation);

    const FVector CamForward = CameraRotation.Vector();

    // Blend factor based on movement state
    const float BlendFactor =
        (Speed > IdleSpeedThreshold) ? HeadLookBlendWhenMoving : 1.0f;

    // Calculate look-at point: trace from camera into the world
    // This gives us the actual point the player is looking at
    FVector TraceEnd = CameraLocation + CamForward * HeadLookAtDistance * 10.f;

    FHitResult HitResult;
    FCollisionQueryParams QueryParams;
    QueryParams.AddIgnoredActor(OwnerCharacter);

    FVector LookAtPoint;
    if (GetWorld()->LineTraceSingleByChannel(
            HitResult, CameraLocation, TraceEnd, ECC_Visibility, QueryParams)) {
      // Hit something - look at that point
      LookAtPoint = HitResult.ImpactPoint;
    } else {
      // Nothing hit - look at far point along camera direction
      LookAtPoint = CameraLocation + CamForward * HeadLookAtDistance;
    }

    // Blend between looking forward (actor forward) and looking at camera
    // target
    const FVector ActorForward = OwnerCharacter->GetActorForwardVector();
    const FVector HeadLocation = OwnerCharacter->GetActorLocation() +
                                 FVector(0, 0, HeadLookAtHeightOffset);
    const FVector ToLookAt = (LookAtPoint - HeadLocation).GetSafeNormal();

    // SINGULARITY FIX:
    // If looking backwards (towards camera while running towards camera),
    // the Lerp between ActorForward and ToLookAt (opposites) creates a vertical
    // singularity (0,0,0). We detect this "Backwards Look" and disable HeadLook
    // in that case.
    float DotProd = FVector::DotProduct(ActorForward, ToLookAt);
    const float BackwardThreshold =
        -0.2f; // If dot < -0.2 (looking > 90-100 deg away), stop looking.

    FVector BlendedDirection;
    if (DotProd < BackwardThreshold) {
      // Looking behind: Force Head to look forward (ignore camera)
      BlendedDirection = ActorForward;
    } else {
      // Normal Behavior: Blend
      BlendedDirection =
          FMath::Lerp(ActorForward, ToLookAt, BlendFactor).GetSafeNormal();
    }

    // HeadLookAtTarget removed (Optimization) - Rotation is sufficient
    // CurrentInertiaState.HeadLookAtTarget = HeadLocation + BlendedDirection
    // * HeadLookAtDistance;

    // Calculate World Space rotation for Transform Bone (Replace Existing
    // approach) This is the "Manual Look At" solution for proper parent-space
    // independence
    // Calculate World Space rotation for Transform Bone (Replace Existing
    // approach) This is the "Manual Look At" solution for proper parent-space
    // independence
    // Use Blended Direction (incorporates speed blend) for the LookAt Rotation
    FVector TargetLookAtPoint =
        HeadLocation + BlendedDirection * HeadLookAtDistance;

    FRotator RawLookAt =
        UKismetMathLibrary::FindLookAtRotation(HeadLocation, TargetLookAtPoint);

    // DEBUG: Verify LookAt Vector
    if (bShowDebug) {
      DrawDebugLine(GetWorld(), HeadLocation, TargetLookAtPoint,
                    FColor::Magenta, false, -1.f, 0, 2.f);
    }

    // CLAMPING (Dynamic based on Stance)
    // We clamp the LOGICAL Rotation (RawLookAt) relative to Actor Body

    ACharacterRope *RopeChar = Cast<ACharacterRope>(OwnerCharacter);
    bool bIsQuad =
        RopeChar && RopeChar->GetStance() == EMonkeyStance::Quadruped;
    FLookAtLimits CurrentLimits = GetCurrentLimits(bIsQuad);

    // 1. Clamp Pitch (Logical)
    float NormalizedPitch = FMath::FindDeltaAngleDegrees(0.f, RawLookAt.Pitch);
    float ClampedPitch = FMath::Clamp(NormalizedPitch, -CurrentLimits.MaxPitch,
                                      CurrentLimits.MaxPitch);

    // 2. Clamp Yaw (Logical) - Relative to Body/Capsule
    FRotator ActorRot = OwnerCharacter->GetActorRotation();
    float RelativeYaw =
        FMath::FindDeltaAngleDegrees(ActorRot.Yaw, RawLookAt.Yaw);
    float ClampedRelYaw =
        FMath::Clamp(RelativeYaw, -CurrentLimits.MaxYaw, CurrentLimits.MaxYaw);

    // 3. Reconstruct Clamped Logical Rotation
    FRotator ClampedLogicalRot = RawLookAt;
    ClampedLogicalRot.Pitch = ClampedPitch;
    ClampedLogicalRot.Yaw = ActorRot.Yaw + ClampedRelYaw;

    // 4. APPLY OFFSET (Last step, purely for Bone alignment)
    // NewRot = ClampedLogical * Offset
    FRotator TargetRotation =
        (ClampedLogicalRot.Quaternion() * HeadRotationOffset.Quaternion())
            .Rotator();

    // SMOOTHING: Use RInterpTo to fix 360/0 snapping and filter jitter
    CurrentHeadLook.HeadLookAtRotation =
        FMath::RInterpTo(CurrentHeadLook.HeadLookAtRotation, TargetRotation,
                         DeltaTime, HeadLookRotationInterpSpeed);
  }
}
