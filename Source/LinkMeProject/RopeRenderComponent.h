// RopeRenderComponent.h

#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "Components/SplineMeshComponent.h"
#include "Components/SplineComponent.h"
#include "RopeRenderComponent.generated.h"

// XPBD Particle
struct FRopeParticle
{
	FVector Position = FVector::ZeroVector;
	FVector PredictedPosition = FVector::ZeroVector;
	FVector PreviousPosition = FVector::ZeroVector;
	FVector Velocity = FVector::ZeroVector;
	float InverseMass = 1.0f; // 0.0f = Pinned/Infinite Mass
    bool bIsActive = false;   // Pool Status
};

// Virtual Segment Constraint (Represents a BendPoint or Player attachment)
USTRUCT(BlueprintType)
struct FPinnedConstraint
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FVector WorldLocation;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 ParticleIndex; // The particle this pin constrains

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bActive = true;

    // Soft Magnetic Pin Parameters
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float MagneticRadius = 50.0f; // Radius of influence (cm) where attraction begins

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float MagneticStrength = 1.0f; // Multiplier for attraction force at center
};

// Distance Constraint for loop solving
struct FDistanceConstraint
{
	int32 IndexA;
	int32 IndexB;
	float RestLength;
	float Compliance = 0.0f; // XPBD compliance (0 = rigid)
};

/**
 * Rope Visual Manager V2 (XPBD + Virtual Segmentation)
 * 
 * Simulates a fixed number of particles (Visual Rope).
 * Instead of adding/removing particles on partial wraps, we apply
 * PIN Constraints to existing particles to match the Gameplay BendPoints.
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class LINKMEPROJECT_API URopeRenderComponent : public USceneComponent
{
	GENERATED_BODY()

public:
	URopeRenderComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void BeginPlay() override;

	/** Called by RopeSystemComponent when BendPoints change (OnRep or Server) */
	void UpdateVisualSegments(const TArray<FVector>& BendPoints, const FVector& EndPosition, float InCurrentLength, float InMaxLength);

protected:
    // Nouvelle approche : Stocke la longueur au repos spécifique pour chaque lien
    // ConstraintRestLengths[i] est la distance idéale entre Particle[i] et Particle[i+1]
    TArray<float> ConstraintRestLengths; 

    // Helper pour calculer combien de particules sont nécessaires pour une longueur donnée
    int32 CalculateSegmentParticleCount(float SegmentLength) const;

	// --- Simulation Core (XPBD) ---
	void SimulateXPBD(float DeltaTime);
	void SolveConstraints(float Dt);
	
	// --- Initialization ---
	void InitializeParticles(const FVector& Start, const FVector& End);
	void RebuildConstraints(const TArray<FVector>& BendPoints, const FVector& EndPosition);

	// --- Rendering ---
	void UpdateSplineInterpolation(); // Catmull-Rom Centripetal
	void ApplyCornerRounding(); // Corner-Rounding algorithm for smooth corners
	void UpdateMeshes();
    // ResampleParticles REMOVED

	// --- Util ---
	USplineMeshComponent* GetPooledSegment(int32 Index);
	void HideUnusedSegments(int32 ActiveCount);

protected:


	float TautThreshold = 0.95f;
bool bRopeIsTaut = false;
float CachedMaxRopeLength = 1000.0f;
float CachedCurrentRopeLength = 0.0f;
float CachedStiffnessAlpha = 0.0f; // Gradient tension [0.0 = slack, 1.0 = taut]
    // Pool Configuration
    UPROPERTY(EditAnywhere, Category="Rope|Sim")
    int32 MaxParticles = 200; // Fixed Pool Size

    // Current number of active particles in the pool
	UPROPERTY(VisibleAnywhere, Category="Rope|Sim")
    int32 ActiveParticleCount = 0;
	
	// Read-only debug view of actual count
	UPROPERTY(VisibleAnywhere, Category="Rope|Sim")
	int32 ParticleCount = 0;

	// Use dynamic resolution based on length?
	UPROPERTY(EditAnywhere, Category="Rope|Sim")
	bool bUseDynamicParticleCount = true;

	// If dynamic resolution is disabled, this fixed count is used
	UPROPERTY(EditAnywhere, Category="Rope|Sim", meta=(EditCondition="!bUseDynamicParticleCount", ClampMin="5"))
	int32 FixedParticleCount = 40;

    // Elasticity of the rope (0 = Rigid, >0 = Stretchy)
	UPROPERTY(EditAnywhere, Category="Rope|Sim", meta=(ClampMin="0.0", ClampMax="1.0"))
	float StretchCompliance = 0.0f;

	UPROPERTY(EditAnywhere, Category="Rope|Sim")
	int32 SolverIterations = 4;

	UPROPERTY(EditAnywhere, Category="Rope|Sim")
	int32 SubSteps = 2; // Physics steps per frame

	UPROPERTY(EditAnywhere, Category="Rope|Sim")
	FVector Gravity = FVector(0, 0, -980.f);

	UPROPERTY(EditAnywhere, Category="Rope|Sim", meta=(ClampMin="0.0", ClampMax="1.0"))
	float PinStrength = 0.2f; // Now used for Soft Constraint Stiffness?

	UPROPERTY(EditAnywhere, Category="Rope|Sim", meta=(ClampMin="0.0001", ClampMax="1.0"))
	float BendPointCompliance = 0.0f; // 0 = Hard Pin, >0 = Saggy (Soft Pin)

	UPROPERTY(EditAnywhere, Category="Rope|Sim", meta=(ClampMin="0.0", ClampMax="1.0"))
	float Damping = 0.98f;

	UPROPERTY(EditAnywhere, Category="Rope|Visuals")
	float MeshLengthBase = 10.0f; // User default 10.0

	// Maximum spacing between particles. Logic will prioritize density over count limit.
	UPROPERTY(EditAnywhere, Category="Rope|Visuals")
	float MaxParticleSpacing = 30.0f;

	// Curve defining stiffness (Y) based on Tension Ratio (X: 0=slack, 1=taut)
	UPROPERTY(EditAnywhere, Category="Rope|Visuals")
	UCurveFloat* TensionCurve;

	UPROPERTY(EditAnywhere, Category="Rope|Visuals", meta=(ClampMin="0.1", ClampMax="3.0"))
	float MaxMeshStretch = 1.5f;

	UPROPERTY(EditAnywhere, Category="Rope|Visuals", meta=(ClampMin="0.1", ClampMax="1.0"))
	float MinMeshStretch = 0.6f;

	UPROPERTY(EditAnywhere, Category="Rope|Visuals")
	float CornerRadius = 15.0f; // Radius for corner rounding (local smoothing)

	UPROPERTY(EditAnywhere, Category="Rope|Visuals", meta=(ClampMin="2", ClampMax="10"))
	int32 CornerSubdivisions = 4; // Number of points per corner arc

	UPROPERTY(EditAnywhere, Category="Rope|Visuals")
	bool bEnableCornerRounding = true;

	UPROPERTY(EditAnywhere, Category="Rope|Physics")
	bool bEnableCollision = true;

	UPROPERTY(EditAnywhere, Category="Rope|Physics")
	TEnumAsByte<ECollisionChannel> RopeCollisionChannel = ECC_WorldStatic;

	UPROPERTY(EditAnywhere, Category="Rope|Visuals")
	UStaticMesh* RopeMesh = nullptr;

public:
    // Reset rendering state (clear particles)
    void ResetRope();
    
    // Toggle visibility of the simulated rope
    UFUNCTION(BlueprintCallable, Category="Rope|Visuals")
    void SetRopeHidden(bool bHidden);

    // Debug Info
    void DrawDebugInfo();

    // --- BLUEPRINT API FOR MANUAL CONTROL ---
    
    UFUNCTION(BlueprintCallable, Category = "Rope|Control")
    void SetRopeParticles(const TArray<FVector>& Positions);

    UFUNCTION(BlueprintCallable, Category = "Rope|Control")
    TArray<FVector> GetRopeParticlePositions() const;

    UFUNCTION(BlueprintCallable, Category = "Rope|Control")
    void SetPinConstraints(const TArray<FPinnedConstraint>& NewPins);

    UFUNCTION(BlueprintCallable, Category = "Rope|Control")
    void UpdatePinLocation(int32 PinIndex, FVector NewLocation);

    UFUNCTION(BlueprintCallable, Category = "Rope|Control")
    void SetRopeSimulationParams(int32 InSubSteps, int32 InIterations, float InDamping, float InGravityScale);

    UFUNCTION(BlueprintCallable, Category = "Rope|Control")
    void SetVisualTensionParams(bool bEnableStraightening, float InStraighteningStiffness);

    UFUNCTION(BlueprintCallable, Category = "Rope|Control")
    void ForceRebuildConstraints(const TArray<FVector>& BendPoints, const FVector& EndPosition);

	// --- Blueprint API: Rope State Queries ---
	
	/** Returns true if rope is currently at maximum tension (straight) */
	UFUNCTION(BlueprintPure, Category="Rope|State")
	bool IsRopeTaut() const { return bRopeIsTaut; }

	/** Returns current visual length of rope (sum of particle distances) */
	UFUNCTION(BlueprintPure, Category="Rope|State")
	float GetVisualRopeLength() const;

	/** Returns normalized tension (0 = slack, 1 = max tension) */
	UFUNCTION(BlueprintPure, Category="Rope|State")
	float GetRopeTension() const;

	/** Force rope to specific visual state (for debugging/cutscenes) */
	UFUNCTION(BlueprintCallable, Category="Rope|Control")
	void SetRopeTautState(bool bTaut) { bRopeIsTaut = bTaut; }

	UPROPERTY(EditAnywhere, Category="Rope|Visuals")
	UMaterialInterface* RopeMaterial = nullptr;

	UPROPERTY(EditAnywhere, Category="Rope|Visuals")
	float RopeThickness = 5.0f;

	UPROPERTY(EditAnywhere, Category="Rope|Visuals")
	float MeshRadius = 5.0f;

	UPROPERTY(EditAnywhere, Category="Rope|Visuals")
	TEnumAsByte<ESplineMeshAxis::Type> ForwardAxis = ESplineMeshAxis::Z;

	UPROPERTY(EditAnywhere, Category="Rope|Debug")
	bool bShowDebugSpline = false;

    // Track internal visibility state
    UPROPERTY(VisibleAnywhere, Category="Rope|Visuals")
    bool bRopeHidden = false;

private:
	// State
	TArray<FRopeParticle> Particles;
	TArray<FPinnedConstraint> PinConstraints;
	TArray<FDistanceConstraint> DistanceConstraints;

	bool bInitialized = false;

	// Components
	UPROPERTY()
	USplineComponent* RopeSpline;

	UPROPERTY()
	TArray<USplineMeshComponent*> MeshPool;
};
