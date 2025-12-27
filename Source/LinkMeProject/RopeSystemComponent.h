// RopeSystemComponent.h - REFACTORED FOR BLUEPRINT LOGIC

#pragma once

#include "Components/ActorComponent.h"
#include "CoreMinimal.h"
#include "RopeTypes.h"

#include "RopeSystemComponent.generated.h"

class ARopeHookActor;
class URopeRenderComponent;

UENUM(BlueprintType)
enum class ERopeState : uint8 { Idle, Flying, Attached };

/** Apex timing tier for SwingJump feedback */
UENUM(BlueprintType)
enum class EApexTier : uint8 {
  None,   // Not in apex window
  OK,     // Late in window (40%+ progress)
  Good,   // Mid window (10-40% progress)
  Perfect // Early in window (0-10% progress)
};

/** Delegate for Apex Jump events */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnApexJump, EApexTier, Tier);

USTRUCT(BlueprintType)
struct FSwingPhysicsSettings {
  GENERATED_BODY()

  /** Tangential boost force when input aligns with velocity */
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rope|Physics",
            meta = (ClampMin = "0.0"))
  float TangentialBoost = 1000.0f;

  /** Lateral force for steering in air */
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rope|Physics",
            meta = (ClampMin = "0.0"))
  float AirControlForce = 50.0f;

  /** Artificial force to keep rope taut */
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rope|Physics",
            meta = (ClampMin = "0.0"))
  float CentripetalBias = 0.5f;

  /** Friction applied against velocity direction */
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rope|Physics",
            meta = (ClampMin = "0.0"))
  float VelocityDamping = 0.1f;
};

/**
 * Lightweight rope brain: manages state, forces, and provides tools for
 * Blueprint logic. Wrap/Unwrap logic is intentionally left to Blueprint for
 * rapid iteration.
 */
UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent),
       BlueprintType, Blueprintable)
class LINKMEPROJECT_API URopeSystemComponent : public UActorComponent {
  GENERATED_BODY()

public:
  URopeSystemComponent();

  virtual void BeginPlay() override;
  virtual void
  TickComponent(float DeltaTime, enum ELevelTick TickType,
                FActorComponentTickFunction *ThisTickFunction) override;
  virtual void GetLifetimeReplicatedProps(
      TArray<FLifetimeProperty> &OutLifetimeProps) const override;

  // ===================================================================
  // ACTIONS - Called from Blueprint Input Handlers
  // ===================================================================

  /** Launch the hook in the specified direction with default force. */
  UFUNCTION(BlueprintCallable, Category = "Rope|Actions")
  void FireHook(const FVector &Direction);

  /** Launch the hook with a specific velocity vector (for charged throws). */
  UFUNCTION(BlueprintCallable, Category = "Rope|Actions")
  void FireChargedHook(const FVector &Velocity);

  /** Server RPC for FireHook */
  UFUNCTION(Server, Reliable)
  void ServerFireHook(const FVector &Direction);

  /** Server RPC for FireChargedHook */
  UFUNCTION(Server, Reliable)
  void ServerFireChargedHook(const FVector &Velocity);

  /** Cut the rope and detach. */
  UFUNCTION(BlueprintCallable, Category = "Rope|Actions")
  void Sever();

  /** Release rope with a velocity boost. Call this instead of Sever for
   * skillful exit. */
  UFUNCTION(BlueprintCallable, Category = "Rope|Actions")
  void SwingJump(float BoostMultiplier = 1.2f);

  /** Called when SwingJump is executed. Implement in BP for VFX/Audio. */
  UFUNCTION(BlueprintImplementableEvent, Category = "Rope|Events")
  void OnSwingJump();

  /** Detaches the hook from the system without destroying it. Used when
   * MaxLength is exceeded. */
  void Detach();

  /** Server RPC for Sever */
  UFUNCTION(Server, Reliable)
  void ServerSever();

  /** Retract the rope (shorten CurrentLength). Call in Tick if button held. */
  UFUNCTION(BlueprintCallable, Category = "Rope|Actions")
  void ReelIn(float DeltaTime);

  UFUNCTION(Server, Unreliable)
  void ServerReelIn(float DeltaTime);

  /** Extend the rope (increase CurrentLength). Call in Tick if button held. */
  UFUNCTION(BlueprintCallable, Category = "Rope|Actions")
  void ReelOut(float DeltaTime);

  UFUNCTION(Server, Unreliable)
  void ServerReelOut(float DeltaTime);

  // ===================================================================
  // BENDPOINT MANAGEMENT - Blueprint API
  // ===================================================================

  /** Add a new bendpoint before the player (last element). */
  UFUNCTION(BlueprintCallable, Category = "Rope|BendPoints")
  void AddBendPoint(const FVector &Location);

  /**
   * Add a new bendpoint with surface normal capture (RECOMMENDED for Surface
   * Normal Validation). This overload should be used when adding bend points
   * from wrap detection logic.
   */
  UFUNCTION(BlueprintCallable, Category = "Rope|BendPoints")
  void AddBendPointWithNormal(const FVector &Location,
                              const FVector &SurfaceNormal);

  /** Remove the bendpoint at the given index. */
  UFUNCTION(BlueprintCallable, Category = "Rope|BendPoints")
  void RemoveBendPointAt(int32 Index);

  /** Get the last fixed bendpoint (the one before the player). */
  UFUNCTION(BlueprintPure, Category = "Rope|BendPoints")
  FVector GetLastFixedPoint() const;

  /** Get the player position (last bendpoint). */
  UFUNCTION(BlueprintPure, Category = "Rope|BendPoints")
  FVector GetPlayerPosition() const;

  /** Get the anchor position (first bendpoint). */
  UFUNCTION(BlueprintPure, Category = "Rope|BendPoints")
  FVector GetAnchorPosition() const;

  /** Update the player position (last bendpoint). Called automatically in Tick.
   */
  UFUNCTION(BlueprintCallable, Category = "Rope|BendPoints")
  void UpdatePlayerPosition();

  // ===================================================================
  // TRACE UTILITIES - For Blueprint Wrap/Unwrap Logic
  // ===================================================================

  /**
   * Perform a capsule sweep between two points.
   * Returns true if hit, fills OutHit with result.
   */
  UFUNCTION(BlueprintCallable, Category = "Rope|Trace")
  bool CapsuleSweepBetween(const FVector &Start, const FVector &End,
                           FHitResult &OutHit, float Radius = 8.0f,
                           bool bTraceComplex = true);

  /**
   * Perform sphere traces at substep intervals between Start and End.
   * Returns the last clear position before hitting geometry.
   */
  UFUNCTION(BlueprintCallable, Category = "Rope|Trace")
  FVector FindLastClearPoint(const FVector &Start, const FVector &End,
                             int32 Subdivisions = 5, float SphereRadius = 15.0f,
                             bool bShowDebugDraw = false);

  /**
   * Compute a bendpoint location offset from a hit surface.
   * Pushes the point away from the surface by Offset distance.
   */
  UFUNCTION(BlueprintPure, Category = "Rope|Trace")
  FVector ComputeBendPointFromHit(const FHitResult &Hit,
                                  float Offset = 15.0f) const;

  // ===================================================================
  // SURFACE NORMAL VALIDATION - For Robust Unwrap Logic
  // ===================================================================

  /**
   * Calculate the pressure direction (force bisector) at a bend point.
   * This is the direction the rope "pushes" against the corner.
   *
   * @param PointA - Previous fixed point
   * @param PointB - Current bend point
   * @param PointP - Player position
   * @return Normalized pressure direction vector (or ZeroVector if rope is
   * perfectly straight)
   */
  UFUNCTION(BlueprintPure, Category = "Rope|Physics")
  static FVector CalculatePressureDirection(const FVector &PointA,
                                            const FVector &PointB,
                                            const FVector &PointP);

  /**
   * Check if the rope is pulling away from the surface (safe to unwrap).
   * Compares pressure direction with surface normal.
   *
   * @param PressureDir - Direction rope pushes (from
   * CalculatePressureDirection)
   * @param SurfaceNormal - Normal of the surface at bend point
   * @param Tolerance - Dot product threshold (-0.05 recommended)
   * @return True if rope is pulling away from surface, False if still pushing
   * against it
   */
  UFUNCTION(BlueprintPure, Category = "Rope|Physics")
  static bool IsRopePullingAway(const FVector &PressureDir,
                                const FVector &SurfaceNormal,
                                float Tolerance = -0.05f);

  /**
   * Complete three-tier unwrap validation: Angle + Surface Normal + LineTrace.
   * This is the recommended way to check if unwrapping is safe.
   *
   * @param PrevFixed - Previous fixed bend point (A)
   * @param PrevFixedNormal - Surface normal at A
   * @param CurrentBend - Current bend point to potentially remove (B)
   * @param CurrentBendNormal - Surface normal at B
   * @param PlayerPos - Current player position (P)
   * @param AngleThreshold - Minimum angle for unwrap (default 178° = -0.999
   * dot)
   * @param bCheckLineTrace - Whether to perform final line trace validation
   * @return True if safe to unwrap, False otherwise
   */
  UFUNCTION(BlueprintCallable, Category = "Rope|Validation")
  bool ShouldUnwrapPhysical(const FVector &PrevFixed,
                            const FVector &CurrentBend,
                            const FVector &CurrentBendNormal,
                            const FVector &PlayerPos,
                            float AngleThreshold = -0.999f,
                            bool bCheckLineTrace = true);

  // ===================================================================
  // WRAPPING LOGIC
  // ===================================================================

  // Native CheckForWrapping/Unwrapping removed in favor of Blueprint Logic
  // Use CapsuleSweepBetween and AddBendPoint directly in BP.

  // ===================================================================
  // STATE ACCESS - Read-Only
  // ===================================================================

  UFUNCTION(BlueprintPure, Category = "Rope|State")
  const TArray<FVector> &GetBendPoints() const { return BendPoints; }

  UFUNCTION(BlueprintPure, Category = "Rope|State")
  int32 GetBendPointCount() const { return BendPoints.Num(); }

  UFUNCTION(BlueprintPure, Category = "Rope|State")
  float GetCurrentLength() const { return CurrentLength; }

  UFUNCTION(BlueprintPure, Category = "Rope|State")
  float GetMaxLength() const { return MaxLength; }

  UFUNCTION(BlueprintPure, Category = "Rope|State")
  ERopeState GetRopeState() const { return RopeState; }

  UFUNCTION(BlueprintPure, Category = "Rope|State")
  bool IsRopeAttached() const { return RopeState == ERopeState::Attached; }

  UFUNCTION(BlueprintPure, Category = "Rope|State")
  ARopeHookActor *GetCurrentHook() const { return CurrentHook; }

  // ===================================================================
  // EVENTS - Blueprint Implementable
  // ===================================================================

  /** Called every tick when rope is Flying. Implement collision/wrap and
   * tension logic here. */
  UFUNCTION(BlueprintImplementableEvent, Category = "Rope|Events")
  void OnRopeTickFlying(float DeltaTime);

  /** Called every tick when rope is Attached. Implement wrap/unwrap logic here.
   */
  UFUNCTION(BlueprintImplementableEvent, Category = "Rope|Events")
  void OnRopeTickAttached(float DeltaTime);

  /** Called when hook impacts and rope becomes attached. */
  UFUNCTION(BlueprintImplementableEvent, Category = "Rope|Events")
  void OnRopeAttached(const FHitResult &ImpactHit);

  /** Called when rope is severed. */
  UFUNCTION(BlueprintImplementableEvent, Category = "Rope|Events")
  void OnRopeSevered();

  // ===================================================================
  // PHYSICS HELPERS
  // ===================================================================
  /**
   * Reels the hook back to the first bendpoint and transitions to Attached
   * state. Called when BendPointCount > 0 during Flying state.
   */
  UFUNCTION(BlueprintCallable, Category = "Rope|Physics")
  void ReelInToFirstBendPoint();

  // ===================================================================
  // CONFIGURATION
  // ===================================================================

  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rope|Config")
  TSubclassOf<ARopeHookActor> HookClass;

  /** Socket name on the Character Mesh to spawn the hook from (e.g. 'hand_r').
   * If empty or not found, uses default offset. */
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rope|Config")
  FName HandSocketName = NAME_None;

  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rope|Config")
  FSwingPhysicsSettings SwingSettings;

  /** Trace channel used for rope collision detection. Set to custom RopeTrace
   * channel to ignore player. */
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rope|Config")
  TEnumAsByte<ECollisionChannel> RopeTraceChannel = ECC_Visibility;

  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rope|Config")
  float MaxLength = 3500.f;

  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rope|Config")
  float ReelSpeed = 600.f;

  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rope|Physics")
  float SpringStiffness = 1600.f;

  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rope|Physics")
  float SwingTorque = 40000.f;

  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rope|Physics")
  float AirControlForce = 20000.f;

  // Performance: Physics update rate (Hz). Higher = more responsive, lower =
  // better performance
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rope|Performance",
            meta = (ClampMin = "10", ClampMax = "60"))
  float PhysicsUpdateRate = 20.0f;

  /** If true, uses a fixed timer for physics (stable but can stutter). If
   * false, runs on Tick (smoother but fps dependent). */
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rope|Performance")
  bool bUseSubsteppedPhysics = true;

  UPROPERTY()
  class URopeCameraManager *CachedCameraManager;

  // Debug settings
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rope|Debug")
  bool bShowDebug = false;

  // ===================================================================
  // APEX WINDOW CONFIGURATION
  // ===================================================================

  /** Duration of the apex window in seconds */
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rope|Apex")
  float ApexFrameTime = 0.3f;

  /** Velocity Z threshold to consider apex (|VelZ| < this) */
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rope|Apex")
  float ApexVelocityThreshold = 100.f;

  /** Curve mapping window progress (X: 0-1) to boost multiplier (Y: 0-1) */
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rope|Apex")
  class UCurveFloat *ApexBoostCurve = nullptr;

  /** Maximum boost multiplier at curve value 1.0 */
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rope|Apex")
  float MaxApexBoost = 1.5f;

  /** Boost value threshold for Perfect tier (≥ this = Perfect) */
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rope|Apex|Tiers",
            meta = (ClampMin = "0", ClampMax = "1"))
  float PerfectBoostThreshold = 0.8f;

  /** Boost value threshold for Good tier (≥ this = Good, < Perfect) */
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rope|Apex|Tiers",
            meta = (ClampMin = "0", ClampMax = "1"))
  float GoodBoostThreshold = 0.6f;

  /** Swing arc position (0-1) where apex window opens. 0.5 = top of arc. */
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rope|Apex",
            meta = (ClampMin = "0", ClampMax = "1"))
  float SwingArcApexStart = 0.4f;

  /** Swing arc position (0-1) where apex window closes. */
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rope|Apex",
            meta = (ClampMin = "0", ClampMax = "1"))
  float SwingArcApexEnd = 0.6f;

  /** Multicast event fired on apex jump (use for UI/Audio) */
  UPROPERTY(BlueprintAssignable, Category = "Rope|Events")
  FOnApexJump OnApexJump;

  /** Client-side implementable event for VFX */
  UFUNCTION(BlueprintImplementableEvent, Category = "Rope|Events")
  void OnApexJump_Client(EApexTier Tier);

protected:
  FVector CalculateSwingForces(const FVector &CurrentVelocity,
                               const FVector &InputVector) const;
  void ApplySwingVelocity(const FVector &NewVelocity);
  void ApplyForcesToPlayer();
  void UpdateRopeVisual();

  // Timer-based physics tick (called at PhysicsUpdateRate)
  void PhysicsTick();

  // Actual physics logic
  void PerformPhysics(float DeltaTime);

  UFUNCTION()
  void OnHookImpact(const FHitResult &Hit);

  void TransitionToAttached(const FHitResult &Hit);

  // Timer handle for physics updates
  FTimerHandle PhysicsTimerHandle;

  // Apex window state
  bool bIsInApexWindow = false;
  float ApexWindowTimer = 0.f;

  /** Update apex detection logic (called in Tick) */
  void UpdateApexDetection(float DeltaTime);

  /** Determine tier based on boost percentage (0-1) */
  EApexTier DetermineTierFromBoost(float BoostPercent) const;

protected:
  UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Replicated,
            Category = "Rope|State")
  float CurrentLength = 0.f;

  /** Bend point positions - Replicated for network efficiency (12 bytes per
   * point) */
  UPROPERTY(VisibleAnywhere, BlueprintReadOnly,
            ReplicatedUsing = OnRep_BendPoints, Category = "Rope|State")
  TArray<FVector> BendPoints;

  /**
   * Surface normals for each bend point - NOT replicated for bandwidth
   * optimization. Only used server-side for Surface Normal Validation. Clients
   * can recalculate if needed or use simplified unwrap logic.
   */
  UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Rope|State")
  TArray<FVector> BendPointNormals;

  UFUNCTION()
  void OnRep_BendPoints();

  UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Replicated,
            Category = "Rope|State")
  ERopeState RopeState = ERopeState::Idle;

  UPROPERTY(Transient, Replicated)
  ARopeHookActor *CurrentHook = nullptr;

  UPROPERTY(Transient)
  URopeRenderComponent *RenderComponent = nullptr;

  // Rendering State Tracking
  UPROPERTY(Transient)
  ERopeState LastRopeState = ERopeState::Idle;

  UPROPERTY(Transient)
  int32 LastPointCount = 0;

  float DefaultBrakingDeceleration = 0.f;
};
