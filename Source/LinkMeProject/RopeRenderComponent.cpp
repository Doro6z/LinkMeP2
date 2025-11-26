// RopeRenderComponent.cpp

#include "RopeRenderComponent.h"
#include "GameFramework/Actor.h"

URopeRenderComponent::URopeRenderComponent()
{
PrimaryComponentTick.bCanEverTick = false; // driven manually by owning rope system
}

void URopeRenderComponent::BeginPlay()
{
Super::BeginPlay();
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
}

void URopeRenderComponent::BuildVerletPoints()
{
VerletPoints.Reset();

if (BendPoints.Num() < 2)
{
RenderPoints.Reset();
return;
}

for (int32 SegmentIndex = 0; SegmentIndex < BendPoints.Num() - 1; ++SegmentIndex)
{
const FVector& Start = BendPoints[SegmentIndex];
const FVector& End = BendPoints[SegmentIndex + 1];
VerletPoints.Add({Start, Start});

const int32 Subdivisions = FMath::Max(SubdivisionsPerSegment, 1);
for (int32 SubIdx = 1; SubIdx <= Subdivisions; ++SubIdx)
{
const float Alpha = static_cast<float>(SubIdx) / static_cast<float>(Subdivisions + 1);
const FVector Point = FMath::Lerp(Start, End, Alpha);
VerletPoints.Add({Point, Point});
}
}

// Add final end point (player side)
VerletPoints.Add({BendPoints.Last(), BendPoints.Last()});

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

// Anchor first and last to original bend endpoints for stability
if (BendPoints.Num() >= 1)
{
VerletPoints[0].Position = BendPoints[0];
}
if (BendPoints.Num() >= 2)
{
VerletPoints.Last().Position = BendPoints.Last();
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
