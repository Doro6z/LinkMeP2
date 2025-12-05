// RopeRenderComponent.cpp

#include "RopeRenderComponent.h"
#include "Components/SplineMeshComponent.h"
#include "Components/SplineComponent.h"

URopeRenderComponent::URopeRenderComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	bWantsInitializeComponent = true;
}

void URopeRenderComponent::BeginPlay()
{
	Super::BeginPlay();

	// Create internal spline
	RopeSpline = NewObject<USplineComponent>(this, TEXT("RopeSpline"));
	RopeSpline->RegisterComponent();
	RopeSpline->AttachToComponent(this, FAttachmentTransformRules::KeepRelativeTransform);
	RopeSpline->SetClosedLoop(false);
}

void URopeRenderComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// Simulation happens here or in UpdateVisualSegments?
	// Ticking here ensures simulation runs even if UpdateVisualSegments isn't called,
	// but we need the latest BendPoints. 
	// Usually UpdateVisualSegments is called by owner's tick.
	// We'll leave simulation to UpdateVisualSegments to ensure sync.
}

void URopeRenderComponent::UpdateVisualSegments(const TArray<FVector>& BendPoints, const FVector& EndPosition)
{
	if (BendPoints.Num() < 1) 
	{
		HideUnusedSegments(0);
		Particles.Reset(); // Clear if no rope
		return;
	}

	TargetBendPoints = BendPoints;

	// Total length of the gameplay rope
	float TotalGameplayLength = 0.f;
	for (int32 i = 0; i < BendPoints.Num() - 1; i++)
	{
		TotalGameplayLength += FVector::Dist(BendPoints[i], BendPoints[i+1]);
	}
	TotalGameplayLength += FVector::Dist(BendPoints.Last(), EndPosition);

	// Initialize if needed
	if (Particles.Num() == 0)
	{
		InitializeRope(BendPoints[0], EndPosition);
	}
	else
	{
		// Hard-pin the ends
		if (Particles.Num() > 0)
		{
			Particles[0].Position = BendPoints[0];
			Particles[0].bFree = false;

			Particles.Last().Position = EndPosition;
			Particles.Last().bFree = false;
		}

		// Handle Resizing (Simple approach: Resample if length diff is huge)
		// We want to maintain a target segment length.
		int32 IdealCount = FMath::Max(2, FMath::CeilToInt(TotalGameplayLength / SegmentLength));
		
		// If the count is significantly off (>20%), resample to avoid massive stretching/bunching
		if (FMath::Abs(Particles.Num() - IdealCount) > (IdealCount * 0.2f))
		{
			// Resample (crude but effective for rapid length changes)
			InitializeRope(BendPoints[0], EndPosition);
		}
	}

	// Simulation Step
	float DeltaTime = GetWorld() ? GetWorld()->GetDeltaSeconds() : 0.016f;
	
	SimulateParticles(DeltaTime);
	SolveDistanceConstraints();
	SolveSoftConstraints();

	// Render
	UpdateSplineRepresentation();
	UpdateSplineMeshes();
}

void URopeRenderComponent::InitializeRope(const FVector& Start, const FVector& End)
{
	float Dist = FVector::Dist(Start, End);
	int32 NumSegments = FMath::Max(1, FMath::CeilToInt(Dist / SegmentLength));
	
	Particles.Reset(NumSegments + 1);

	for (int32 i = 0; i <= NumSegments; i++)
	{
		float Alpha = (float)i / (float)NumSegments;
		FVector Pos = FMath::Lerp(Start, End, Alpha);

		FRopeParticle P;
		P.Position = Pos;
		P.PreviousPosition = Pos;
		P.bFree = (i != 0 && i != NumSegments); // Pin ends
		Particles.Add(P);
	}
}

void URopeRenderComponent::SimulateParticles(float DeltaTime)
{
	for (FRopeParticle& P : Particles)
	{
		if (!P.bFree) continue;

		// Verlet Integration
		FVector Velocity = (P.Position - P.PreviousPosition) * Damping; // Damping is "Friction"
		FVector NewPos = P.Position + Velocity + (Gravity * DeltaTime * DeltaTime);

		P.PreviousPosition = P.Position;
		P.Position = NewPos;
	}
}

void URopeRenderComponent::SolveDistanceConstraints()
{
	if (Particles.Num() < 2) return;

	// Calculate target length for each segment based on current total length constraint
	// We want the rope to be able to stretch slightly or slack, but generally follow SegmentLength.
	// Actually, if we want the rope to LOOK like it has the length of the gameplay rope,
	// we should constrain the TOTAL length.
	
	// Better: Average the segment length based on current particle count
	// to make the rope fit the gameplay length exactly.
	float TotalLen = 0.f; // Current dist between ends
	// Or we use the configured SegmentLength? 
	// If we use fixed SegmentLength, the rope will sag if points are close.
	// This is desirable!
	
	float TargetDist = SegmentLength;

	for (int32 Iter = 0; Iter < SolverIterations; Iter++)
	{
		for (int32 i = 0; i < Particles.Num() - 1; i++)
		{
			FRopeParticle& P1 = Particles[i];
			FRopeParticle& P2 = Particles[i+1];

			FVector Delta = P2.Position - P1.Position;
			float CurrentDist = Delta.Size();
			
			if (CurrentDist > KINDA_SMALL_NUMBER)
			{
				float Error = (CurrentDist - TargetDist) / CurrentDist;
				
				// Apply correction
				FVector Correction = Delta * 0.5f * Error;

				if (P1.bFree && P2.bFree)
				{
					P1.Position += Correction;
					P2.Position -= Correction;
				}
				else if (P1.bFree)
				{
					P1.Position += Correction * 2.f;
				}
				else if (P2.bFree)
				{
					P2.Position -= Correction * 2.f;
				}
			}
		}
	}
}

void URopeRenderComponent::SolveSoftConstraints()
{
	if (TargetBendPoints.Num() < 1) return;

	// "Soft Pins": Create an attraction force from particles to the Gameplay Polyline.
	// This prevents the rope from clipping through the corners that the BendPoints represent.
	
	// A simpler, more robust method for "wrapping" is:
	// If a particle is "near" a bendpoint segment, confine it?
	
	// User Request: "Attire point visual le plus proche... vers bendpoint"
	// (Attract closest visual point to bendpoint).
	
	// Algorithm:
	// For each BendPoint (except first/last), find the closest Particle.
	// Pull that particle towards the BendPoint.
	
	// Note: First and Last BendPoints are already hard-pinned (Start/End of array).
	
	for (int32 i = 1; i < TargetBendPoints.Num() - 1; i++) // Skip first/last (Anchors)
	{ // Wait, last BendPoint is Player.
		// BendPoints array: [0]=Anchor, [1]=Corner, [2]=Player
		// We skipped 0. Last is Num-1. So we stop before Last.
		
		if (i >= TargetBendPoints.Num()) break;
		
		FVector Target = TargetBendPoints[i];
		
		// Find closest particle
		float BestDistSq = MAX_FLT;
		int32 BestIdx = -1;

		for (int32 p = 0; p < Particles.Num(); p++)
		{
			float DistSq = FVector::DistSquared(Particles[p].Position, Target);
			if (DistSq < BestDistSq)
			{
				BestDistSq = DistSq;
				BestIdx = p;
			}
		}

		if (BestIdx != -1 && Particles[BestIdx].bFree)
		{
			// Soft Pull
			Particles[BestIdx].Position = FMath::Lerp(Particles[BestIdx].Position, Target, SoftConstraintStrength);
		}
	}
}

void URopeRenderComponent::UpdateSplineRepresentation()
{
	if (!RopeSpline) return;

	RopeSpline->ClearSplinePoints(false);
	
	for (int32 i = 0; i < Particles.Num(); i++)
	{
		RopeSpline->AddSplinePoint(Particles[i].Position, ESplineCoordinateSpace::World, false);
	}
	
	RopeSpline->UpdateSpline();
}

void URopeRenderComponent::UpdateSplineMeshes()
{
	if (!RopeSpline || RopeSpline->GetNumberOfSplinePoints() < 2) 
	{
		HideUnusedSegments(0);
		return;
	}

	int32 NumSegments = RopeSpline->GetNumberOfSplinePoints() - 1;

	for (int32 i = 0; i < NumSegments; i++)
	{
		USplineMeshComponent* Segment = GetPooledSegment(i);
		if (Segment)
		{
			Segment->SetVisibility(true);

			FVector StartPos, StartTan, EndPos, EndTan;
			RopeSpline->GetLocationAndTangentAtSplinePoint(i, StartPos, StartTan, ESplineCoordinateSpace::World);
			RopeSpline->GetLocationAndTangentAtSplinePoint(i + 1, EndPos, EndTan, ESplineCoordinateSpace::World);

			Segment->SetStartAndEnd(StartPos, StartTan, EndPos, EndTan, true);
		}
	}

	HideUnusedSegments(NumSegments);
}

USplineMeshComponent* URopeRenderComponent::GetPooledSegment(int32 Index)
{
	if (Index >= SegmentPool.Num())
	{
		USplineMeshComponent* NewComp = NewObject<USplineMeshComponent>(this);
		if (NewComp)
		{
			NewComp->SetStaticMesh(RopeMesh);
			NewComp->SetMaterial(0, RopeMaterial);
			NewComp->SetMobility(EComponentMobility::Movable);
			
			// Use absolute coordinates for exact spline matching
			NewComp->SetUsingAbsoluteLocation(true);
			NewComp->SetUsingAbsoluteRotation(true);
			NewComp->SetUsingAbsoluteScale(true); 

			NewComp->SetForwardAxis(ForwardAxis);
			
			float Scale = RopeThickness; 
			NewComp->SetStartScale(FVector2D(Scale, Scale));
			NewComp->SetEndScale(FVector2D(Scale, Scale));
			
			NewComp->RegisterComponent();
			NewComp->AttachToComponent(this, FAttachmentTransformRules::KeepWorldTransform);
			
			SegmentPool.Add(NewComp);
		}
	}

	if (SegmentPool.IsValidIndex(Index))
	{
		return SegmentPool[Index];
	}
	
	return nullptr;
}

void URopeRenderComponent::HideUnusedSegments(int32 ActiveCount)
{
	for (int32 i = ActiveCount; i < SegmentPool.Num(); i++)
	{
		if (SegmentPool[i])
		{
			SegmentPool[i]->SetVisibility(false);
		}
	}
}
