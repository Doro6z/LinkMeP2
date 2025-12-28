#pragma once

#include "Components/ActorComponent.h"
#include "CoreMinimal.h"
#include "InertialMovementComponent.generated.h"

class UCharacterMovementComponent;

/**
 * Limits for Head Look At (Yaw/Pitch) per Stance
 */
USTRUCT(BlueprintType)
struct FLookAtLimits {
  GENERATED_BODY()

  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Limits")
  float MaxYaw = 90.f;

  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Limits")
  float MaxPitch = 60.f;
};

/**
 * Body inertial state (Lean, Twist) - Replicated efficiently (low frequency
 * acceptable)
 */
/**
 * Body inertial state (Lean, Twist) - Replicated efficiently (low frequency
 * acceptable)
 */
USTRUCT(BlueprintType)
struct FBodyInertiaState {
  GENERATED_BODY()

  // Lateral lean (Banking)
  UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Inertia")
  float LeanRoll = 0.f;

  // Forward/Backward tilt (Acceleration/Braking)
  UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Inertia")
  float LeanPitch = 0.f;

  // Torso twist for look offset (Yaw relative to capsule)
  UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Inertia")
  float TorsoTwistYaw = 0.f;

  // Internal Turn Velocity (Not replicated for AnimBP anymore, used for
  // internal logic) To restore AnimBP replication, re-add 'float TurnVelocity'
  // property here.
};

/**
 * Head Look state (Rotation) - High Priority Replication (Intent)
 */
USTRUCT(BlueprintType)
struct FHeadLookState {
  GENERATED_BODY()

  // World Space rotation for head - Use with Transform Bone (Replace Existing,
  // World Space) No Vector Target needed (Rotation is sufficient and lighter)
  UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Inertia")
  FRotator HeadLookAtRotation = FRotator::ZeroRotator;
};

// Delegate for Turn In Place events (for triggering animations)
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnTurnInPlaceStarted, float,
                                            Direction);

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
  const FBodyInertiaState &GetBodyInertia() const { return CurrentBodyInertia; }

  UFUNCTION(BlueprintPure, Category = "Inertia")
  const FHeadLookState &GetHeadLook() const { return CurrentHeadLook; }

protected:
  virtual void BeginPlay() override;
  virtual void GetLifetimeReplicatedProps(
      TArray<FLifetimeProperty> &OutLifetimeProps) const override;

  // --- Debug ---

  /** Toggle on-screen debug display for inertia values */
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inertia|Debug")
  bool bShowDebug = false;

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

  // Maximum pitch lean angle (forward/backward) in degrees
  UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Inertia|Lean")
  float MaxLeanPitch = 15.0f;

  // Lean multiplier when accelerating (starting to move)
  UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Inertia|Lean")
  float AccelerationLeanMultiplier = 1.5f;

  // Lean multiplier when braking (stopping) - Higher for more dramatic stop
  UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Inertia|Lean")
  float BrakingLeanMultiplier = 3.0f;

  // Spring Stiffness. Higher = Faster response/Rigid.
  UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Inertia|Springs")
  float LeanSpringStiffness = 50.0f;

  // Spring Damping. 1.0 = Critical Damping (No oscillation), < 1.0 = Bouncy.
  UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Inertia|Springs")
  float LeanSpringDamping = 0.8f;

  // --- Turn In Place Configuration ---

  // Speed below which Turn In Place is active (character considered Idle)
  UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Inertia|Turn")
  float IdleSpeedThreshold = 10.0f;

  // Maximum torso twist angle relative to camera (degrees)
  UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Inertia|Turn")
  float MaxTorsoAngle = 90.0f;

  // Yaw delta threshold before capsule starts turning (Turn In Place trigger)
  UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Inertia|Turn")
  float CapsuleTurnThreshold = 70.0f;

  // Interpolation speed for torso twist when idle (higher = snappier)
  UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Inertia|Turn")
  float TorsoTwistInterpSpeed = 10.0f;

  // Interpolation speed for capsule turn in place (DEPRECATED - using inertia
  // now)
  UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Inertia|Turn")
  float CapsuleTurnInterpSpeed = 2.5f;

  // Interpolation speed for torso reset when moving (body aligns with legs)
  UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Inertia|Turn")
  float TorsoResetInterpSpeed = 5.0f;

  // --- Turn In Place Inertia (Proposition 1) ---

  // Maximum angular velocity during Turn In Place (deg/sec)
  UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Inertia|Turn")
  float MaxTurnInPlaceAngularVelocity = 150.0f;

  // Angular acceleration for Turn In Place (deg/sec^2)
  UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Inertia|Turn")
  float TurnInPlaceAcceleration = 400.0f;

  // Damping for Turn In Place (higher = stops faster, less overshoot)
  UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Inertia|Turn")
  float TurnInPlaceDamping = 6.0f;

  // --- Turn Anticipation (Proposition 2) ---

  // Extra torso twist during turn acceleration (0-1, percentage of turn
  // velocity)
  UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Inertia|Turn")
  float TurnAnticipationStrength = 0.3f;

  // --- Head Look At Configuration ---

  // Distance in front of character to project LookAt target (cm)
  UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Inertia|HeadLook")
  float HeadLookAtDistance = 500.0f;

  // Blend factor for head look when moving (0 = no look, 1 = full look)
  // Exposed for future FreeLook functionality
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inertia|HeadLook")
  float HeadLookBlendWhenMoving = 0.5f;

  // Height offset from actor location for LookAt origin (cm)
  UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Inertia|HeadLook")
  float HeadLookAtHeightOffset = 150.0f;

  // Rotation offset to align head bone with LookAt direction (e.g. Yaw -90 if
  // bone points Y+)
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inertia|HeadLook")
  FRotator HeadRotationOffset = FRotator::ZeroRotator;

  // Smoothing speed for head rotation (prevent snaps)
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inertia|HeadLook")
  float HeadLookRotationInterpSpeed = 10.0f;

  // Max Yaw angle for head before spine starts twisting (Deadzone for spine)
  // DEPRECATED - Use BipedLimits / QuadLimits instead
  // UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inertia|HeadLook")
  // float HeadMaxYaw = 90.0f;

  UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Inertia|HeadLook")
  FLookAtLimits BipedLimits{110.f, 60.f};

  UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Inertia|HeadLook")
  FLookAtLimits QuadLimits{50.f, 70.f};

  // Helper to get current limits based on Stance
  FLookAtLimits GetCurrentLimits(bool bIsQuadruped) const;

  // --- State Data ---

  UPROPERTY(BlueprintReadOnly, Replicated, Category = "Inertia|Output")
  FBodyInertiaState CurrentBodyInertia;

  UPROPERTY(BlueprintReadOnly, Replicated, Category = "Inertia|Output")
  FHeadLookState CurrentHeadLook;

  // Event fired when Turn In Place begins (Direction: +1 = Right, -1 = Left)
  UPROPERTY(BlueprintAssignable, Category = "Inertia|Events")
  FOnTurnInPlaceStarted OnTurnInPlaceStarted;

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

  // Procedural Turn State
  float MeshYaw =
      0.f; // Tracked mesh orientation (decoupled from capsule when stationary)
  bool bWasStationary = false;
  float CurrentTurnVelocity = 0.f; // Turn In Place angular velocity (deg/sec)
  bool bWasTurning = false;        // For detecting turn start (delegate)

  void UpdateInertiaPhysics(float DeltaTime);
  void UpdateProceduralTurn(float DeltaTime);
  void UpdateHeadLookAt(float DeltaTime);
};
