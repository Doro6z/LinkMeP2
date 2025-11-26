// RopeRenderComponent.h

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "RopeRenderComponent.generated.h"

USTRUCT(BlueprintType)
struct FVerletPoint
{
GENERATED_BODY()

UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Rope")
FVector Position = FVector::ZeroVector;

UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Rope")
FVector LastPosition = FVector::ZeroVector;
};

/**
 * Client-side cosmetic rope renderer. It converts BendPoints from the gameplay rope
 * into locally simulated Verlet points, then exposes a ready-to-draw list of render
 * points (one per segment). Rendering can be handled by a procedural mesh or an
 * instanced static mesh component in Blueprint.
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class LINKMEPROJECT_API URopeRenderComponent : public UActorComponent
{
GENERATED_BODY()

public:
URopeRenderComponent();

/** Update the local verlet representation from gameplay bend points. */
void RefreshFromBendPoints(const TArray<FVector>& BendPoints);

/** Simulate cosmetic verlet points. Should be called every tick on the owning client. */
void Simulate(float DeltaTime);

/** Returns the current sequence of render points (one per simulated point). */
const TArray<FVector>& GetRenderPoints() const { return RenderPoints; }

protected:
virtual void BeginPlay() override;

/** Create the verlet chain from current BendPoints. */
void BuildVerletPoints();

/** Solve simple distance constraints along the chain. */
void SatisfyConstraints();

protected:
/** Number of cosmetic segments generated between each gameplay bend point. */
UPROPERTY(EditAnywhere, Category="Rope|Cosmetic")
int32 SubdivisionsPerSegment = 4;

/** Damping factor for verlet integration (0..1). */
UPROPERTY(EditAnywhere, Category="Rope|Cosmetic")
float Damping = 0.96f;

/** Gravity multiplier applied to cosmetic points. */
UPROPERTY(EditAnywhere, Category="Rope|Cosmetic")
float GravityScale = 0.5f;

/** How many solver iterations to run per frame. */
UPROPERTY(EditAnywhere, Category="Rope|Cosmetic")
int32 SolverIterations = 6;

/** Points received from gameplay (anchor to player). */
TArray<FVector> BendPoints;

/** Locally simulated verlet points. */
TArray<FVerletPoint> VerletPoints;

/** Points exposed to external renderers (copy of Verlet positions). */
TArray<FVector> RenderPoints;
};
