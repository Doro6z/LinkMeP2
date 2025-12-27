// MonkeyAnimInstance.h

#pragma once

#include "Animation/AnimInstance.h"
#include "CoreMinimal.h"

#include "Components/InertialMovementComponent.h" // For FInertiaState
#include "MonkeyTypes.h"


#include "MonkeyAnimInstance.generated.h"

// Forward declarations
class ACharacterRope;
enum class EMonkeyStance : uint8;

/**
 * Custom AnimInstance for Monkey character.
 * Caches locomotion state from ACharacterRope via delegate (push) and polling
 * (pull).
 */
UCLASS()
class LINKMEPROJECT_API UMonkeyAnimInstance : public UAnimInstance {
  GENERATED_BODY()

public:
  // ===================================================================
  // LOCOMOTION STATE (Exposed to AnimGraph)
  // ===================================================================

  /** Current Stance - Updated via Delegate (Push) */
  UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Locomotion")
  EMonkeyStance Stance;

  /** Gait Alpha (0-1) - 0=Walk, 1=Sprint. Used for Blend Poses. */
  UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Locomotion")
  float GaitAlpha = 0.0f;

  /** Current Stride Length - Updated each frame (Pull) */
  UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Locomotion")
  float CurrentStrideLength = 100.0f;

  /** Character Speed (horizontal) */
  UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Locomotion")
  float Speed = 0.0f;

  /** Is the character in the air? */
  UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Locomotion")
  bool bIsFalling = false;

  // ===================================================================
  // PROCEDURAL ANIMATION DATA (Copied from Character each frame)
  // ===================================================================

  /** All procedural animation data (IK, Lean, Swing, Landing) */
  UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Procedural")
  FProceduralAnimData ProceduralData;

  /** Inertia-based lean state (Roll, Pitch, TorsoTwist) from
   * UInertialMovementComponent */
  UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Procedural")
  FInertiaState InertiaState;

  // ===================================================================
  // STRIDE WHEEL (Exposed to AnimGraph)
  // ===================================================================

  /** Gait Index (0=Walk, 1=Run) - For Blend Poses by Int. */
  UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Locomotion")
  int32 GaitIndex = 0;

  /** Stride Phase (0-1) - One full stride cycle. */
  UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Stride Wheel")
  float StridePhase = 0.0f;

  /** Explicit Time for Sequence Evaluator (StridePhase * AnimDuration). */
  UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Stride Wheel")
  float ExplicitTime = 0.0f;

  /** Animation cycle duration in seconds (default 4s = 120 frames @ 30fps) */
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stride Wheel")
  float AnimCycleDuration = 4.0f;

  /** Get the cached Character Rope reference (Avoids Cast on Tick) */
  UFUNCTION(BlueprintPure, Category = "References")
  ACharacterRope *GetCharacterRope() const;

protected:
  // ===================================================================
  // LIFECYCLE
  // ===================================================================

  virtual void NativeInitializeAnimation() override;
  virtual void NativeUninitializeAnimation() override;
  virtual void NativeUpdateAnimation(float DeltaSeconds) override;

private:
  // ===================================================================
  // DELEGATE HANDLERS
  // ===================================================================

  /** Called when Character's Stance changes */
  UFUNCTION()
  void OnStanceUpdated(EMonkeyStance OldStance, EMonkeyStance NewStance);

  /** Cached reference to owning Character */
  UPROPERTY()
  TWeakObjectPtr<ACharacterRope> CachedCharacter;

  // ===================================================================
  // STRIDE TRACKING (Internal)
  // ===================================================================

  /** Total distance traveled (used for StridePhase calculation) */
  float TotalDistance = 0.0f;

  /** Previous StrideLength for phase preservation */
  float PreviousStrideLength = 100.0f;
};
