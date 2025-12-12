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
    for(int32 i=0; i<ActiveParticleCount - 1; i++) 
    {
         if (Particles[i].bIsActive && Particles[i+1].bIsActive)
             CurrentLength += FVector::Dist(Particles[i].Position, Particles[i+1].Position);
    }
    
    GEngine->AddOnScreenDebugMessage(KeyBase + 4, 0.0f, FColor::White, 
        FString::Printf(TEXT("  Sim Length: %.2f m"), CurrentLength / 100.0f));

    // New Debug Info
    FColor TensionColor = bRopeIsTaut ? FColor::Red : FColor::Cyan;
    GEngine->AddOnScreenDebugMessage((int32)GetUniqueID() + 500, 0.0f, TensionColor, 
        FString::Printf(TEXT("  STATE: %s  (Tension: %.2f)"), bRopeIsTaut ? TEXT("TAUT") : TEXT("SLACK"), GetRopeTension()));
    
    GEngine->AddOnScreenDebugMessage((int32)GetUniqueID() + 600, 0.0f, FColor::Yellow, 
        FString::Printf(TEXT("  CacheLen: %.2f / Max: %.2f"), CachedCurrentRopeLength, CachedMaxRopeLength));
}

void URopeRenderComponent::UpdateVisualSegments(const TArray<FVector>& BendPoints, const FVector& EndPosition, float InCurrentLength, float InMaxLength)
{
    if (BendPoints.Num() == 0) return;

    CachedCurrentRopeLength = InCurrentLength;
    CachedMaxRopeLength = InMaxLength;
    
    // Detect Tension State
    // Tension is based on: Visual Length (Actual) vs Deployed Length (Constraint)
    // But for now, user asked to use RopeSystem length. 
    // If we assume "CurrentLength" from System IS the constraint, and the Rope IS satisfying it...
    // Actually, we usually compare VisualLength to CurrentLength.
    // User said: "Rope Length est celui du RopeSystem".
    // So let's use that for our displayed stats and internal logic threshold.
    
    // We calculate Visual Length
    float VisualLen = 0.0f;
    for (int32 i = 0; i < BendPoints.Num() - 1; i++)
         VisualLen += FVector::Dist(BendPoints[i], BendPoints[i+1]);
    VisualLen += FVector::Dist(BendPoints.Last(), EndPosition);

    // Tension: If Visual Length matches or exceeds Deployed Limit
    float TensionRatio = VisualLen / FMath::Max(1.0f, CachedCurrentRopeLength);
    
    // FIXED STIFFNESS LOGIC:
    // When slack (TensionRatio < 0.4): NO straightening, let gravity work naturally
    // When taut (TensionRatio 0.4 to 1.0): Progressive straightening with cubic ease-in
    const float SlackThreshold = 0.4f;  // Below this: pure sag
    const float TautThresholdVal = 1.0f; // At this: full steel cable mode
    
    if (TensionRatio < SlackThreshold)
    {
        // Fully slack: NO straightening at all
        CachedStiffnessAlpha = 0.0f;
    }
    else
    {
        // Map [0.4, 1.0] → [0.0, 1.0] for stiffness blend
        float RawAlpha = FMath::GetMappedRangeValueClamped(
            FVector2D(SlackThreshold, TautThresholdVal), 
            FVector2D(0.0f, 1.0f), 
            TensionRatio
        );
        
        // Cubic ease-in: Slow start, then rapid tightening as we approach full tension
        CachedStiffnessAlpha = FMath::Pow(RawAlpha, 3.0f);
    }
    
    // Binary taut flag (for backwards compat / debug)
    bRopeIsTaut = (TensionRatio >= 0.95f);

    // Debug Info on Screen
    if (GEngine)
    {
        int32 KeyBase = 77700;
        FColor StateColor = FColor::MakeRedToGreenColorFromScalar(CachedStiffnessAlpha);
        GEngine->AddOnScreenDebugMessage(KeyBase + 1, 0.0f, StateColor, 
            FString::Printf(TEXT("[Rope] TENSION: %.2f | Stiffness: %.2f"), 
            TensionRatio, CachedStiffnessAlpha));
            
        GEngine->AddOnScreenDebugMessage(KeyBase + 2, 0.0f, FColor::Yellow, 
            FString::Printf(TEXT("[Rope] Limit: %.0f / Vis: %.0f"), 
            CachedCurrentRopeLength, VisualLen));
    }
    // If first time, initialize particles in a straight line
    if (!bInitialized)
    {
        InitializeParticles(BendPoints[0], EndPosition);
        bInitialized = true; // CRITICAL: Enable rendering pipeline
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



int32 URopeRenderComponent::CalculateSegmentParticleCount(float SegmentLength) const
{
    // On veut au moins 1 particule par segment
    // On arrondit pour avoir un nombre stable d'entiers
    int32 Count = FMath::Max(1, FMath::RoundToInt(SegmentLength / MeshLengthBase));
    return Count;
}

void URopeRenderComponent::RebuildConstraints(const TArray<FVector>& BendPoints, const FVector& EndPosition)
{
    // Safety: Ensure Pool matches MaxParticles to prevent bounds errors
    if (Particles.Num() != MaxParticles)
    {
        Particles.SetNum(MaxParticles);
        // Ensure new particles are inactive
        for (FRopeParticle& P : Particles) P.bIsActive = false;
    }

    // 1. Reset
    PinConstraints.Reset();
    DistanceConstraints.Reset();
    ConstraintRestLengths.Reset(); // Nouveau array
    
    // On déverrouille toutes les masses par défaut
    for (FRopeParticle& P : Particles) P.InverseMass = 1.0f;

    int32 GlobalParticleIndex = 0;
    
    // Ancre de départ (Toujours bloquée)
    Particles[0].Position = BendPoints[0];
    Particles[0].InverseMass = 0.0f;
    Particles[0].bIsActive = true;
    
    FPinnedConstraint StartPin;
    StartPin.ParticleIndex = 0;
    StartPin.WorldLocation = BendPoints[0];
    StartPin.bActive = true;
    PinConstraints.Add(StartPin);

    // 2. Boucle Topologique sur les Segments
    // On itère de 0 à N (BendPoints + Position Joueur)
    // Nombre de segments = BendPoints.Num() car le dernier segment va vers EndPosition
    // ATTENTION : BendPoints contient déjà l'Ancre et les Coins.
    
    // Stratégie :
    // Segment 0 : BendPoints[0] -> BendPoints[1]
    // ...
    // Segment Final : BendPoints.Last() -> EndPosition
    
    TArray<FVector> AllPoints = BendPoints;
    AllPoints.Add(EndPosition); // On ajoute le joueur à la fin de la liste logique

    for (int32 i = 0; i < AllPoints.Num() - 1; i++)
    {
        FVector StartPos = AllPoints[i];
        FVector EndPos = AllPoints[i+1];
        bool bIsLastSegment = (i == AllPoints.Num() - 2); // Le segment connecté au joueur

        float SegmentDist = FVector::Dist(StartPos, EndPos);
        
        // A. CALCUL DU BUDGET LOCAL
        int32 SegmentCount;
        
        if (bIsLastSegment)
        {
            // Segment Joueur : On prend tout ce qui reste
            // Cela permet une élongation fluide sans "Snap" d'arrondi violent
            int32 Remaining = MaxParticles - GlobalParticleIndex - 1;
            
            if (Remaining < 1)
            {
                // Plus de budget : on connecte directement à la particule existante
                SegmentCount = 0;
            }
            else
            {
                // On calcule l'idéal et on clampe
                int32 Ideal = FMath::CeilToInt(SegmentDist / MeshLengthBase);
                SegmentCount = FMath::Clamp(Ideal, 1, Remaining);
            }
        }
        else
        {
            // Segment Fixe : Arrondi Strict -> Nombre de particules CONSTANT -> Pas de jitter
            SegmentCount = CalculateSegmentParticleCount(SegmentDist);
            
            // Safety check overflow
            if (GlobalParticleIndex + SegmentCount >= MaxParticles)
            {
                SegmentCount = MaxParticles - GlobalParticleIndex - 1;
                if(SegmentCount < 1) break; // Plus de budget
            }
        }

        // B. COMPENSATION D'ÉLASTICITÉ (Le secret anti-pop)
        // Si le segment fait 105cm et qu'on a 10 particules -> RestLength = 10.5cm
        float CompensatedRestLength = SegmentDist / (float)SegmentCount;

        // C. GÉNÉRATION DES PARTICULES ET CONTRAINTES
        for (int32 k = 0; k < SegmentCount; k++)
        {
            int32 CurrentIdx = GlobalParticleIndex + k;
            int32 NextIdx = CurrentIdx + 1;

            // Activation
            Particles[NextIdx].bIsActive = true;
            
            // Position initiale (Seulement si nouvelle activation pour éviter le teleport)
            // Note: Pour une stabilité parfaite, sur les segments fixes, on peut forcer la position
            // Mais laissons la physique faire le lerp.
            
            // Création de la contrainte de distance
            FDistanceConstraint DistC;
            DistC.IndexA = CurrentIdx;
            DistC.IndexB = NextIdx;
            DistC.RestLength = CompensatedRestLength; // Valeur compensée !
            DistC.Compliance = StretchCompliance;
            DistanceConstraints.Add(DistC);
        }

        // D. PINNING (Verrouillage des coins)
        // Si ce n'est pas le segment du joueur, on CLOUE la dernière particule au BendPoint suivant.
        if (!bIsLastSegment)
        {
            int32 EndSegmentIndex = GlobalParticleIndex + SegmentCount;
            
            // 1. Force la position exacte (Anti-Drift)
            Particles[EndSegmentIndex].Position = EndPos;
            Particles[EndSegmentIndex].PredictedPosition = EndPos;
            
            // 2. Masse Infinie (Le mur ne bouge pas)
            Particles[EndSegmentIndex].InverseMass = 0.0f;
            
            // 3. Ajout Contrainte Pin (pour le solver)
            FPinnedConstraint Pin;
            Pin.ParticleIndex = EndSegmentIndex;
            Pin.WorldLocation = EndPos;
            Pin.bActive = true;
            PinConstraints.Add(Pin);
        }

        GlobalParticleIndex += SegmentCount;
    }
    
    // Update du count final
    ActiveParticleCount = GlobalParticleIndex + 1;
    
    // Pin Final (Joueur)
    // On le laisse généralement avec une masse 0 pour qu'il suive parfaitement le joueur/gun
    Particles[ActiveParticleCount-1].InverseMass = 0.0f;
    Particles[ActiveParticleCount-1].Position = EndPosition;
    
    FPinnedConstraint EndPin;
    EndPin.ParticleIndex = ActiveParticleCount-1;
    EndPin.WorldLocation = EndPosition;
    EndPin.bActive = true;
    PinConstraints.Add(EndPin);
}


// SimulateXPBD: Replaced with hardened version
void URopeRenderComponent::SimulateXPBD(float DeltaTime)
{
    // ALT-TAB PROTECTION: Skip entire frame if DeltaTime is too large
    // This prevents simulation explosions from accumulated time
    if (DeltaTime > 0.1f)
    {
        return; // Skip this frame entirely - no simulation
    }

    if (DeltaTime <= KINDA_SMALL_NUMBER) return;

    float SubStepDt = DeltaTime / (float)SubSteps;
    const float MaxSpeed = 20000.0f; // 200m/s limit (increased from 50m/s to allow fast swings)
    const float MaxSpeedSq = MaxSpeed * MaxSpeed;

    for (int32 Step = 0; Step < SubSteps; Step++)
    {
        // 1. Predict
        for (FRopeParticle& P : Particles)
        {
            if (!P.bIsActive) continue;
            
            // Skip gravity for pinned particles
            if (P.InverseMass == 0.0f)
            {
                P.PredictedPosition = P.Position;
                continue;
            }
            
            // Apply Gravity / External Forces (ALWAYS)
            P.Velocity += Gravity * SubStepDt;
            
            P.PredictedPosition = P.Position + P.Velocity * SubStepDt;
        }

        // 2. Solve Constraints
        SolveConstraints(SubStepDt);

        // 3. Update Integration
        for (FRopeParticle& P : Particles)
        {
            if (!P.bIsActive) continue;
            
            P.Velocity = (P.PredictedPosition - P.Position) / SubStepDt;
            
            // --- VELOCITY CLAMPING (Stability) ---
            if (P.Velocity.SizeSquared() > MaxSpeedSq)
            {
                P.Velocity = P.Velocity.GetSafeNormal() * MaxSpeed;
            }
            if (P.Velocity.ContainsNaN())
            {
                P.Velocity = FVector::ZeroVector; 
            }
            // -------------------------------------

            P.Velocity *= Damping; // Apply damping to smooth motion
            P.PreviousPosition = P.Position;
            P.Position = P.PredictedPosition;

            // Correction if we clamped:
            if (P.Velocity.SizeSquared() > MaxSpeedSq)
            {
                 P.Position = P.PreviousPosition + P.Velocity * SubStepDt;
            }
            
            // 4. Ground Collision
            if (bEnableCollision && P.InverseMass > 0.0f)
            {
                FHitResult Hit;
                FCollisionQueryParams Params;
                Params.AddIgnoredActor(GetOwner());

                FVector Start = P.PreviousPosition;
                FVector End = P.Position;
                

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
                    // Hard Constraint with Magnetic Softness
                    // Instead of a hard Lerp (snap), we use a magnetic force field
                    FVector Delta = TargetPos - P.PredictedPosition;
                    float Dist = Delta.Size();
                    
                    if (Dist > KINDA_SMALL_NUMBER)
                    {
                        // Calculate Magnetic Falloff
                        // Strength = 1 / (1 + (Dist/Radius)^2)
                        float Radius = FMath::Max(1.0f, Pin.MagneticRadius);
                        float NormDist = Dist / Radius;
                        float Falloff = 1.0f / (1.0f + NormDist * NormDist);
                        
                        // Apply attraction
                        // Combine magnetic strength with global PinStrength
                        float Attraction = Pin.MagneticStrength * Falloff * PinStrength;
                        
                        // Limit attraction to avoid overshooting in one step
                        Attraction = FMath::Clamp(Attraction, 0.0f, 1.0f);
                        
                        P.PredictedPosition += Delta * Attraction;
                    }
                    else
                    {
                        P.PredictedPosition = TargetPos;
                    }
                }
                else
                {
                    // Soft Constraint (XPBD for dynamic mass)
                    float Alpha = BendPointCompliance / (Dt * Dt);
                    float W = P.InverseMass;
                    float Factor = W / (W + Alpha);
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

    // --- VISUAL TENSION (Per-Segment Straightening) ---
    // Applied with gradient stiffness to avoid abrupt transitions
    // Straighten each segment between consecutive pins independently
    if (CachedStiffnessAlpha > KINDA_SMALL_NUMBER && ActiveParticleCount > 2)
    {
        // Collect and sort pin indices
        TArray<int32> PinIndices;
        for (const FPinnedConstraint& Pin : PinConstraints)
        {
            if (Pin.bActive && Particles.IsValidIndex(Pin.ParticleIndex))
            {
                PinIndices.AddUnique(Pin.ParticleIndex);
            }
        }
        PinIndices.Sort();
        
        // For each segment between consecutive pins
        for (int32 s = 0; s < PinIndices.Num() - 1; s++)
        {
            int32 SegStart = PinIndices[s];
            int32 SegEnd = PinIndices[s + 1];
            
            // Skip if segment is too short
            if (SegEnd - SegStart <= 1) continue;
            
            FVector A = Particles[SegStart].PredictedPosition;
            FVector B = Particles[SegEnd].PredictedPosition;
            
            // Straighten particles between these two pins
            for (int32 i = SegStart + 1; i < SegEnd; i++)
            {
                if (Particles[i].InverseMass == 0.0f || !Particles[i].bIsActive) continue;
                
                float Alpha = (float)(i - SegStart) / (float)(SegEnd - SegStart);
                FVector IdealPos = FMath::Lerp(A, B, Alpha);
                
                // Apply correction scaled by gradient stiffness
                FVector Delta = IdealPos - Particles[i].PredictedPosition;
                Particles[i].PredictedPosition += Delta * (0.15f * CachedStiffnessAlpha);
            }
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

// --- BLUEPRINT API IMPLEMENTATION ---

void URopeRenderComponent::SetRopeParticles(const TArray<FVector>& Positions)
{
    if (Positions.Num() == 0) return;
    
    // Resize pool if needed (hard reset)
    ActiveParticleCount = FMath::Min(Positions.Num(), MaxParticles);
    
    for (int32 i = 0; i < ActiveParticleCount; i++)
    {
        Particles[i].Position = Positions[i];
        Particles[i].PreviousPosition = Positions[i];
        Particles[i].PredictedPosition = Positions[i];
        Particles[i].bIsActive = true;
    }
    
    // Deactivate others
    for (int32 i = ActiveParticleCount; i < MaxParticles; i++)
    {
        Particles[i].bIsActive = false;
    }
    
    bInitialized = true;
}

TArray<FVector> URopeRenderComponent::GetRopeParticlePositions() const
{
    TArray<FVector> Positions;
    Positions.Reserve(ActiveParticleCount);
    for (int32 i = 0; i < ActiveParticleCount; i++)
    {
        Positions.Add(Particles[i].Position);
    }
    return Positions;
}

void URopeRenderComponent::SetPinConstraints(const TArray<FPinnedConstraint>& NewPins)
{
    PinConstraints = NewPins;
}

void URopeRenderComponent::UpdatePinLocation(int32 PinIndex, FVector NewLocation)
{
    if (PinConstraints.IsValidIndex(PinIndex))
    {
        PinConstraints[PinIndex].WorldLocation = NewLocation;
    }
}

void URopeRenderComponent::SetRopeSimulationParams(int32 InSubSteps, int32 InIterations, float InDamping, float InGravityScale)
{
    SubSteps = FMath::Clamp(InSubSteps, 1, 10);
    SolverIterations = FMath::Clamp(InIterations, 1, 20);
    Damping = FMath::Clamp(InDamping, 0.0f, 1.0f);
    Gravity = FVector(0, 0, -980.0f) * InGravityScale;
}

void URopeRenderComponent::SetVisualTensionParams(bool bEnableStraightening, float InStraighteningStiffness)
{
    // Note: bEnableStraightening maps to whether we apply the constraint
    // We can use TautThreshold manipulation or add a specific flag if needed.
    // For now, let's just expose the stiffness param if we store it.
    // Currently hardcoded to 0.15f * StiffnessAlpha.
    // TODO: Expose StraighteningStiffness as a member variable.
}

void URopeRenderComponent::ForceRebuildConstraints(const TArray<FVector>& BendPoints, const FVector& EndPosition)
{
    RebuildConstraints(BendPoints, EndPosition);
}

float URopeRenderComponent::GetVisualRopeLength() const
{
    float Len = 0.0f;
    for (int32 i = 0; i < ActiveParticleCount - 1; i++)
    {
        Len += FVector::Dist(Particles[i].Position, Particles[i+1].Position);
    }
    return Len;
}

float URopeRenderComponent::GetRopeTension() const
{
    return CachedStiffnessAlpha; // Normalized tension
}



