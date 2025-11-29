// RopeSystemComponent.h

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "RopeSystemComponent.generated.h"

class ARopeHookActor;
class URopeRenderComponent;

/** Lightweight particle for Verlet rope simulation */
USTRUCT()
struct FRopeParticle
{
	GENERATED_BODY()

	UPROPERTY()
	FVector Position = FVector::ZeroVector;

	UPROPERTY()
	FVector PrevPosition = FVector::ZeroVector;

	UPROPERTY()
	bool bAnchored = false;

	FRopeParticle() = default;
	FRopeParticle(const FVector& InPos, bool bInAnchored = false)
		: Position(InPos), PrevPosition(InPos), bAnchored(bInAnchored)
	{}
};

UENUM(BlueprintType)
enum class ERopeState : uint8
{
	Idle,
	Flying,
	Attached
};

/**
 * Gameplay rope brain: manages bend points, length, forces, and hook lifecycle.
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent), BlueprintType, Blueprintable)
class LINKMEPROJECT_API URopeSystemComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	URopeSystemComponent();

	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	// --- Actions ---

	/** Launch the hook in the specified direction. */
	UFUNCTION(BlueprintCallable, Category="LinkMe|Actions")
	void FireHook(const FVector& Direction);

	/** Cut the rope and detach. */
	UFUNCTION(BlueprintCallable, Category="LinkMe|Actions")
	void Sever();

	/** Retract the rope (shorten length). */
	UFUNCTION(BlueprintCallable, Category="LinkMe|Actions")
	void ReelIn(float DeltaTime);

	/** Extend the rope (lengthen). */
	UFUNCTION(BlueprintCallable, Category="LinkMe|Actions")
	void ReelOut(float DeltaTime);

	// Distance from the wall to place the bend point (prevents z-fighting/clipping)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rope System")
	float BendOffset = 15.0f;

	/** Maximum length of the rope in units. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="LinkMe|Config")
	float MaxLength = 3500.f;

	/** Speed at which the rope reels in/out (units per second). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="LinkMe|Config")
	float ReelSpeed = 600.f;

	/** Stiffness of the rope spring force. Higher = stiffer. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="LinkMe|Config")
	float SpringStiffness = 1600.f;

	/** Torque applied to swing around the anchor. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="LinkMe|Config")
	float SwingTorque = 40000.f;

	/** Force applied by player input during swing (Air Control). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="LinkMe|Config")
	float AirControlForce = 20000.f;

	/** Minimum angle (in degrees) required to create a bend point. Prevents wrapping on flat walls. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="LinkMe|Config")
	float CornerThresholdDegrees = 5.f;

	/** Minimum length of a segment to be created. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="LinkMe|Config")
	float MinSegmentLength = 20.f;

	/** Cooldown after wrapping before another wrap can occur (seconds). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="LinkMe|Config")
	float WrapCooldown = 0.2f;

	/** Cooldown after unwrapping before another unwrap can occur (seconds). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="LinkMe|Config")
	float UnwrapCooldown = 0.2f;

	/** Radius for sphere/capsule sweep collision (rope thickness) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="LinkMe|Collision")
	float RopeRadius = 8.0f;

	/** Show debug lines and spheres for gameplay validation. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="LinkMe|Debug")
	bool bShowDebug = true;

	/** Class of the projectile to spawn as the hook. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="LinkMe|Config")
	TSubclassOf<ARopeHookActor> HookClass;

	// --- State Access ---

	/** Get the current list of bend points (Anchor -> ... -> Player). */
	UFUNCTION(BlueprintPure, Category="LinkMe|State")
	const TArray<FVector>& GetBendPoints() const { return BendPoints; }

	/** Get the current active length of the rope. */
	UFUNCTION(BlueprintPure, Category="LinkMe|State")
	float GetCurrentLength() const { return CurrentLength; }

	/** Get the current state of the rope system. */
	UFUNCTION(BlueprintPure, Category="LinkMe|State")
	ERopeState GetRopeState() const { return RopeState; }

	/** Helper to get the last fixed bend point (the one before the player). */
	FVector GetLastFixedPoint() const;

protected:
	// --- Internal Logic ---

	/** Main gameplay loop for rope logic: updates bend points, handles wrapping/unwrapping. */
	void ManageBendPoints(float DeltaTime);

	/** Checks if the dynamic segment hits geometry and creates a new bend point. */
	void CheckForWrapping(float DeltaTime);

	/** Checks if the last bend point should be removed (unwrapped). */
	void CheckForUnwrapping(float DeltaTime);

	/** Refines the impact point to be slightly off the wall to prevent clipping. */
	FVector RefineImpactPoint(const FVector& Start, const FVector& End, const FHitResult& InitialHit);

	/** Nouvelle version hybride : utilise triangle + edge + push-out pour créer un bend point robuste. */
	FVector ComputeWarpBendPoint(const FVector& Start, const FVector& End, const FHitResult& Hit);

	void ApplyForcesToPlayer();
	void UpdateRopeVisual();
	
	UFUNCTION()
	void OnHookImpact(const FHitResult& Hit);
	void TransitionToAttached(const FHitResult& Hit);

    // Legacy swing toggle
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="LinkMe|Config")
    bool bUseLegacySwing = false;

    void ApplySwingLegacy();

	// --- Deprecated / Legacy Simulation (Unused) ---
	// void InitializeSimulation();
	// void StepSimulation(float DeltaTime);
	// void ExtractBendPoints();

protected:
	// --- Internal State ---

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="LinkMe|State")
	float CurrentLength = 0.f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="LinkMe|State")
	TArray<FVector> BendPoints;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="LinkMe|State")
	ERopeState RopeState = ERopeState::Idle;

	UPROPERTY(Transient)
	ARopeHookActor* CurrentHook = nullptr;

	UPROPERTY(Transient)
	URopeRenderComponent* RenderComponent = nullptr;

	float DefaultBrakingDeceleration = 0.f;

	// Cooldown timers
	float WrapCooldownTimer = 0.f;
	float UnwrapCooldownTimer = 0.f;

	// Last wrap position to prevent re-wrapping at the same spot
	FVector LastWrapPosition = FVector::ZeroVector;

	// Legacy particles (kept only if needed for compilation, but unused)
	// TArray<FRopeParticle> SimParticles; 
	// bool bNeedsReinitialize = true;
};
