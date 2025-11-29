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
	AnchorIndices.Reset();

	if (BendPoints.Num() < 2)
	{
		RenderPoints.Reset();
		return;
	}

	// Add the first anchor point (Start of the rope)
	VerletPoints.Add({BendPoints[0], BendPoints[0]});
	AnchorIndices.Add(0);

	for (int32 SegmentIndex = 0; SegmentIndex < BendPoints.Num() - 1; ++SegmentIndex)
	{
		const FVector& Start = BendPoints[SegmentIndex];
		const FVector& End = BendPoints[SegmentIndex + 1];

		const int32 Subdivisions = FMath::Max(SubdivisionsPerSegment, 1);
		for (int32 SubIdx = 1; SubIdx <= Subdivisions; ++SubIdx)
		{
			const float Alpha = static_cast<float>(SubIdx) / static_cast<float>(Subdivisions + 1);
			const FVector Point = FMath::Lerp(Start, End, Alpha);
			VerletPoints.Add({Point, Point});
		}

		// Add the end point of this segment (which is the start of the next, or the player)
		VerletPoints.Add({End, End});
		AnchorIndices.Add(VerletPoints.Num() - 1);
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

	// Pin all Anchor Points to their gameplay positions
	// AnchorIndices[i] corresponds to BendPoints[i]
	const int32 NumAnchors = FMath::Min(AnchorIndices.Num(), BendPoints.Num());
	
	for (int32 i = 0; i < NumAnchors; ++i)
	{
		const int32 VerletIdx = AnchorIndices[i];
		if (VerletPoints.IsValidIndex(VerletIdx))
		{
			VerletPoints[VerletIdx].Position = BendPoints[i];
		}
	}

// Enforce distance constraint between consecutive points
for (int32 Index = 0; Index < VerletPoints.Num() - 1; ++Index)
{
FVerletPoint& A = VerletPoints[Index];
FVerletPoint& B = VerletPoints[Index + 1];

const float Desired = FVector::Distance(A.Position, B.Position);
const FVector Dir = (B.Position - A.Position).GetSafeNormal();

const FVector Mid = (A.Position + B.Position) * 0.5f;
const FVector Offset = Dir * (Desired * 0.5f);

A.Position = Mid - Offset;
B.Position = Mid + Offset;
}
}
