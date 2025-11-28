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

	// --- Configuration ---

	/** Class of the projectile to spawn as the hook. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="LinkMe|Config")
	TSubclassOf<ARopeHookActor> HookClass;

	/** Maximum length of the rope in units. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="LinkMe|Config")
	float MaxLength = 1500.f;

	/** Speed at which the rope reels in/out (units per second). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="LinkMe|Config")
	float ReelSpeed = 600.f;

	// Distance from the wall to place the bend point (prevents z-fighting/clipping)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rope System")
	float BendOffset = 15.0f;

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
	float CornerThresholdDegrees = 15.f;

	/** Number of Verlet particles for simulation (more = smoother but slower) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="LinkMe|Verlet")
	int32 NumSimParticles = 12;

	/** Radius for sphere sweep collision (rope thickness) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="LinkMe|Verlet")
	float RopeRadius = 8.0f;

	/** Gravity scale for Verlet particles */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="LinkMe|Verlet")
	float GravityScale = 0.3f;

	/** Number of substeps per frame for Verlet stability */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="LinkMe|Verlet")
	int32 SubSteps = 2;

	/** Number of constraint solver iterations */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="LinkMe|Verlet")
	int32 ConstraintIterations = 2;

	/** Minimum angle change to extract a bend point from particles */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="LinkMe|Verlet")
	float BendAngleThreshold = 20.0f;

	/** Show debug lines and spheres for gameplay validation. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="LinkMe|Debug")
	bool bShowDebug = true;

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

protected:
	// --- Internal Logic ---

	// Verlet simulation
	void InitializeSimulation();
	void StepSimulation(float DeltaTime);
	void ExtractBendPoints();

	// Legacy (now unused, kept for reference)
	void ManageBendPointsOld();
	
	void ManageRopeLength(float DeltaTime);
	void ApplyForcesToPlayer();
	void UpdateRopeVisual();
	bool LineTrace(const FVector& Start, const FVector& End, FHitResult& OutHit) const;

	UFUNCTION()
	void OnHookImpact(const FHitResult& Hit);
	void TransitionToAttached(const FHitResult& Hit);

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

	// Verlet simulation particles
	TArray<FRopeParticle> SimParticles;
	bool bNeedsReinitialize = true;
};
