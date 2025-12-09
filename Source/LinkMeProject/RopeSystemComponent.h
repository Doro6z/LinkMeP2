// RopeSystemComponent.h - REFACTORED FOR BLUEPRINT LOGIC

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "RopeSystemComponent.generated.h"

class ARopeHookActor;
class URopeRenderComponent;

UENUM(BlueprintType)
enum class ERopeState : uint8
{
	Idle,
	Flying,
	Attached
};

/**
 * Lightweight rope brain: manages state, forces, and provides tools for Blueprint logic.
 * Wrap/Unwrap logic is intentionally left to Blueprint for rapid iteration.
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent), BlueprintType, Blueprintable)
class LINKMEPROJECT_API URopeSystemComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	URopeSystemComponent();

	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// ===================================================================
	// ACTIONS - Called from Blueprint Input Handlers
	// ===================================================================

	/** Launch the hook in the specified direction. */
	UFUNCTION(BlueprintCallable, Category="Rope|Actions")
	void FireHook(const FVector& Direction);

	/** Server RPC for FireHook */
	UFUNCTION(Server, Reliable)
	void ServerFireHook(const FVector& Direction);

	/** Cut the rope and detach. */
	UFUNCTION(BlueprintCallable, Category="Rope|Actions")
	void Sever();

	/** Server RPC for Sever */
	UFUNCTION(Server, Reliable)
	void ServerSever();

	/** Retract the rope (shorten CurrentLength). Call in Tick if button held. */
	UFUNCTION(BlueprintCallable, Category="Rope|Actions")
	void ReelIn(float DeltaTime);

	UFUNCTION(Server, Unreliable)
	void ServerReelIn(float DeltaTime);

	/** Extend the rope (increase CurrentLength). Call in Tick if button held. */
	UFUNCTION(BlueprintCallable, Category="Rope|Actions")
	void ReelOut(float DeltaTime);

	UFUNCTION(Server, Unreliable)
	void ServerReelOut(float DeltaTime);

	// ===================================================================
	// BENDPOINT MANAGEMENT - Blueprint API
	// ===================================================================

	/** Add a new bendpoint before the player (last element). */
	UFUNCTION(BlueprintCallable, Category="Rope|BendPoints")
	void AddBendPoint(const FVector& Location);

	/** Remove the bendpoint at the given index. */
	UFUNCTION(BlueprintCallable, Category="Rope|BendPoints")
	void RemoveBendPointAt(int32 Index);

	/** Get the last fixed bendpoint (the one before the player). */
	UFUNCTION(BlueprintPure, Category="Rope|BendPoints")
	FVector GetLastFixedPoint() const;

	/** Get the player position (last bendpoint). */
	UFUNCTION(BlueprintPure, Category="Rope|BendPoints")
	FVector GetPlayerPosition() const;

	/** Get the anchor position (first bendpoint). */
	UFUNCTION(BlueprintPure, Category="Rope|BendPoints")
	FVector GetAnchorPosition() const;

	/** Update the player position (last bendpoint). Called automatically in Tick. */
	UFUNCTION(BlueprintCallable, Category="Rope|BendPoints")
	void UpdatePlayerPosition();

	// ===================================================================
	// TRACE UTILITIES - For Blueprint Wrap/Unwrap Logic
	// ===================================================================

	/** 
	 * Perform a capsule sweep between two points.
	 * Returns true if hit, fills OutHit with result.
	 */
	UFUNCTION(BlueprintCallable, Category="Rope|Trace")
	bool CapsuleSweepBetween(
		const FVector& Start, 
		const FVector& End, 
		FHitResult& OutHit,
		float Radius = 8.0f,
		bool bTraceComplex = true
	);

	/**
	 * Perform sphere traces at substep intervals between Start and End.
	 * Returns the last clear position before hitting geometry.
	 */
	UFUNCTION(BlueprintCallable, Category="Rope|Trace")
	FVector FindLastClearPoint(
		const FVector& Start,
		const FVector& End,
		int32 Subdivisions = 5,
		float SphereRadius = 15.0f
	);

	/**
	 * Compute a bendpoint location offset from a hit surface.
	 * Pushes the point away from the surface by Offset distance.
	 */
	UFUNCTION(BlueprintPure, Category="Rope|Trace")
	FVector ComputeBendPointFromHit(const FHitResult& Hit, float Offset = 15.0f) const;

	// ===================================================================
	// STATE ACCESS - Read-Only
	// ===================================================================

	UFUNCTION(BlueprintPure, Category="Rope|State")
	const TArray<FVector>& GetBendPoints() const { return BendPoints; }

	UFUNCTION(BlueprintPure, Category="Rope|State")
	int32 GetBendPointCount() const { return BendPoints.Num(); }

	UFUNCTION(BlueprintPure, Category="Rope|State")
	float GetCurrentLength() const { return CurrentLength; }

	UFUNCTION(BlueprintPure, Category="Rope|State")
	float GetMaxLength() const { return MaxLength; }

	UFUNCTION(BlueprintPure, Category="Rope|State")
	ERopeState GetRopeState() const { return RopeState; }

	UFUNCTION(BlueprintPure, Category="Rope|State")
	bool IsRopeAttached() const { return RopeState == ERopeState::Attached; }

	UFUNCTION(BlueprintPure, Category="Rope|State")
	ARopeHookActor* GetCurrentHook() const { return CurrentHook; }

	// ===================================================================
	// EVENTS - Blueprint Implementable
	// ===================================================================

	/** Called every tick when rope is attached. Implement wrap/unwrap logic here. */
	UFUNCTION(BlueprintImplementableEvent, Category="Rope|Events")
	void OnRopeTickAttached(float DeltaTime);

	/** Called when hook impacts and rope becomes attached. */
	UFUNCTION(BlueprintImplementableEvent, Category="Rope|Events")
	void OnRopeAttached(const FHitResult& ImpactHit);

	/** Called when rope is severed. */
	UFUNCTION(BlueprintImplementableEvent, Category="Rope|Events")
	void OnRopeSevered();

	// ===================================================================
	// CONFIGURATION
	// ===================================================================

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Rope|Config")
	TSubclassOf<ARopeHookActor> HookClass;

	/** Trace channel used for rope collision detection. Set to custom RopeTrace channel to ignore player. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Rope|Config")
	TEnumAsByte<ECollisionChannel> RopeTraceChannel = ECC_Visibility;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Rope|Config")
	float MaxLength = 3500.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Rope|Config")
	float ReelSpeed = 600.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Rope|Physics")
	float SpringStiffness = 1600.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Rope|Physics")
	float SwingTorque = 40000.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Rope|Physics")
	float AirControlForce = 20000.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Rope|Debug")
	bool bShowDebug = true;

protected:
	void ApplyForcesToPlayer();
	void UpdateRopeVisual();
	
	UFUNCTION()
	void OnHookImpact(const FHitResult& Hit);
	
	void TransitionToAttached(const FHitResult& Hit);

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Replicated, Category="Rope|State")
	float CurrentLength = 0.f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, ReplicatedUsing=OnRep_BendPoints, Category="Rope|State")
	TArray<FVector> BendPoints;

	UFUNCTION()
	void OnRep_BendPoints();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Replicated, Category="Rope|State")
	ERopeState RopeState = ERopeState::Idle;

	UPROPERTY(Transient, Replicated)
	ARopeHookActor* CurrentHook = nullptr;

	UPROPERTY(Transient)
	URopeRenderComponent* RenderComponent = nullptr;

	float DefaultBrakingDeceleration = 0.f;
};
