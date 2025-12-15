// RopeRenderComponent.h - XPBD Restored Version (Build 6+)

#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "Components/SplineComponent.h"
#include "Components/SplineMeshComponent.h"
#include "RopeRenderComponent.generated.h"

// XPBD Particle
struct FRopeParticle
{
	FVector Position = FVector::ZeroVector;
    FVector OldPosition = FVector::ZeroVector; // Added for Verlet/XPBD consistency
	FVector PredictedPosition = FVector::ZeroVector;
	FVector PreviousPosition = FVector::ZeroVector;
	FVector Velocity = FVector::ZeroVector;
	float InverseMass = 1.0f; 
    bool bIsActive = false; 
};
// ... (skip lines) ...


// Virtual Segment Constraint
USTRUCT(BlueprintType)
struct FPinnedConstraint
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FVector WorldLocation;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 ParticleIndex; 

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bActive = true;
};

// Distance Constraint
struct FDistanceConstraint
{
	int32 IndexA;
	int32 IndexB;
	float RestLength;
	float Compliance = 0.0f;
};

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class LINKMEPROJECT_API URopeRenderComponent : public USceneComponent
{
	GENERATED_BODY()

public:
	URopeRenderComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void BeginPlay() override;

	/**
     * Rebuilds the rope topology.
     */
	UFUNCTION(BlueprintCallable, Category = "Rope")
	void UpdateRope(const TArray<FVector>& Points, bool bDeployingMode = false);

    /** Updates just the pin positions (Endpoints/Corners) without topology rebuild */
    UFUNCTION(BlueprintCallable, Category = "Rope")
    void UpdatePinPositions(const TArray<FVector>& Points);

    UFUNCTION(BlueprintCallable, Category = "Rope")
    void SetRopeDeploying(bool bDeploying);

    UFUNCTION(BlueprintCallable, Category = "Rope")
    void HideRope() { SetRopeHidden(true); }
    
    UFUNCTION(BlueprintCallable, Category = "Rope")
    void ResetRope() { ResetSimulation(); }
    
    UFUNCTION(BlueprintPure, Category = "Rope")
    bool IsRopeActive() const { return bInitialized && !bRopeHidden; }

    UFUNCTION(BlueprintCallable, Category = "Rope")
    void SetRopeHidden(bool bHidden);

    UPROPERTY(EditAnywhere, Category="Rope|Debug")
    bool bShowDebugSpline = false;

protected:

	// --- Config ---
    
	UPROPERTY(EditAnywhere, Category="Rope|Sim")
	int32 ParticleCount = 60; // Default reasonable count

	UPROPERTY(EditAnywhere, Category="Rope|Sim")
	int32 SubSteps = 4;

	UPROPERTY(EditAnywhere, Category="Rope|Sim")
	int32 SolverIterations = 2;

	UPROPERTY(EditAnywhere, Category="Rope|Sim")
	FVector Gravity = FVector(0, 0, -980.f);

	UPROPERTY(EditAnywhere, Category="Rope|Sim", meta=(ClampMin="0.0", ClampMax="1.0"))
	float Damping = 0.1f;

    // --- Visuals ---
    
	UPROPERTY(EditAnywhere, Category="Rope|Visuals")
	UStaticMesh* RopeMesh = nullptr;

	UPROPERTY(EditAnywhere, Category="Rope|Visuals")
	UMaterialInterface* RopeMaterial = nullptr;

	UPROPERTY(EditAnywhere, Category="Rope|Visuals")
	float RopeThickness = 5.0f;
    
    UPROPERTY(EditAnywhere, Category="Rope|Visuals")
    TEnumAsByte<ESplineMeshAxis::Type> ForwardAxis = ESplineMeshAxis::Z;



    // --- State ---
    
	TArray<FRopeParticle> Particles;
	TArray<FPinnedConstraint> PinConstraints;
	TArray<FDistanceConstraint> DistanceConstraints;

	bool bInitialized = false;
    bool bRopeHidden = false;
    bool bIsDeploying = false;

	// --- Components ---
	UPROPERTY()
	USplineComponent* RopeSpline;

	UPROPERTY()
	TArray<USplineMeshComponent*> SplineMeshes;

	// --- Internal ---
	void SimulateXPBD(float DeltaTime);
	void SolveConstraints(float Dt);
	void RebuildFromPoints(const TArray<FVector>& Points);
	void UpdateMeshes();
	void HideUnusedSegments(int32 ActiveCount);
    void ResetSimulation();
    USplineMeshComponent* GetPooledSegment(int32 Index);
};
