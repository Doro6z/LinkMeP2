// MonkeyAnimInstance.cpp

#include "Animation/MonkeyAnimInstance.h"
#include "CharacterRope.h"
#include "GameFramework/CharacterMovementComponent.h"

void UMonkeyAnimInstance::NativeInitializeAnimation() {
  Super::NativeInitializeAnimation();

  // Cache the owning character
  if (APawn *Pawn = TryGetPawnOwner()) {
    CachedCharacter = Cast<ACharacterRope>(Pawn);

    if (CachedCharacter.IsValid()) {
      // Bind to Stance Changed delegate (Push pattern)
      CachedCharacter->OnStanceChanged.AddDynamic(
          this, &UMonkeyAnimInstance::OnStanceUpdated);

      // Initialize with current values
      Stance = CachedCharacter->GetStance();
      CurrentStrideLength = CachedCharacter->GetCurrentStrideLength();
    }
  }
}

void UMonkeyAnimInstance::NativeUninitializeAnimation() {
  // Unbind from delegate to prevent dangling references
  if (CachedCharacter.IsValid()) {
    CachedCharacter->OnStanceChanged.RemoveDynamic(
        this, &UMonkeyAnimInstance::OnStanceUpdated);
  }

  Super::NativeUninitializeAnimation();
}

void UMonkeyAnimInstance::NativeUpdateAnimation(float DeltaSeconds) {
  Super::NativeUpdateAnimation(DeltaSeconds);

  if (!CachedCharacter.IsValid()) {
    return;
  }

  // Pull frequently-changing values each frame
  CurrentStrideLength = CachedCharacter->GetCurrentStrideLength();

  // Speed (horizontal velocity)
  Speed = CachedCharacter->GetVelocity().Size2D();

  // Calculate GaitAlpha (0-1) based on Stance
  // 0 = Walk, 1 = Sprint
  // Use 90% of MaxSpeed as divisor to ensure GaitAlpha reaches 1.0 before
  // actual max
  float MaxSpeed = 0.0f;
  if (Stance == EMonkeyStance::Biped) {
    MaxSpeed = CachedCharacter->BipedSpeeds.Z; // Sprint speed
  } else {
    MaxSpeed =
        CachedCharacter->QuadrupedSpeeds.Z; // Sprint speed (Quad default)
  }

  // Apply 10% buffer: divide by 90% of MaxSpeed
  const float EffectiveMaxSpeed = MaxSpeed * 0.9f;
  if (EffectiveMaxSpeed > 0.0f) {
    GaitAlpha = FMath::Clamp(Speed / EffectiveMaxSpeed, 0.0f, 1.0f);
  } else {
    GaitAlpha = 0.0f;
  }

  // GaitIndex for Blend Poses by Int: 0 = Walk, 1 = Run
  GaitIndex = (GaitAlpha >= 1.0f) ? 1 : 0;

  // Falling state
  if (UCharacterMovementComponent *CMC =
          CachedCharacter->GetCharacterMovement()) {
    bIsFalling = CMC->IsFalling();
  }

  // ===================================================================
  // STRIDE PHASE CALCULATION (with phase preservation)
  // ===================================================================

  // Accumulate distance traveled
  const float DistanceDelta = Speed * DeltaSeconds;
  TotalDistance += DistanceDelta;

  // Phase preservation: if StrideLength changed, adjust TotalDistance
  // to maintain the same phase with the new StrideLength
  if (!FMath::IsNearlyEqual(CurrentStrideLength, PreviousStrideLength, 0.1f)) {
    // Calculate current phase with old stride
    const float OldPhase =
        FMath::Fmod(TotalDistance, PreviousStrideLength) / PreviousStrideLength;
    // Adjust TotalDistance so phase stays the same with new stride
    TotalDistance = OldPhase * CurrentStrideLength;
    PreviousStrideLength = CurrentStrideLength;
  }

  // Calculate StridePhase (0-1)
  if (CurrentStrideLength > 0.0f) {
    StridePhase =
        FMath::Fmod(TotalDistance, CurrentStrideLength) / CurrentStrideLength;
    StridePhase = FMath::Clamp(StridePhase, 0.0f, 1.0f);
  } else {
    StridePhase = 0.0f;
  }

  // Calculate ExplicitTime for Sequence Evaluator
  ExplicitTime = StridePhase * AnimCycleDuration;

  // Copy Procedural Animation Data (single struct copy)
  ProceduralData = CachedCharacter->ProceduralData;

  // Copy Inertia State from InertialMovementComponent (Phase 2)
  if (UInertialMovementComponent *InertialComp =
          CachedCharacter->InertialMovementComp) {
    InertiaState = InertialComp->GetInertiaState();
  }
}

void UMonkeyAnimInstance::OnStanceUpdated(EMonkeyStance OldStance,
                                          EMonkeyStance NewStance) {
  // Push update: Stance changed on Character, update local cache
  Stance = NewStance;

  UE_LOG(LogTemp, Log, TEXT("[MonkeyAnimInstance] Stance changed: %s -> %s"),
         *UEnum::GetValueAsString(OldStance),
         *UEnum::GetValueAsString(NewStance));
}

ACharacterRope *UMonkeyAnimInstance::GetCharacterRope() const {
  return CachedCharacter.Get();
}
