// RopeRenderComponent.cpp

#include "RopeRenderComponent.h"
#include "GameFramework/Actor.h"
#include "Components/InstancedStaticMeshComponent.h"

URopeRenderComponent::URopeRenderComponent()
{
	PrimaryComponentTick.bCanEverTick = false; // driven manually by owning rope system
}

void URopeRenderComponent::BeginPlay()
{
	Super::BeginPlay();

	// Create ISMC for rope visual
	RopeMeshComponent = NewObject<UInstancedStaticMeshComponent>(GetOwner(), UInstancedStaticMeshComponent::StaticClass(), TEXT("RopeMeshISMC"));
	if (RopeMeshComponent)
	{
		RopeMeshComponent->RegisterComponent();
		RopeMeshComponent->AttachToComponent(GetOwner()->GetRootComponent(), FAttachmentTransformRules::KeepRelativeTransform);
		RopeMeshComponent->SetUsingAbsoluteLocation(true); // Fix: Render in World Space
		RopeMeshComponent->SetUsingAbsoluteRotation(true);
		RopeMeshComponent->SetUsingAbsoluteScale(true);
		RopeMeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		RopeMeshComponent->SetCastShadow(false);
	}
}

void URopeRenderComponent::RefreshFromBendPoints(const TArray<FVector>& InBendPoints)
{
	BendPoints = InBendPoints;
	BuildVerletPoints();
}

void URopeRenderComponent::Simulate(float DeltaTime)
{
	if (VerletPoints.Num() == 0)
	{
		RenderPoints.Reset();
		if (RopeMeshComponent) RopeMeshComponent->ClearInstances();
		return;
	}

	const FVector Gravity = FVector(0.f, 0.f, GetWorld() ? GetWorld()->GetGravityZ() * GravityScale : -980.f * GravityScale);

	for (FVerletPoint& Point : VerletPoints)
	{
		const FVector Velocity = (Point.Position - Point.LastPosition) * Damping;
		const FVector NewPosition = Point.Position + Velocity + Gravity * DeltaTime * DeltaTime;
		Point.LastPosition = Point.Position;
		Point.Position = NewPosition;
	}

	for (int32 Iter = 0; Iter < SolverIterations; ++Iter)
	{
		SatisfyConstraints();
	}

	RenderPoints.SetNum(VerletPoints.Num());
	for (int32 Index = 0; Index < VerletPoints.Num(); ++Index)
	{
		RenderPoints[Index] = VerletPoints[Index].Position;
	}

	// Update visual mesh instances
	UpdateRopeVisual();
}

void URopeRenderComponent::UpdateRopeVisual()
{
	if (!RopeMeshComponent || !RopeMesh || RenderPoints.Num() < 2)
	{
		if (RopeMeshComponent) RopeMeshComponent->ClearInstances();
		return;
	}

	// Set mesh if not already set
	if (RopeMeshComponent->GetStaticMesh() != RopeMesh)
	{
		RopeMeshComponent->SetStaticMesh(RopeMesh);
	}

	RopeMeshComponent->ClearInstances();

	// Create cylinder instance between each pair of points
	for (int32 i = 0; i < RenderPoints.Num() - 1; ++i)
	{
		const FVector Start = RenderPoints[i];
		const FVector End = RenderPoints[i + 1];
		const FVector Mid = (Start + End) * 0.5f;
		const float Length = FVector::Distance(Start, End);

		if (Length < 0.1f) continue; // Skip very short segments

		const FVector Dir = (End - Start).GetSafeNormal();
		const FRotator Rot = Dir.Rotation();

		FTransform InstanceTransform;
		InstanceTransform.SetLocation(Mid);
		InstanceTransform.SetRotation(Rot.Quaternion());
		InstanceTransform.SetScale3D(FVector(RopeThickness, RopeThickness, Length / 100.f)); // Assuming mesh is 100 units tall

		RopeMeshComponent->AddInstance(InstanceTransform);
	}
}



void URopeRenderComponent::BuildVerletPoints()
{
        VerletPoints.Reset();
        RestLengths.Reset();
        AnchorIndices.Reset();

        if (BendPoints.Num() < 2)
        {
                RenderPoints.Reset();
                return;
        }

        // Add the very first anchor.
        VerletPoints.Add({BendPoints[0], BendPoints[0]});
        AnchorIndices.Add(0);

        for (int32 SegmentIndex = 0; SegmentIndex < BendPoints.Num() - 1; ++SegmentIndex)
        {
                const FVector& Start = BendPoints[SegmentIndex];
                const FVector& End = BendPoints[SegmentIndex + 1];

                // Subdivide the interior of the segment (excluding endpoints which are anchors).
                const int32 Subdivisions = FMath::Max(SubdivisionsPerSegment, 1);
                for (int32 SubIdx = 1; SubIdx <= Subdivisions; ++SubIdx)
                {
                        const float Alpha = static_cast<float>(SubIdx) / static_cast<float>(Subdivisions + 1);
                        const FVector Point = FMath::Lerp(Start, End, Alpha);
                        VerletPoints.Add({Point, Point});
                }

                // Add the end point of this segment (which is also the start of the next one) and mark it as an anchor.
                const int32 EndIndex = VerletPoints.Add({End, End});
                AnchorIndices.Add(EndIndex);
        }

        // Compute rest lengths between all consecutive verlet points.
        RestLengths.SetNum(FMath::Max(VerletPoints.Num() - 1, 0));
        for (int32 Index = 0; Index < RestLengths.Num(); ++Index)
        {
                RestLengths[Index] = FVector::Distance(VerletPoints[Index].Position, VerletPoints[Index + 1].Position);
        }

        RenderPoints.SetNum(VerletPoints.Num());
        for (int32 Index = 0; Index < VerletPoints.Num(); ++Index)
        {
                RenderPoints[Index] = VerletPoints[Index].Position;
        }
}

void URopeRenderComponent::SatisfyConstraints()
{
        if (VerletPoints.Num() < 2)
        {
                return;
        }

        // Anchor all bend points to their authoritative positions.
        for (int32 AnchorIdx = 0; AnchorIdx < AnchorIndices.Num(); ++AnchorIdx)
        {
                const int32 VerletIndex = AnchorIndices[AnchorIdx];
                if (VerletPoints.IsValidIndex(VerletIndex) && BendPoints.IsValidIndex(AnchorIdx))
                {
                        VerletPoints[VerletIndex].Position = BendPoints[AnchorIdx];
                }
        }

        // Enforce distance constraint between consecutive points using precomputed rest lengths.
        for (int32 Index = 0; Index < RestLengths.Num(); ++Index)
        {
                FVerletPoint& A = VerletPoints[Index];
                FVerletPoint& B = VerletPoints[Index + 1];

                const float Desired = RestLengths[Index];
                FVector Delta = B.Position - A.Position;
                float Distance = Delta.Size();

                if (Distance < KINDA_SMALL_NUMBER)
                {
                        continue;
                }

                const float Diff = (Distance - Desired) / Distance;
                const FVector Offset = Delta * 0.5f * Diff;

                // Do not move anchors; only adjust free points.
                if (!AnchorIndices.Contains(Index))
                {
                        A.Position += Offset;
                }
                if (!AnchorIndices.Contains(Index + 1))
                {
                        B.Position -= Offset;
                }
        }
}
