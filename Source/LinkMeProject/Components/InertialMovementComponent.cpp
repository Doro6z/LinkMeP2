#include "Components/InertialMovementComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/KismetMathLibrary.h"

UInertialMovementComponent::UInertialMovementComponent() {
  PrimaryComponentTick.bCanEverTick = true;

  // Defaults
  LeanMultiplier = 0.05f;
  AccelerationTiltScale = 0.1f;
  BankingScale = 0.5f;
  LeanSpringStiffness = 50.0f;
  LeanSpringDamping = 0.8f; // Slightly under-damped for organic feel
}

void UInertialMovementComponent::BeginPlay() {
  Super::BeginPlay();

  OwnerCharacter = Cast<ACharacter>(GetOwner());
  if (OwnerCharacter) {
    MovementComp = OwnerCharacter->GetCharacterMovement();
    PreviousVelocity = OwnerCharacter->GetVelocity();
    PreviousYaw = OwnerCharacter->GetActorRotation().Yaw;
  }
}

void UInertialMovementComponent::TickComponent(
    float DeltaTime, ELevelTick TickType,
    FActorComponentTickFunction *ThisTickFunction) {
  Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
  UpdateInertiaPhysics(DeltaTime);
}

void UInertialMovementComponent::UpdateInertiaPhysics(float DeltaTime) {
  if (!OwnerCharacter || DeltaTime <= 0.0f)
    return;

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

  // Pitch: Tilt based on forward acceleration/braking
  // Forward Accel (X > 0) -> Lean Forward (Pitch < 0 in UE)
  TargetPitch = -LocalAcceleration.X * AccelerationTiltScale;

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
    const float Displacement = TargetPitch - CurrentInertiaState.LeanPitch;
    const float Force = (Displacement * LeanSpringStiffness) -
                        (CurrentPitchVelocity * DampingValue);
    CurrentPitchVelocity += Force * DeltaTime;
    CurrentInertiaState.LeanPitch += CurrentPitchVelocity * DeltaTime;
  }

  // Simulate Roll
  {
    const float Displacement = TargetRoll - CurrentInertiaState.LeanRoll;
    const float Force = (Displacement * LeanSpringStiffness) -
                        (CurrentRollVelocity * DampingValue);
    CurrentRollVelocity += Force * DeltaTime;
    CurrentInertiaState.LeanRoll += CurrentRollVelocity * DeltaTime;
  }

  // Store previous state
  PreviousVelocity = CurrentVelocity;
  PreviousYaw = CurrentYaw;
}
