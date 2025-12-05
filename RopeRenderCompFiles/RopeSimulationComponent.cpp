// RopeSimulationComponent.cpp

#include "RopeSimulationComponent.h"
#include "RopeActor.h"
#include "Engine/World.h"
#include "DrawDebugHelpers.h"

URopeSimulationComponent::URopeSimulationComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void URopeSimulationComponent::BeginPlay()
{
	Super::BeginPlay();
}

void URopeSimulationComponent::InitializeSimulation(ARopeActor* InOwner)
{
	RopeOwner = InOwner;
}

void URopeSimulationComponent::Simulate(float DeltaTime)
{
	if (!RopeOwner)
		return;

	if (!RopeOwner->bInitialized || RopeOwner->Positions.Num() < 2)
		return;

	if (DeltaTime < KINDA_SMALL_NUMBER)
		return;

	// Safety clamp on physics delta time
	const float MaxDeltaTime = 0.05f;
	const float SafeDeltaTime = FMath::Min(DeltaTime, MaxDeltaTime);

	IntegrateVerlet(SafeDeltaTime);
	SolveDistanceConstraints(SafeDeltaTime);

	if (RopeOwner->Params.bEnableCollision)
	{
		SolveCollision();
	}
}

void URopeSimulationComponent::IntegrateVerlet(float DeltaTime)
{
	TArray<FVector>& Positions = RopeOwner->Positions;
	TArray<FVector>& LastFramePositions = RopeOwner->LastFramePositions;

	if (Positions.Num() < 2)
		return;

	const FVector Gravity = FVector(0.f, 0.f, -980.f) * RopeOwner->Params.GravityScale;
	const float Damping = 1.f - FMath::Clamp(RopeOwner->Params.Damping, 0.f, 1.f);

	const int32 Substeps = FMath::Clamp(RopeOwner->Params.Substep, 1, 10);
	const float dt = DeltaTime / (float)Substeps;
	const float dt2 = dt * dt;

	const int32 LastIndex = Positions.Num() - 1;

	// LooseEnd logic:
	// Start = ALWAYS anchored -> never integrated
	// End = anchored ONLY if not loose
	int32 FirstDynamic = 1;  // start always static
	int32 LastDynamic  = RopeOwner->End.bLooseEnd ? LastIndex : LastIndex - 1;

	for (int32 Sub = 0; Sub < Substeps; ++Sub)
	{
		for (int32 i = FirstDynamic; i <= LastDynamic; ++i)
		{
			const FVector Velocity = (Positions[i] - LastFramePositions[i]) * Damping;
			const FVector NewPos = Positions[i] + Velocity + Gravity * dt2;

			LastFramePositions[i] = Positions[i];
			Positions[i] = NewPos;
		}
	}
}

void URopeSimulationComponent::SolveDistanceConstraints(float DeltaTime)
{
	TArray<FVector>& Positions = RopeOwner->Positions;

	if (Positions.Num() < 2)
		return;

	const int32 SegmentCount = FMath::Max(1, RopeOwner->Params.SegmentCount);
	const float BaseLength = RopeOwner->RestLength / (float)SegmentCount;

	const float Elasticity = FMath::Clamp(RopeOwner->Params.Elasticity, 0.f, 1.f);
	const float Stiffness = 1.f - Elasticity;

	const int32 IterCount = FMath::Clamp(RopeOwner->Params.ConstraintIterations, 1, 20);
	const int32 LastIndex = Positions.Num() - 1;

	const bool bEndPinned = !RopeOwner->End.bLooseEnd;

	for (int32 Iter = 0; Iter < IterCount; ++Iter)
	{
		for (int32 i = 0; i < LastIndex; ++i)
		{
			FVector& P0 = Positions[i];
			FVector& P1 = Positions[i + 1];

			FVector Delta = P1 - P0;
			const float Dist = Delta.Size();

			if (Dist < 0.0001f)
				continue;

			FVector Dir = Delta / Dist;

			float TargetLength = BaseLength;

			if (Elasticity > KINDA_SMALL_NUMBER)
			{
				float StretchFactor = 1.f + Elasticity * RopeOwner->Params.MaxStretchRatio;
				TargetLength = FMath::Clamp(Dist, BaseLength, BaseLength * StretchFactor);
			}

			const float Error = Dist - TargetLength;
			const FVector Correction = Dir * (Error * 0.5f * FMath::Lerp(0.25f, 0.75f, Stiffness));

			const bool bFirst = (i == 0);
			const bool bLast  = (i + 1 == LastIndex);

			// Start anchor ALWAYS pinned
			if (!bFirst)
				P0 += Correction;

			// End anchor pinned ONLY if not LooseEnd
			if (!bLast || bEndPinned)
				P1 -= Correction;
		}

		// Reapply anchors each iteration
		Positions[0] = RopeOwner->Start.Location;

		if (bEndPinned)
			Positions.Last() = RopeOwner->End.Location;
	}
}

void URopeSimulationComponent::SolveCollision()
{
	UWorld* World = GetWorld();
	if (!World || !RopeOwner)
		return;

	TArray<FVector>& Positions = RopeOwner->Positions;
	if (Positions.Num() < 2)
		return;

	FCollisionQueryParams Query;
	Query.AddIgnoredActor(RopeOwner);

	const float Radius = RopeOwner->Params.CollisionRadius;
	const auto Channel = RopeOwner->Params.CollisionChannel;
	const int32 IterCount = RopeOwner->Params.CollisionIterations;
	const bool bEndPinned = !RopeOwner->End.bLooseEnd;

	for (int32 Iter = 0; Iter < IterCount; ++Iter)
	{
		for (int32 i = 0; i < Positions.Num() - 1; ++i)
		{
			FVector& P0 = Positions[i];
			FVector& P1 = Positions[i + 1];

			FHitResult Hit;
			if (World->SweepSingleByChannel(
				Hit,
				P0,
				P1,
				FQuat::Identity,
				Channel,
				FCollisionShape::MakeSphere(Radius),
				Query))
			{
				FVector Safe = Hit.ImpactPoint + Hit.ImpactNormal * Radius;

				const bool bIsFirst = (i == 0);
				const bool bIsLast  = (i + 1 == Positions.Num() - 1);

				if (!bIsFirst && !bIsLast)
				{
					FVector Half = (Safe - P1) * 0.5f;
					P1 += Half;
					P0 -= Half;
				}
				else if (bIsLast && !bEndPinned)
				{
					P1 = Safe;
				}
			}
		}

		// Reapply anchors
		Positions[0] = RopeOwner->Start.Location;

		if (bEndPinned)
			Positions.Last() = RopeOwner->End.Location;
	}
}
