// CharacterRope.h

#pragma once

#include "Components/InertialMovementComponent.h"
#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "HookChargeComponent.h"
#include "MonkeyTypes.h"
#include "RopeCameraManager.h"
#include "RopeSystemComponent.h"

#include "CharacterRope.generated.h"

// ============================================================================
// LOCOMOTION ENUMS
// ============================================================================

UENUM(BlueprintType)
enum class EMonkeyStance : uint8 {
  Quadruped UMETA(DisplayName = "Quadruped"), // Default, faster
  Biped UMETA(DisplayName = "Biped")          // Combat/Interaction, slower
};

UENUM(BlueprintType)
enum class EMonkeyGait : uint8 {
  Walk UMETA(DisplayName = "Walk"),
  Jog UMETA(DisplayName = "Jog"),
  Sprint UMETA(DisplayName = "Sprint")
};

// Delegate for Stance Change events
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnStanceChanged, EMonkeyStance,
                                             OldStance, EMonkeyStance,
                                             NewStance);

UCLASS()
class LINKMEPROJECT_API ACharacterRope : public ACharacter {
  GENERATED_BODY()

public:
  ACharacterRope();

protected:
  /** Camera Manager for states and effects */
  UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
  URopeCameraManager *CameraManager;

  /** Aiming Component for target detection and state */
  UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Aiming")
  class UTPSAimingComponent *AimingComponent;

  // ===================================================================
  // MAGNETISM CONFIGURATION (for TPSAimingComponent)
  // ===================================================================

  /** Enable target magnetism (snap to hookable targets) */
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TPS Magnetism")
  bool bEnableMagnetism = true;

  /** Maximum range to detect hookable targets */
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TPS Magnetism")
  float MagnetismRange = 3000.0f;

  /** Cone angle for target detection (degrees) */
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TPS Magnetism")
  float MagnetismConeAngle = 15.0f;

  /** Strength of magnetism snap (0-1) */
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TPS Magnetism")
  float MagnetismStrength = 0.5f;

  /** Animation State: True when aiming/preparing to fire hook. Used by AnimBP.
   */
  UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Animation")
  bool bIsPreparingHook;

  UFUNCTION(BlueprintCallable, Category = "Aiming")
  void StartAiming();

  UFUNCTION(BlueprintCallable, Category = "Aiming")
  void StopAiming();

  /** Start Focus mode (precise aiming with camera effects). Automatically calls
   * StartAiming. */
  UFUNCTION(BlueprintCallable, Category = "Aiming")
  void StartFocus();

  /** Stop Focus mode (camera effects off, but can still be aiming) */
  UFUNCTION(BlueprintCallable, Category = "Aiming")
  void StopFocus();

  /** Get the direction to fire the hook (uses aiming component) */
  UFUNCTION(BlueprintPure, Category = "Aiming")
  FVector GetFireDirection() const;

  // ===================================================================
  // HOOK CHARGE SYSTEM
  // ===================================================================

  UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hook Charge")
  UHookChargeComponent *HookChargeComponent;

  UFUNCTION(BlueprintCallable, Category = "Hook Charge")
  void StartChargingHook();

  UFUNCTION(BlueprintCallable, Category = "Hook Charge")
  void CancelHookCharge();

  /** Release the charged hook */
  UFUNCTION(BlueprintCallable, Category = "Hook Charge")
  void FireChargedHook();

  UPROPERTY(EditAnywhere, BlueprintReadWrite,
            Category = "Hook Charge|Visualization")
  bool bShowTrajectoryWhileCharging = true;

  UPROPERTY(EditAnywhere, BlueprintReadWrite,
            Category = "Hook Charge|Visualization")
  float TrajectoryUpdateFrequency = 0.05f;

  UPROPERTY(EditAnywhere, BlueprintReadWrite,
            Category = "Hook Charge|Visualization")
  FLinearColor TrajectoryColorNormal = FLinearColor::Yellow;

  UPROPERTY(EditAnywhere, BlueprintReadWrite,
            Category = "Hook Charge|Visualization")
  FLinearColor TrajectoryColorPerfect = FLinearColor::Green;

  UPROPERTY(EditAnywhere, BlueprintReadWrite,
            Category = "Hook Charge|Visualization")
  FLinearColor TrajectoryColorUnreachable = FLinearColor::Red;

  // Reticle for Focus Mode
  UPROPERTY(EditAnywhere, BlueprintReadWrite,
            Category = "Hook Charge|Visualization")
  TSubclassOf<AActor> FocusReticleClass;

protected:
  void UpdateTrajectoryVisualization(float DeltaTime);
  void UpdateFocusReticle();
  FVector GetProjectileStartLocation() const;

  UPROPERTY()
  AActor *FocusReticleInstance = nullptr;

  float TimeSinceLastTrajectoryUpdate = 0.0f;

protected:
  virtual void BeginPlay() override;
  virtual void Landed(const FHitResult &Hit) override;

  // ===================================================================
  // LOCOMOTION & STANCE SYSTEM
  // ===================================================================

public:
  UPROPERTY(BlueprintAssignable, Category = "Locomotion")
  FOnStanceChanged OnStanceChanged;

protected:
  /** Current Stance (Replicated) */
  UPROPERTY(VisibleAnywhere, BlueprintReadOnly,
            ReplicatedUsing = OnRep_CurrentStance, Category = "Locomotion")
  EMonkeyStance CurrentStance;

  /** Current Gait (Calculated locally) */
  UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Locomotion")
  EMonkeyGait CurrentGait;

  /** Flag for Sprint Input (Replicated logic via RPC) */
  UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Locomotion")
  bool bIsSprinting;

  /** Flag for Walk Input (Replicated logic via RPC) */
  UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Locomotion")
  bool bIsWalking;

public:
  // --- CONFIGURATION ---

  /* Max Walk Speed for Quadruped Stance (Walk, Jog, Sprint) */
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Locomotion|Quadruped")
  FVector QuadrupedSpeeds = FVector(300.f, 600.f, 900.f);

  /* Max Walk Speed for Biped Stance (Walk, Jog, Sprint) */
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Locomotion|Biped")
  FVector BipedSpeeds = FVector(200.f, 450.f, 700.f);

protected:
  // --- CAPSULE CONFIGURATION ---

  /** Radius and HalfHeight for Quadruped Stance */
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Locomotion|Quadruped")
  FVector2D QuadrupedCapsuleSize =
      FVector2D(40.f, 60.f); // X=Radius, Y=HalfHeight

  /** Radius and HalfHeight for Biped Stance */
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Locomotion|Biped")
  FVector2D BipedCapsuleSize = FVector2D(35.f, 90.f); // X=Radius, Y=HalfHeight

  // --- STRIDE CONFIGURATION ---

  /** Curve defining Stride Length (Y) based on Speed (X).
   *  Example: 0->80, 600->140 */
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Locomotion|Stride")
  class UCurveFloat *StrideCurveQuadruped;

  /** Curve defining Stride Length (Y) based on Speed (X) for Biped Stance. */
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Locomotion|Stride")
  class UCurveFloat *StrideCurveBiped;

  /** Current logic-calculated Stride Length. driven by Curve. */
  UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Locomotion|Stride")
  float CurrentStrideLength = 100.0f;

public:
  UFUNCTION(BlueprintPure, Category = "Locomotion")
  float GetCurrentStrideLength() const { return CurrentStrideLength; }

  // ===================================================================
  // PROCEDURAL ANIMATION DATA
  // ===================================================================

  /** All procedural animation data (IK, Lean, Swing, Landing) */
  UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Procedural")
  FProceduralAnimData ProceduralData;

  /** Trace distance for IK ground detection */
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Procedural|IK")
  float IK_TraceDistance = 55.0f;

  /** Foot offset from ground (to avoid clipping) */
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Procedural|IK")
  float IK_FootOffset = 5.0f;

  /** Enable IK (for debugging) */
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Procedural|IK")
  bool bEnableIK = true;

  /** Component handling physics-based inertia (Lean, Tilt, Turn) */
  UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Procedural")
  class UInertialMovementComponent *InertialMovementComp;

protected:
  /** Update all procedural animation data (IK, Lean, Swing, Landing) */
  void UpdateProceduralAnimation(float DeltaTime);

public:
  // --- ACTIONS ---

  /** Toggle between Quad and Biped Stance */
  UFUNCTION(BlueprintCallable, Category = "Locomotion")
  void ToggleStance();

  /** Set Stance Explicitly */
  UFUNCTION(BlueprintCallable, Category = "Locomotion")
  void SetStance(EMonkeyStance NewStance);

  UFUNCTION(BlueprintCallable, Category = "Locomotion")
  void StartSprint();

  UFUNCTION(BlueprintCallable, Category = "Locomotion")
  void StopSprint();

  UFUNCTION(BlueprintCallable, Category = "Locomotion")
  void StartWalking();

  UFUNCTION(BlueprintCallable, Category = "Locomotion")
  void StopWalking();

  /** Get Current Stance */
  UFUNCTION(BlueprintPure, Category = "Locomotion")
  EMonkeyStance GetStance() const { return CurrentStance; }

  /** Get Current Gait */
  UFUNCTION(BlueprintPure, Category = "Locomotion")
  EMonkeyGait GetGait() const { return CurrentGait; }

protected:
  // --- REPLICATION & INTERNAL ---

  UFUNCTION()
  void OnRep_CurrentStance(EMonkeyStance OldStance);

  UFUNCTION(Server, Reliable)
  void ServerSetStance(EMonkeyStance NewStance);

  UFUNCTION(Server, Reliable)
  void ServerSetSprint(bool bNewSprinting);

  UFUNCTION(Server, Reliable)
  void ServerSetWalk(bool bNewWalking);

  /** Internal implementation (shared Client/Server) */
  void LocalSetStance(EMonkeyStance NewStance);

  /** Calculate target speed based on Stance/Input and apply to CMC */
  void UpdateLocomotionSpeed(float DeltaTime);

  // --- CAPSULE MANAGEMENT ---

  /** Try to change stance, checking for collision if standing up */
  UFUNCTION(BlueprintCallable, Category = "Locomotion")
  bool TrySetStance(EMonkeyStance NewStance);

  /** Check if there is room to stand up */
  bool CanStandUp() const;

  /** Updates rotation settings (OrientToMovement vs ControllerYaw) based on
   * Stance */
  void UpdateRotationSettings(EMonkeyStance NewStance);

  /** Update Capsule Size based on Stance */
  void UpdateCapsuleSize(EMonkeyStance NewStance);

  // Overrides for Replication
  virtual void GetLifetimeReplicatedProps(
      TArray<FLifetimeProperty> &OutLifetimeProps) const override;

public:
  virtual void Tick(float DeltaTime) override;

  virtual void SetupPlayerInputComponent(
      class UInputComponent *PlayerInputComponent) override;
};
