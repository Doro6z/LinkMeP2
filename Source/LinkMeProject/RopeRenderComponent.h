// RopeRenderComponent.h

#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "Components/SplineMeshComponent.h"
#include "Components/SplineComponent.h"
#include "RopeRenderComponent.generated.h"

struct FRopeParticle
{
	FVector Position;
	FVector PreviousPosition;
	bool bFree; // If false, this particle is hard-pinned (start/end)
};

/**
 * Hybrid Verlet Rope Renderer.
 * Simulates a high-resolution visual rope using Verlet integration,
 * loosely bound to the low-resolution gameplay BendPoints.
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class LINKMEPROJECT_API URopeRenderComponent : public USceneComponent
{
	GENERATED_BODY()

public:
	URopeRenderComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/** 
	 * Main update function called by RopeSystemComponent.
	 * Updates the simulation targets (BendPoints).
	 */
	void UpdateVisualSegments(const TArray<FVector>& BendPoints, const FVector& EndPosition);

protected:
	virtual void BeginPlay() override;

	/** Internal simulation steps */
	void SimulateParticles(float DeltaTime);
	void SolveDistanceConstraints();
	void SolveSoftConstraints(); // Pulls particles towards gameplay bendpoints
	
	/** Recreates the rope particles if necessary (e.g. first frame or reset) */
	void InitializeRope(const FVector& Start, const FVector& End);

	/** Updates the SplineComponent to match particle positions */
	void UpdateSplineRepresentation();

	/** Updates the visible SplineMeshes along the spline */
	void UpdateSplineMeshes();

	/** Component Pool Management */
	USplineMeshComponent* GetPooledSegment(int32 Index);
	void HideUnusedSegments(int32 ActiveCount);

protected:
	// -- Config --

	/** Length of each visual segment in the simulation. Smaller = smoother/heavier. */
	UPROPERTY(EditAnywhere, Category="Rope|Simulation")
	float SegmentLength = 20.0f;

	/** How strongly the rope is pulled towards the fixed gameplay bendpoints (0-1). */
	UPROPERTY(EditAnywhere, Category="Rope|Simulation")
	float SoftConstraintStrength = 0.2f;

	/** Number of relaxation iterations for constraints. */
	UPROPERTY(EditAnywhere, Category="Rope|Simulation")
	int32 SolverIterations = 4;

	UPROPERTY(EditAnywhere, Category="Rope|Simulation")
	FVector Gravity = FVector(0, 0, -980.f);

	UPROPERTY(EditAnywhere, Category="Rope|Simulation")
	float Damping = 0.95f;

	UPROPERTY(EditAnywhere, Category="Rope|Visuals")
	UStaticMesh* RopeMesh = nullptr;

	UPROPERTY(EditAnywhere, Category="Rope|Visuals")
	UMaterialInterface* RopeMaterial = nullptr;

	UPROPERTY(EditAnywhere, Category="Rope|Visuals")
	float RopeThickness = 5.0f;

	UPROPERTY(EditAnywhere, Category="Rope|Visuals")
	TEnumAsByte<ESplineMeshAxis::Type> ForwardAxis = ESplineMeshAxis::Z;

protected:
	// -- State --

	/** High-res visual particles */
	TArray<FRopeParticle> Particles;

	/** Copy of the latest gameplay bendpoints to target */
	TArray<FVector> TargetBendPoints;

	/** Spline component used for smoothing the visual path */
	UPROPERTY()
	USplineComponent* RopeSpline;

	/** Pool of meshes */
	UPROPERTY(Transient)
	TArray<USplineMeshComponent*> SegmentPool;
};
