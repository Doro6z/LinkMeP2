#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "RopeMeshUtils.generated.h"

struct FTriangleData
{
    FVector A;
    FVector B;
    FVector C;
    FVector Normal;
    bool bValid = false;
};

UCLASS()
class LINKMEPROJECT_API URopeMeshUtils : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    static FTriangleData GetTriangleFromHit(const FHitResult& Hit);
    static void GetClosestEdgeOnTriangle(const FVector& Point, const FVector& A, const FVector& B, const FVector& C, FVector& OutEdgeA, FVector& OutEdgeB);
    static FVector ClosestPointOnSegment(const FVector& Start, const FVector& End, const FVector& Point);
};
