// RopeRenderComponent.cpp

#include "RopeRenderComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/Engine.h"

URopeRenderComponent::URopeRenderComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
    // Tick after physics (PrePhysics is default, maybe PostPhysics is better for follow?)
    // Actually, we want to follow the Character who moves in Physics, so PostPhysics or standard Tick is fine.
}

void URopeRenderComponent::BeginPlay()
{
	Super::BeginPlay();

    RopeSpline = NewObject<USplineComponent>(this, TEXT("InternalSpline"));
    RopeSpline->RegisterComponent();
    RopeSpline->SetClosedLoop(false);

    // Initialize Fixed Pool
    Particles.SetNum(MaxParticles);
    for (FRopeParticle& P : Particles)
    {
        P = FRopeParticle(); // Reset
        P.bIsActive = false;
    }
    ActiveParticleCount = 0;
}

// ... TickComponent ...

// ... ResetRope ...

// ... DrawDebugInfo ...

// ... UpdateVisualSegments ...

void URopeRenderComponent::InitializeParticles(const FVector& Start, const FVector& End)
{
    // Sanity check pool
    if (Particles.Num() != MaxParticles) Particles.SetNum(MaxParticles);

    // ActiveParticleCount must be set by caller (RebuildConstraints)
    ActiveParticleCount = FMath::Clamp(ActiveParticleCount, 2, MaxParticles);
    
    // Distribute particles linearly
    for (int32 i = 0; i < ActiveParticleCount; i++)
    {
        float Alpha = (float)i / (float)(ActiveParticleCount - 1);
        FVector Pos = FMath::Lerp(Start, End, Alpha);
        
        FRopeParticle& P = Particles[i];
        P.Position = Pos;
        P.PreviousPosition = Pos;
        P.PredictedPosition = Pos;
        P.Velocity = FVector::ZeroVector;
        P.InverseMass = 1.0f; // All free by default
        P.bIsActive = true;
    }
    
    // Deactivate remainder
    for (int32 i = ActiveParticleCount; i < MaxParticles; i++)
    {
        Particles[i].bIsActive = false;
        Particles[i].Position = End; // hide at end
    }

    // Build Distance Constraints (Chain)
    DistanceConstraints.Reset();
    float TotalDist = FVector::Dist(Start, End);
    float SegmentLen = TotalDist / (float)(ActiveParticleCount - 1);

    for (int32 i = 0; i < ActiveParticleCount - 1; i++)
    {
        FDistanceConstraint C;
        C.IndexA = i;
        C.IndexB = i + 1;
        C.RestLength = SegmentLen; 
        C.Compliance = StretchCompliance;  
        DistanceConstraints.Add(C);
    }
}

void URopeRenderComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    if (bInitialized && Particles.Num() > 0)
    {
        SimulateXPBD(DeltaTime);
        UpdateSplineInterpolation();
        ApplyCornerRounding(); // Apply corner rounding after spline interpolation
        UpdateMeshes();

        // Debug Visualization
        if (bShowDebugSpline && GetWorld())
        {
            DrawDebugInfo();

            // Distinct Colors for Net Role
            FColor ParticleColor = FColor::Cyan;
            FColor SplineColor = FColor::Yellow;
            if (GetOwner() && GetOwner()->HasAuthority())
            {
                ParticleColor = FColor::Purple; // Server
                SplineColor = FColor::Orange;
            }

            // Draw Particles
            for (int32 i = 0; i < ActiveParticleCount; i++)
            {
                DrawDebugSphere(GetWorld(), Particles[i].Position, 8.0f, 8, ParticleColor, false, -1.f, 0, 1.0f);
            }

            // Draw Spline Curve
            if (RopeSpline && RopeSpline->GetNumberOfSplinePoints() > 1)
            {
                int32 NumSegments = 20; // Sample points per spline segment
                for (int32 i = 0; i < RopeSpline->GetNumberOfSplinePoints() - 1; i++)
                {
                    for (int32 j = 0; j < NumSegments; j++)
                    {
                        float SegmentStart = i + (float)j / (float)NumSegments;
                        float SegmentEnd = i + (float)(j + 1) / (float)NumSegments;
                        
                        FVector Start = RopeSpline->GetLocationAtSplineInputKey(SegmentStart, ESplineCoordinateSpace::World);
                        FVector End = RopeSpline->GetLocationAtSplineInputKey(SegmentEnd, ESplineCoordinateSpace::World);
                        
                        DrawDebugLine(GetWorld(), Start, End, SplineColor, false, -1.f, 0, 2.0f);
                    }
                }
            }
        }
    }
}

void URopeRenderComponent::ResetRope()
{
    // Clear Pool
    for (FRopeParticle& P : Particles) P.bIsActive = false;
    ActiveParticleCount = 0;
    ParticleCount = 0;
    bInitialized = false;
    
    if (RopeSpline)
    {
        RopeSpline->ClearSplinePoints(true);
    }
    
    HideUnusedSegments(0);
}

void URopeRenderComponent::SetRopeHidden(bool bHidden)
{
    bRopeHidden = bHidden;
    SetVisibility(!bHidden, true); // Hide this component and children

    // Force update meshes to respect the new flag
    if (bHidden)
    {
        HideUnusedSegments(0);
    }
}

void URopeRenderComponent::DrawDebugInfo()
{
    if (!GEngine) return;
    
    // Use UniqueID as base key to allow multiple components to display without overwriting
    uint64 KeyBase = GetUniqueID(); 

    FColor TextColor = bInitialized ? FColor::Green : FColor::Red;
    
    FString NetRole = TEXT("Unknown");
    if (GetOwner())
    {
        if (GetOwner()->HasAuthority()) NetRole = TEXT("Server");
        else NetRole = TEXT("Client");
        
        // Detailed check
        ENetMode NM = GetNetMode();
        if (NM == NM_DedicatedServer) NetRole = TEXT("Ded.Server");
        else if (NM == NM_ListenServer) NetRole = TEXT("ListenServer");
        else if (NM == NM_Client) NetRole = TEXT("Client");
        else if (NM == NM_Standalone) NetRole = TEXT("Standalone");
    }

    GEngine->AddOnScreenDebugMessage(KeyBase + 1, 0.0f, TextColor, 
        FString::Printf(TEXT("[%s][%s] RopeRender: %s"), *NetRole, *GetOwner()->GetName(), bInitialized ? TEXT("Active") : TEXT("Inactive")));

    GEngine->AddOnScreenDebugMessage(KeyBase + 2, 0.0f, FColor::White, 
        FString::Printf(TEXT("  Particles: %d / %d"), ActiveParticleCount, MaxParticles));
        
    GEngine->AddOnScreenDebugMessage(KeyBase + 3, 0.0f, FColor::White, 
        FString::Printf(TEXT("  Mesh Length Base: %.1f | Stretch: %.2f"), MeshLengthBase, MaxMeshStretch));

    float CurrentLength = 0.0f;
    for(int32 i=0; i<ActiveParticleCount - 1; i++) CurrentLength += FVector::Dist(Particles[i].Position, Particles[i+1].Position);
    
    GEngine->AddOnScreenDebugMessage(KeyBase + 4, 0.0f, FColor::White, 
        FString::Printf(TEXT("  Sim Length: %.2f m"), CurrentLength / 100.0f));
}

void URopeRenderComponent::UpdateVisualSegments(const TArray<FVector>& BendPoints, const FVector& EndPosition)
{
    if (BendPoints.Num() == 0) return;

    // If first time, initialize particles in a straight line
    if (!bInitialized)
    {
        InitializeParticles(BendPoints[0], EndPosition);
        bInitialized = true;
    }

    // Update Constraints (Virtual Segmentation)
    RebuildConstraints(BendPoints, EndPosition);

    if (BendPoints.Num() > 0 && bShowDebugSpline)
    {
         UE_LOG(LogTemp, Log, TEXT("[RopeRender] Updated Segments: %d BendPoints. Start=%s, End=%s"), 
            BendPoints.Num(), *BendPoints[0].ToString(), *EndPosition.ToString());
    }

    // If hidden, force clear
    if (bRopeHidden)
    {
        HideUnusedSegments(0);
        return;
    }
}

// Duplicate InitializeParticles DELETED


void URopeRenderComponent::RebuildConstraints(const TArray<FVector>& BendPoints, const FVector& EndPosition)
{
    PinConstraints.Reset();

    // --- 1. Calculate Total Visual Length ---
    float TotalLen = 0.f;
    TArray<float> SegmentLens;
    for (int32 i = 0; i < BendPoints.Num() - 1; i++)
    {
        float D = FVector::Dist(BendPoints[i], BendPoints[i+1]);
        SegmentLens.Add(D);
        TotalLen += D;
    }
    float LastSeg = FVector::Dist(BendPoints.Last(), EndPosition);
    SegmentLens.Add(LastSeg);
    TotalLen += LastSeg;

    // --- 2. Dynamic Particle Count (Pool Management) ---
    int32 TargetCount = 0;
    
    if (bUseDynamicParticleCount)
    {
        TargetCount = FMath::Max(5, FMath::CeilToInt(TotalLen / MeshLengthBase));
    }
    else
    {
        TargetCount = FMath::Max(3, FixedParticleCount);
    }
    TargetCount = FMath::Min(TargetCount, MaxParticles); // Hard cap

    // Handle Initialization
    if (ActiveParticleCount == 0)
    {
        ActiveParticleCount = TargetCount;
        InitializeParticles(BendPoints[0], EndPosition); 
        return; 
    }
    
    // Handle Pool Resize (No Interpolation of existing particles!)
    if (TargetCount > ActiveParticleCount)
    {
        // Grow: Activate new particles at the tail
        // Initialize them at the position of the last active particle to prevent "flying in"
        FVector SpawnPos = Particles[ActiveParticleCount-1].Position;
        
        for (int32 i = ActiveParticleCount; i < TargetCount; i++)
        {
            FRopeParticle& P = Particles[i];
            P.bIsActive = true;
            P.Position = SpawnPos;
            P.PreviousPosition = SpawnPos;
            P.PredictedPosition = SpawnPos;
            P.Velocity = Particles[ActiveParticleCount-1].Velocity; // Inherit velocity for smoothness?
            P.InverseMass = 1.0f;
        }
    }
    else if (TargetCount < ActiveParticleCount)
    {
        // Shrink: Deactivate tail
        for (int32 i = TargetCount; i < ActiveParticleCount; i++)
        {
            Particles[i].bIsActive = false;
        }
    }
    
    ActiveParticleCount = TargetCount;
    ParticleCount = ActiveParticleCount; // Sync legacy prop

    // --- 3. Recalculate RestLength for Distance Constraints ---
    if (TotalLen > KINDA_SMALL_NUMBER && ActiveParticleCount > 1)
    {
        DistanceConstraints.Reset();
        float NewRestLen = TotalLen / (float)(ActiveParticleCount - 1);
        
        for (int32 i = 0; i < ActiveParticleCount - 1; i++)
        {
            FDistanceConstraint C;
            C.IndexA = i;
            C.IndexB = i + 1;
            C.RestLength = NewRestLen;
            C.Compliance = StretchCompliance; 
            DistanceConstraints.Add(C);
        }
    }

    // --- 4. Reset all InverseMass to free (non-pinned) ---
    for (FRopeParticle& P : Particles)
    {
        P.InverseMass = 1.0f;
    }

    // --- 5. Pin Start (Anchor) ---
    Particles[0].InverseMass = 0.0f; // Infinite mass
    FPinnedConstraint StartPin;
    StartPin.ParticleIndex = 0;
    StartPin.WorldLocation = BendPoints[0];
    StartPin.bActive = true;
    PinConstraints.Add(StartPin);

    // --- 6. Pin End (Player) ---
    Particles.Last().InverseMass = 0.0f;
    FPinnedConstraint EndPin;
    EndPin.ParticleIndex = ParticleCount - 1;
    EndPin.WorldLocation = EndPosition;
    EndPin.bActive = true;
    PinConstraints.Add(EndPin);

    // --- 7. Pin Corners (Virtual Segmentation) ---
    if (BendPoints.Num() > 1)
    {
        float AccumDist = 0.f;
        for (int32 i = 1; i < BendPoints.Num(); i++) // Skip 0 (already pinned)
        {
            AccumDist += SegmentLens[i - 1];
            float Alpha = AccumDist / TotalLen;
            int32 PinIdx = FMath::Clamp(FMath::RoundToInt(Alpha * (ParticleCount - 1)), 1, ParticleCount - 2);
            
            // If Compliance is 0, we use infinite mass (Hard Pin).
            // If Compliance > 0, we use dynamic mass (Soft Pin) to allow gravity to affect it.
            if (BendPointCompliance > KINDA_SMALL_NUMBER)
            {
                Particles[PinIdx].InverseMass = 1.0f; // Soft Pin
            }
            else
            {
                Particles[PinIdx].InverseMass = 0.0f; // Hard Pin
            }

            FPinnedConstraint Pin;
            Pin.ParticleIndex = PinIdx;
            Pin.WorldLocation = BendPoints[i];
            Pin.bActive = true;
            PinConstraints.Add(Pin);
        }
    }
}


void URopeRenderComponent::SimulateXPBD(float DeltaTime)
{
    if (DeltaTime <= KINDA_SMALL_NUMBER) return;

    float SubStepDt = DeltaTime / (float)SubSteps;

    for (int32 Step = 0; Step < SubSteps; Step++)
    {
        // 1. Predict
        for (FRopeParticle& P : Particles)
        {
            // Apply Gravity / External Forces
            P.Velocity += Gravity * SubStepDt;
            P.PredictedPosition = P.Position + P.Velocity * SubStepDt;
        }

        // 2. Solve Constraints
        SolveConstraints(SubStepDt);

        // 3. Update Integration
        for (FRopeParticle& P : Particles)
        {
            P.Velocity = (P.PredictedPosition - P.Position) / SubStepDt;
            P.Velocity *= Damping; // Apply damping to smooth motion
            P.PreviousPosition = P.Position;
            P.Position = P.PredictedPosition;

            // 4. Ground Collision (Simple Raycast/SphereTrace Projection)
            if (bEnableCollision)
            {
                FHitResult Hit;
                FCollisionQueryParams Params;
                Params.AddIgnoredActor(GetOwner()); // Ignore self

                FVector Start = P.PreviousPosition; // Trace from previous to catch fast movement
                FVector End = P.Position;
                
                // If movement is very small, force a small downward trace to prevent sinking
                if (FVector::DistSquared(Start, End) < 1.0f)
                {
                    End = Start + FVector(0, 0, -5.0f);
                }

                // Simple LineTrace or SphereTrace? SphereTrace is better for thickness.
                // Using a small radius for robustness.
                float TraceRadius = RopeThickness * 0.5f;
                
                bool bHit = GetWorld()->SweepSingleByChannel(
                    Hit, 
                    Start, 
                    End, 
                    FQuat::Identity, 
                    RopeCollisionChannel, 
                    FCollisionShape::MakeSphere(TraceRadius), 
                    Params
                );

                if (bHit)
                {
                    // Project out of collision
                    // Position = HitLocation + Normal * Radius
                    P.Position = Hit.Location + (Hit.ImpactNormal * TraceRadius);
                    P.PredictedPosition = P.Position;
                    
                    // Friction/Restitution? For now, just stop normal velocity
                    FVector VelocityNormal = P.Velocity.ProjectOnTo(Hit.ImpactNormal);
                    P.Velocity -= VelocityNormal; // Kill normal velocity (inelastic)
                }
                // Fallback: Hard floor check if world trace fails but z < GroundZ (optional, but WorldTrace is better)
            }
        }
    }
}

void URopeRenderComponent::SolveConstraints(float Dt)
{
    for (int32 Iter = 0; Iter < SolverIterations; Iter++)
    {
        // A. Apply Pins
        for (const FPinnedConstraint& Pin : PinConstraints)
        {
            if (Pin.bActive && Particles.IsValidIndex(Pin.ParticleIndex))
            {
                FVector TargetPos = Pin.WorldLocation;
                FRopeParticle& P = Particles[Pin.ParticleIndex];
                
                // Determine if this is a Hard or Soft pin
                // Hard = Infinite Mass (InverseMass = 0) OR Compliance = 0
                // Soft = Dynamic Mass (InverseMass > 0) AND Compliance > 0
                
                // Start/End are always Hard (Mass 0). Mid-points are Soft if BendPointCompliance > 0.
                
                if (P.InverseMass == 0.0f)
                {
                    // Hard Constraint / Kinematic
                    // Use PinStrength for smooth interpolation (Manual Smoothing)
                    // If PinStrength = 1.0, it's instant. 0.2 is smooth.
                    P.PredictedPosition = FMath::Lerp(P.PredictedPosition, TargetPos, PinStrength);
                }
                else
                {
                    // Soft Constraint (XPBD)
                    // Allows gravity to pull the particle down, creating a "sag".
                    // Correction = w / (w + alpha/dt^2) * (Target - Current)
                    
                    float Alpha = BendPointCompliance / (Dt * Dt);
                    float W = P.InverseMass;
                    float Factor = W / (W + Alpha);
                    
                    // Apply correction towards Target
                    // This pulls the particle towards the pin, but fights against Gravity (in Velocity/PredictedPos)
                    P.PredictedPosition += (TargetPos - P.PredictedPosition) * Factor;
                }
            }
        }

        // B. Solve Distance Constraints (Weighted by InverseMass)
        for (const FDistanceConstraint& C : DistanceConstraints)
        {
            FRopeParticle& P1 = Particles[C.IndexA];
            FRopeParticle& P2 = Particles[C.IndexB];

            FVector Delta = P1.PredictedPosition - P2.PredictedPosition;
            float Len = Delta.Size();
            if (Len < KINDA_SMALL_NUMBER) continue;

            float Error = Len - C.RestLength;
            FVector Dir = Delta / Len;

            // XPBD: Weighted correction based on InverseMass + Compliance
            float W1 = P1.InverseMass;
            float W2 = P2.InverseMass;
            float WSum = W1 + W2;
            
            // Calculate Compliance Alpha = Compliance / dt^2
            float Alpha = C.Compliance / (Dt * Dt);
            float Denom = WSum + Alpha;
            
            // If both particles are pinned (WSum ≈ 0) AND Rigid (Alpha = 0), skip correction
            // (If Elastic/Soft, Alpha > 0 so Denom > 0, we solve it).
            if (Denom < KINDA_SMALL_NUMBER) continue;

            // Apply correction proportional to mass ratio
            // DeltaLambda = (-C - alpha * lambda) / (w1 + w2 + alpha) ... simplified for position-based:
            float Correction = Error / Denom;
            
            P1.PredictedPosition -= Dir * Correction * W1;
            P2.PredictedPosition += Dir * Correction * W2;
        }
    }
}

void URopeRenderComponent::UpdateSplineInterpolation()
{
    if (!RopeSpline) return;

    RopeSpline->ClearSplinePoints(false);

    // Catmull-Rom Centripetal Logic via Unreal Spline?
    // Unreal's SplineComponent does generic cubic interpolation.
    // To get "Centripetal" specifically we might need manual curve calculation,
    // OR we just set points and let Unreal handle smoothing, checking for loops settings.
    
    // Standard FSplinePoint is simple.
    // If we want to prevent loops, we can use Linear tangency at corners?
    
    for (int32 i = 0; i < ActiveParticleCount; i++)
    {
        RopeSpline->AddSplinePoint(Particles[i].Position, ESplineCoordinateSpace::World, false);
        
        // Set point type to CurveCustomTangent or CurveClamped?
        // CurveClamped is good for preventing overshoots.
        RopeSpline->SetSplinePointType(i, ESplinePointType::CurveClamped, false);
    }

    RopeSpline->UpdateSpline();
}

void URopeRenderComponent::ApplyCornerRounding()
{
    if (!RopeSpline || !bEnableCornerRounding) return;
    
    int32 NumPoints = RopeSpline->GetNumberOfSplinePoints();
    if (NumPoints < 3) return; // Need at least 3 points to have corners
    
    // Store rounded points (we'll rebuild the spline with these)
    TArray<FVector> RoundedPoints;
    RoundedPoints.Reserve(NumPoints * (CornerSubdivisions + 2)); // Estimate
    
    // First point (anchor) - no rounding
    FVector FirstPoint = RopeSpline->GetLocationAtSplinePoint(0, ESplineCoordinateSpace::World);
    RoundedPoints.Add(FirstPoint);
    
    // Process middle points (corners that need rounding)
    for (int32 i = 1; i < NumPoints - 1; i++)
    {
        FVector Pi = RopeSpline->GetLocationAtSplinePoint(i, ESplineCoordinateSpace::World);
        FVector PrevPoint = RopeSpline->GetLocationAtSplinePoint(i - 1, ESplineCoordinateSpace::World);
        FVector NextPoint = RopeSpline->GetLocationAtSplinePoint(i + 1, ESplineCoordinateSpace::World);
        
        // Calculate direction vectors (normalized)
        FVector D_in = (PrevPoint - Pi).GetSafeNormal();
        FVector D_out = (NextPoint - Pi).GetSafeNormal();
        
        // Calculate tangent points at distance CornerRadius from corner
        float SegmentLenIn = FVector::Dist(PrevPoint, Pi);
        float SegmentLenOut = FVector::Dist(NextPoint, Pi);
        float R = FMath::Min(CornerRadius, FMath::Min(SegmentLenIn, SegmentLenOut) * 0.4f);
        
        FVector T1 = Pi + D_in * R;
        FVector T2 = Pi + D_out * R;
        
        // Add straight segment up to T1
        RoundedPoints.Add(T1);
        
        // Generate arc points between T1 and T2 using quadratic Bézier with Pi as control point
        for (int32 j = 1; j < CornerSubdivisions; j++)
        {
            float t = (float)j / (float)CornerSubdivisions;
            
            // Quadratic Bézier: B(t) = (1-t)^2 * T1 + 2*(1-t)*t * Pi + t^2 * T2
            float OneMinusT = 1.0f - t;
            FVector ArcPoint = (OneMinusT * OneMinusT) * T1 
                             + (2.0f * OneMinusT * t) * Pi 
                             + (t * t) * T2;
            
            RoundedPoints.Add(ArcPoint);
        }
    }
    
    // Last point (player) - no rounding
    FVector LastPoint = RopeSpline->GetLocationAtSplinePoint(NumPoints - 1, ESplineCoordinateSpace::World);
    RoundedPoints.Add(LastPoint);
    
    // Rebuild spline with rounded points
    RopeSpline->ClearSplinePoints(false);
    for (const FVector& Point : RoundedPoints)
    {
        RopeSpline->AddSplinePoint(Point, ESplineCoordinateSpace::World, false);
    }
    RopeSpline->UpdateSpline();
}

void URopeRenderComponent::UpdateMeshes()
{
    if (!RopeSpline) return;

    int32 NumSplinePoints = RopeSpline->GetNumberOfSplinePoints();
    if (NumSplinePoints < 2) 
    {
        HideUnusedSegments(0);
        return;
    }

    int32 MeshIndex = 0; // Global mesh index in pool

    // For each segment between spline points
    for (int32 i = 0; i < NumSplinePoints - 1; i++)
    {
        FVector StartPos = RopeSpline->GetLocationAtSplinePoint(i, ESplineCoordinateSpace::World);
        FVector EndPos = RopeSpline->GetLocationAtSplinePoint(i + 1, ESplineCoordinateSpace::World);
        float SegmentLength = FVector::Dist(StartPos, EndPos);
        
        // Calculate how many meshes needed for this segment
        // We want to ensure that NO MESH exceeds MeshLengthBase * MaxMeshStretch.
        // Formula: Count = Ceil(Length / (Base * MaxStretch))
        // However, we also want to stay close to Base if possible.
        // User request: "Max size = 10 units * maxstretch factor" (MeshLengthBase is now ~20)
        
        float MaxAllowedLength = MeshLengthBase * MaxMeshStretch;
        int32 MeshCountForSegment = FMath::Max(1, FMath::CeilToInt(SegmentLength / MaxAllowedLength));
        
        // If the segment is extremely short, we might crush it too much? 
        // MinMeshStretch handles the lower bound visuals (scale), but we just distribute points here.
        
        // Distribute meshes along this segment
        for (int32 j = 0; j < MeshCountForSegment; j++)
        {
            float AlphaStart = (float)j / (float)MeshCountForSegment;
            float AlphaEnd = (float)(j + 1) / (float)MeshCountForSegment;
            
            // Calculate input keys for this mesh
            float InputKeyStart = i + AlphaStart;
            float InputKeyEnd = i + AlphaEnd;
            
            // Get positions and tangents along the spline
            FVector MeshStartPos = RopeSpline->GetLocationAtSplineInputKey(InputKeyStart, ESplineCoordinateSpace::World);
            FVector MeshEndPos = RopeSpline->GetLocationAtSplineInputKey(InputKeyEnd, ESplineCoordinateSpace::World);
            FVector MeshStartTan = RopeSpline->GetTangentAtSplineInputKey(InputKeyStart, ESplineCoordinateSpace::World);
            FVector MeshEndTan = RopeSpline->GetTangentAtSplineInputKey(InputKeyEnd, ESplineCoordinateSpace::World);
            
            // Validate tangents (prevent zero-length)
            float MinTangentLength = 1.0f;
            if (MeshStartTan.SizeSquared() < MinTangentLength)
            {
                MeshStartTan = (MeshEndPos - MeshStartPos).GetSafeNormal() * MinTangentLength;
            }
            if (MeshEndTan.SizeSquared() < MinTangentLength)
            {
                MeshEndTan = (MeshEndPos - MeshStartPos).GetSafeNormal() * MinTangentLength;
            }
            
            // Get mesh from pool
            USplineMeshComponent* Mesh = GetPooledSegment(MeshIndex++);
            if (Mesh)
            {
                Mesh->SetVisibility(true);
                Mesh->SetStartAndEnd(MeshStartPos, MeshStartTan, MeshEndPos, MeshEndTan, true);
                
                // Debug: Visualize tangents if enabled
                if (bShowDebugSpline && GetWorld())
                {
                    DrawDebugLine(GetWorld(), MeshStartPos, MeshStartPos + MeshStartTan * 0.3f, FColor::Red, false, -1.f, 0, 1.0f);
                    DrawDebugLine(GetWorld(), MeshEndPos, MeshEndPos + MeshEndTan * 0.3f, FColor::Green, false, -1.f, 0, 1.0f);
                }
            }
        }
    }
    
    HideUnusedSegments(MeshIndex);
}

USplineMeshComponent* URopeRenderComponent::GetPooledSegment(int32 Index)
{
    // Expand pool if needed
    if (Index >= MeshPool.Num())
    {
        // Add new item
        USplineMeshComponent* Comp = NewObject<USplineMeshComponent>(this);
        if (!Comp) return nullptr;

        Comp->SetStaticMesh(RopeMesh);
        Comp->SetMaterial(0, RopeMaterial);
        Comp->SetMobility(EComponentMobility::Movable);
        Comp->SetForwardAxis(ForwardAxis);
        
        // Scale Factor Calculation
        float ScaleFactor = RopeThickness / MeshRadius;
        Comp->SetStartScale(FVector2D(ScaleFactor, ScaleFactor));
        Comp->SetEndScale(FVector2D(ScaleFactor, ScaleFactor));

        Comp->RegisterComponent();
        
        // CRITICAL: Ensure we render in World Space, as our simulation is in World Space.
        // Otherwise, attachment to the Character will double-transform the positions.
        Comp->SetUsingAbsoluteLocation(true);
        Comp->SetUsingAbsoluteRotation(true);
        Comp->SetUsingAbsoluteScale(true);

        Comp->AttachToComponent(this, FAttachmentTransformRules::KeepWorldTransform);
        MeshPool.Add(Comp);
    }

    // Return current item
    if (MeshPool.IsValidIndex(Index))
    {
        return MeshPool[Index];
    }
    
    return nullptr;
}

void URopeRenderComponent::HideUnusedSegments(int32 ActiveCount)
{
    // Safely hide unused segments
    for (int32 i = ActiveCount; i < MeshPool.Num(); i++)
    {
        USplineMeshComponent* Mesh = MeshPool[i];
        if (IsValid(Mesh))
        {
             Mesh->SetVisibility(false);
             // Verify if we need to Unregister? Usually SetVisibility(false) is enough and cheaper.
        }
    }
}
