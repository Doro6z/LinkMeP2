// RopeCameraManager.h
// Centralized camera management with states and effect layers for juice

#pragma once

#include "Camera/CameraComponent.h"
#include "Components/ActorComponent.h"
#include "CoreMinimal.h"
#include "GameFramework/SpringArmComponent.h"
#include "RopeCameraManager.generated.h"

class UCurveFloat;

/**
 * Camera states for different gameplay modes
 */
UENUM(BlueprintType)
enum class ECameraState : uint8 {
  Grounded, // Walking, running, idle
  Swinging, // Swinging on rope
};

/**
 * Effect layer for additive camera modifications
 */
USTRUCT(BlueprintType)
struct FCameraEffectLayer {
  GENERATED_BODY()

  /** Unique identifier for this layer */
  UPROPERTY(EditAnywhere, BlueprintReadWrite)
  FName LayerID;

  /** FOV delta (additive) */
  UPROPERTY(EditAnywhere, BlueprintReadWrite)
  float FOVDelta = 0.f;

  /** Position offset (additive, in local space) */
  UPROPERTY(EditAnywhere, BlueprintReadWrite)
  FVector PositionOffset = FVector::ZeroVector;

  /** Rotation offset (additive) */
  UPROPERTY(EditAnywhere, BlueprintReadWrite)
  FRotator RotationOffset = FRotator::ZeroRotator;

  /** Blend weight (0-1) */
  UPROPERTY(EditAnywhere, BlueprintReadWrite,
            meta = (ClampMin = "0", ClampMax = "1"))
  float BlendWeight = 1.f;

  /** Current blend alpha (internal use) */
  float CurrentBlendAlpha = 0.f;

  /** Blend speed for FInterpTo */
  UPROPERTY(EditAnywhere, BlueprintReadWrite)
  float BlendSpeed = 10.f;

  FCameraEffectLayer() = default;
  FCameraEffectLayer(FName InID) : LayerID(InID) {}
};

/**
 * Centralized camera manager component.
 * Creates and controls SpringArm + Camera.
 * Manages camera states and effect layers for juice effects.
 */
UCLASS(ClassGroup = (Camera), meta = (BlueprintSpawnableComponent))
class LINKMEPROJECT_API URopeCameraManager : public UActorComponent {
  GENERATED_BODY()

public:
  URopeCameraManager();

  virtual void BeginPlay() override;
  virtual void
  TickComponent(float DeltaTime, ELevelTick TickType,
                FActorComponentTickFunction *ThisTickFunction) override;

  // ===================================================================
  // CAMERA COMPONENTS (Created by this component)
  // ===================================================================

  UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
  USpringArmComponent *SpringArm;

  UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
  UCameraComponent *Camera;

  // ===================================================================
  // STATE MANAGEMENT
  // ===================================================================

  /** Get current camera state */
  UFUNCTION(BlueprintPure, Category = "Camera|State")
  ECameraState GetCurrentState() const { return CurrentState; }

  /** Set camera state */
  UFUNCTION(BlueprintCallable, Category = "Camera|State")
  void SetState(ECameraState NewState);

  /** Check if currently aiming (modifier overlay) */
  UFUNCTION(BlueprintPure, Category = "Camera|State")
  bool IsAiming() const { return bIsAiming; }

  /** Set aiming state (modifier overlay on any state) */
  UFUNCTION(BlueprintCallable, Category = "Camera|State")
  void SetAiming(bool bAiming);

  /** Toggle shoulder side */
  UFUNCTION(BlueprintCallable, Category = "Camera|State")
  void ToggleShoulderSwap();

  // ===================================================================
  // EFFECT LAYERS
  // ===================================================================

  /** Add or update an effect layer */
  UFUNCTION(BlueprintCallable, Category = "Camera|Effects")
  void AddEffect(const FCameraEffectLayer &Effect);

  /** Remove an effect layer by ID */
  UFUNCTION(BlueprintCallable, Category = "Camera|Effects")
  void RemoveEffect(FName LayerID);

  /** Check if an effect layer exists */
  UFUNCTION(BlueprintPure, Category = "Camera|Effects")
  bool HasEffect(FName LayerID) const;

  // ===================================================================
  // JUICE EFFECTS API
  // ===================================================================

  /** Apply a transient effect that auto-removes after duration */
  UFUNCTION(BlueprintCallable, Category = "Camera|Juice")
  void ApplyTransientEffect(FName LayerID, float FOVDelta,
                            FVector PositionOffset, float Duration);

  // ===================================================================
  // CONFIGURATION
  // ===================================================================

  /** Base field of view */
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Config")
  float BaseFOV = 90.f;

  /** Base spring arm length */
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Config")
  float BaseArmLength = 400.f;

  /** Base socket offset (shoulder position) */
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Config")
  FVector BaseSocketOffset = FVector(0.f, 80.f, 60.f);

  /** Socket offset when aiming */
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Config")
  FVector AimingSocketOffset = FVector(50.f, 60.f, -20.f);

  /** FOV when aiming */
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Config")
  float AimingFOV = 70.f;

  // ===================================================================
  // LAG CONFIGURATION (State-Based Curves)
  // ===================================================================

  /** Curve: Speed → Lag for Grounded state (X=Speed cm/s, Y=Lag) */
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Lag|Curves")
  UCurveFloat *GroundedLagCurve = nullptr;

  /** Curve: Speed → Lag for Swinging state (X=Speed cm/s, Y=Lag) */
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Lag|Curves")
  UCurveFloat *SwingingLagCurve = nullptr;

  /** Fallback lag if curve is null (Grounded) */
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Lag|Curves")
  float DefaultGroundedLag = 8.f;

  /** Fallback lag if curve is null (Swinging) */
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Lag|Curves")
  float DefaultSwingingLag = 12.f;

  /** Blend speed from Grounded → Swinging (fast) */
  UPROPERTY(EditAnywhere, BlueprintReadWrite,
            Category = "Camera|Lag|Transitions")
  float LagTransitionToSwingSpeed = 10.f;

  /** Blend speed from Swinging → Grounded (smooth) */
  UPROPERTY(EditAnywhere, BlueprintReadWrite,
            Category = "Camera|Lag|Transitions")
  float LagTransitionToGroundSpeed = 3.f;

  // Legacy properties (deprecated, kept for compatibility)
  UPROPERTY()
  float BaseLagSpeed_DEPRECATED = 8.f;

  UPROPERTY()
  float MinSwingLagSpeed_DEPRECATED = 0.f;

  UPROPERTY()
  float LagFadeOutSpeed_DEPRECATED = 10.f;

  UPROPERTY()
  float LagFadeInSpeed_DEPRECATED = 3.f;

  /** Interpolation speed for state transitions */
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Config")
  float TransitionSpeed = 10.f;

  /** Minimum camera pitch (looking down) */
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Config",
            meta = (ClampMin = "-90", ClampMax = "0"))
  float MinPitch = -89.f;

  /** Maximum camera pitch (looking up) */
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Config",
            meta = (ClampMin = "0", ClampMax = "90"))
  float MaxPitch = 89.f;

  // ===================================================================
  // JUICE CONFIGURATION
  // ===================================================================

  /** Minimum speed to start FOV boost */
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Juice")
  float SpeedThresholdForFOV = 800.f;

  /** Speed at which FOV boost reaches maximum */
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Juice")
  float MaxSpeedForFOV = 2000.f;

  /** Maximum FOV boost at max speed */
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Juice")
  float HighSpeedFOVBoost = 15.f;

  /** Optional curve for Speed-to-FOV mapping (X=Speed, Y=FOV Delta) */
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Juice")
  UCurveFloat *SpeedToFOVCurve = nullptr;

  /** Use curve instead of linear mapping for FOV */
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Juice")
  bool bUseFOVCurve = false;

  /** Enable juice effects */
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Juice")
  bool bEnableJuiceEffects = true;

  // ===================================================================
  // DEBUG API (Blueprint HUD)
  // ===================================================================

  /** Enable debug display on screen */
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Debug")
  bool bShowJuiceDebug = false;

  /** Toggle juice debug display */
  UFUNCTION(BlueprintCallable, Category = "Camera|Debug")
  void ToggleJuiceDebug() { bShowJuiceDebug = !bShowJuiceDebug; }

  /** Get current FOV (after all effects) */
  UFUNCTION(BlueprintPure, Category = "Camera|Debug")
  float GetCurrentFOV() const;

  /** Get current socket offset */
  UFUNCTION(BlueprintPure, Category = "Camera|Debug")
  FVector GetCurrentSocketOffset() const;

  /** Get number of active effect layers */
  UFUNCTION(BlueprintPure, Category = "Camera|Debug")
  int32 GetActiveLayerCount() const { return ActiveLayers.Num(); }

  /** Get list of active layer IDs */
  UFUNCTION(BlueprintPure, Category = "Camera|Debug")
  TArray<FName> GetActiveLayerIDs() const;

  /** Get total FOV delta from effects */
  UFUNCTION(BlueprintPure, Category = "Camera|Debug")
  float GetTotalFOVDelta() const;

  /** Get camera state as string */
  UFUNCTION(BlueprintPure, Category = "Camera|Debug")
  FString GetStateAsString() const;

  // ===================================================================
  // BLUEPRINT SETTERS (Runtime Tuning)
  // ===================================================================

  UFUNCTION(BlueprintCallable, Category = "Camera|Config")
  void SetBaseFOV(float NewFOV) { BaseFOV = NewFOV; }

  UFUNCTION(BlueprintCallable, Category = "Camera|Config")
  void SetAimingFOV(float NewFOV) { AimingFOV = NewFOV; }

  UFUNCTION(BlueprintCallable, Category = "Camera|Lag")
  void SetBaseLagSpeed(float NewLag) { DefaultGroundedLag = NewLag; }

  UFUNCTION(BlueprintCallable, Category = "Camera|Lag")
  void SetMinSwingLagSpeed(float NewLag) { DefaultSwingingLag = NewLag; }

  UFUNCTION(BlueprintCallable, Category = "Camera|Juice")
  void SetSpeedThreshold(float Threshold) { SpeedThresholdForFOV = Threshold; }

  UFUNCTION(BlueprintCallable, Category = "Camera|Juice")
  void SetHighSpeedFOVBoost(float Boost) { HighSpeedFOVBoost = Boost; }

  UFUNCTION(BlueprintCallable, Category = "Camera|Juice")
  void SetFOVCurve(UCurveFloat *Curve) { SpeedToFOVCurve = Curve; }

  /** If true, UpdateCamera() (C++) is skipped and OnBlueprintUpdateCamera is
   * called instead */
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Config")
  bool bUseBlueprintCameraLogic = false;

  /** Event called when logic is delegated to Blueprint */
  UFUNCTION(BlueprintImplementableEvent, Category = "Camera|Events")
  void OnBlueprintUpdateCamera(float DeltaTime);

protected:
  /** Update camera based on current state and effects */
  void UpdateCamera(float DeltaTime);

  /** Apply effect layers to camera */
  void ApplyEffectLayers(float DeltaTime);

  /** Clamp pitch input */
  void ClampPitch();

  /** Update speed-based juice effects */
  void UpdateJuiceEffects(float DeltaTime);

  /** Update high-speed FOV effect */
  void UpdateHighSpeedEffect(float DeltaTime);

  /** Detect and trigger swing apex effect */
  void UpdateSwingApexEffect(float DeltaTime);

  /** Internal Debug Draw */
  void DrawDebugInfo();

  // Current state
  UPROPERTY()
  ECameraState CurrentState = ECameraState::Grounded;

  UPROPERTY()
  bool bIsAiming = false;

  UPROPERTY()
  bool bShoulderSwapped = false;

  // Effect layers
  UPROPERTY()
  TArray<FCameraEffectLayer> ActiveLayers;

  // Cached values for interpolation
  float CurrentFOV = 90.f;
  float CurrentLagSpeed = 8.f;
  FVector CurrentSocketOffset = FVector::ZeroVector;

  // Juice state tracking
  float PreviousVerticalVelocity = 0.f;
};
