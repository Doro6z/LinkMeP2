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
UCLASS(Blueprintable, ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
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
	int32 SubSteps = 4; // Physics steps per frame

	UPROPERTY(EditAnywhere, Category="Rope|Sim")
	FVector Gravity = FVector(0, 0, -980.f);

	UPROPERTY(EditAnywhere, Category="Rope|Sim", meta=(ClampMin="0.0", ClampMax="1.0"))
	float PinStrength = 0.5f; // Now used for Soft Constraint Stiffness?

	UPROPERTY(EditAnywhere, Category="Rope|Sim", meta=(ClampMin="0.0001", ClampMax="1.0"))
	float BendPointCompliance = 0.0f; // 0 = Hard Pin, >0 = Saggy (Soft Pin)

	UPROPERTY(EditAnywhere, Category="Rope|Sim", meta=(ClampMin="0.0", ClampMax="1.0"))
	float Damping = 0.1f;
	UPROPERTY(EditAnywhere, Category="Rope|Sim", meta=(ClampMin="0.0", ClampMax="1.0"))
	float StraighteningAlpha = 0.5f; // 0 = no straightening, 1 = fully straight under tension
	
	UPROPERTY(EditAnywhere, Category="Rope|Straightening")
	bool bEnableStraightening = true;

	UPROPERTY(EditAnywhere, Category="Rope|Straightening", meta=(ClampMin="0.0", ClampMax="1.0"))
	float GravityScaleWhenTight = 0.2f;

	UPROPERTY(EditAnywhere, Category="Rope|Visuals")
	float MeshLengthBase = 10.0f; // User default 10.0

	// Maximum spacing between particles. Logic will prioritize density over count limit.
	UPROPERTY(EditAnywhere, Category="Rope|Visuals")
	float MaxParticleSpacing = 30.0f;

	UPROPERTY(EditAnywhere, Category="Rope|Visuals")
	float CornerRadius = 15.0f;

	UPROPERTY(EditAnywhere, Category="Rope|Visuals", meta=(ClampMin="2", ClampMax="10"))
	int32 CornerSubdivisions = 4;

	UPROPERTY(EditAnywhere, Category="Rope|Visuals")
	bool bEnableCornerRounding = true;

	UPROPERTY(EditAnywhere, Category="Rope|Physics")
	bool bEnableCollision = true;

	UPROPERTY(EditAnywhere, Category="Rope|Physics")
	TEnumAsByte<ECollisionChannel> RopeCollisionChannel = ECC_WorldStatic;

	UPROPERTY(EditAnywhere, Category="Rope|Visuals")
	UStaticMesh* RopeMesh = nullptr;

public:
	// ============ API PRINCIPALE ============
    
    /**
     * REBUILD TOPOLOGIQUE : Reconstruit toute la structure.
     * Appelé UNIQUEMENT quand le nombre de points change.
     * @param Points - [Start, ...Middles, End]. Minimum 2 éléments.
     * @param bDeployingMode - Si true (mode Flying), RestLength = Distance actuelle.
     */
    UFUNCTION(BlueprintCallable, Category="Rope")
    void UpdateRope(const TArray<FVector>& Points, bool bDeployingMode = false);
    
    /**
     * MISE À JOUR POSITIONS : Met à jour les Pins sans toucher à la structure.
     * Appelé à chaque frame pour suivre le joueur/hook.
     * @param Points - Mêmes points que UpdateRope, mais ne change PAS la topologie.
     */
    UFUNCTION(BlueprintCallable, Category="Rope")
    void UpdatePinPositions(const TArray<FVector>& Points);
    
    /**
     * MODE DÉPLOIEMENT (Flying) : Met à jour les RestLength dynamiquement.
     * Appelé à chaque frame pendant que le hook vole.
     * Fait que la corde "se déroule" au lieu de "claquer".
     */
    UFUNCTION(BlueprintCallable, Category="Rope")
    void SetRopeDeploying(bool bDeploying);
    
    /**
     * Cache la corde sans la détruire.
     */
    UFUNCTION(BlueprintCallable, Category="Rope")
    void HideRope();
    
    UFUNCTION(BlueprintCallable, Category="Rope")
    void SetRopeHidden(bool bHidden);

    void DrawDebugInfo();
    
    /**
     * Réinitialise complètement (clear + hide).
     */
    UFUNCTION(BlueprintCallable, Category="Rope")
    void ResetRope();
    
    /**
     * La corde est-elle actuellement visible et active?
     */
    UFUNCTION(BlueprintPure, Category="Rope")
    bool IsRopeActive() const { return bInitialized && !bRopeHidden; }
    
protected:
    // Apply straightening when rope is under tension
    void ApplyStraightening();
    
    // Flag: La corde est en mode "déploiement" (RestLength suit la distance)
    bool bIsDeploying = false;
    
    // Nombre de points de la dernière topologie (pour détecter les changements)
    int32 LastPointCount = 0;
    
    /** Returns current visual length of rope (sum of particle distances) */
    UFUNCTION(BlueprintPure, Category="Rope|State")
    float GetVisualRopeLength() const;

    // Interne: reconstruit les particules depuis les points
    void RebuildFromPoints(const TArray<FVector>& Points);
    
    // Interne: met à jour uniquement les positions des Pins (InverseMass = 0)
    void RefreshPinPositions(const TArray<FVector>& Points);
    
    // Interne: en mode Deploying, ajuste les RestLength pour suivre la distance
    void UpdateDeployingRestLengths();

	// --- Blueprint API: Rope State Queries ---
	
	/** Returns true if rope is currently at maximum tension (straight) */
	UFUNCTION(BlueprintPure, Category="Rope|State")
	bool IsRopeTaut() const { return bRopeIsTaut; }

	/** Returns normalized tension (0 = slack, 1 = max tension) */
	UFUNCTION(BlueprintPure, Category="Rope|State")
	float GetRopeTension() const;

	/** Force rope to specific visual state (for debugging/cutscenes) */
	UFUNCTION(BlueprintCallable, Category="Rope|Control")
	void SetRopeTautState(bool bTaut) { bRopeIsTaut = bTaut; }

public:
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

protected:
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
	TArray<USplineMeshComponent*> SplineMeshes;
};
