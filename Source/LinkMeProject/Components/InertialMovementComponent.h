#pragma once

#include "Components/ActorComponent.h"
#include "CoreMinimal.h"
#include "InertialMovementComponent.generated.h"


class UCharacterMovementComponent;

/**
 * Struct holidng the smoothed inertial state for animation.
 */
USTRUCT(BlueprintType)
struct FInertiaState {
  GENERATED_BODY()

  // Lateral lean (Banking)
  UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Inertia")
  float LeanRoll = 0.f;

  // Forward/Backward tilt (Acceleration/Braking)
  UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Inertia")
  float LeanPitch = 0.f;

  // Torso twist for look offset (Not yet implemented logic, placeholder)
  UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Inertia")
  float TorsoTwistYaw = 0.f;
};

/**
 * Component responsible for calculating physics-based procedural additive
 * animation offsets. Simulates weight, inertia, and banking using Spring-Damper
 * dynamics.
 */
UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class LINKMEPROJECT_API UInertialMovementComponent : public UActorComponent {
  GENERATED_BODY()

public:
  UInertialMovementComponent();

  virtual void
  TickComponent(float DeltaTime, ELevelTick TickType,
                FActorComponentTickFunction *ThisTickFunction) override;

  UFUNCTION(BlueprintPure, Category = "Inertia")
  const FInertiaState &GetInertiaState() const { return CurrentInertiaState; }

protected:
  virtual void BeginPlay() override;

  // --- Configuration ---

  // Scale for lateral lean (banking) based on turn rate and velocity
  UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Inertia|Lean")
  float BankingScale = 0.5f;

  // Scale for forward/backward tilt based on acceleration
  UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Inertia|Lean")
  float AccelerationTiltScale = 0.1f;

  // Multiplier for lateral velocity lean (strafing)
  UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Inertia|Lean")
  float LeanMultiplier = 0.05f;

  // Spring Stiffness. Higher = Faster response/Rigid.
  UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Inertia|Springs")
  float LeanSpringStiffness = 50.0f;

  // Spring Damping. 1.0 = Critical Damping (No oscillation), < 1.0 = Bouncy.
  UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Inertia|Springs")
  float LeanSpringDamping = 0.8f;

  // --- State Data ---

  UPROPERTY(BlueprintReadOnly, Category = "Inertia|Output")
  FInertiaState CurrentInertiaState;

private:
  UPROPERTY()
  TObjectPtr<ACharacter> OwnerCharacter;

  UPROPERTY()
  TObjectPtr<UCharacterMovementComponent> MovementComp;

  // Physics State
  FVector PreviousVelocity;
  float PreviousYaw;

  // Spring Velocities (internal for SpringDamper)
  float CurrentRollVelocity = 0.f;
  float CurrentPitchVelocity = 0.f;

  void UpdateInertiaPhysics(float DeltaTime);
};
