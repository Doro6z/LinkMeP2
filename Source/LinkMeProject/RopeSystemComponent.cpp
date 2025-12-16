// RopeSystemComponent.cpp - REFACTORED FOR BLUEPRINT LOGIC

#include "RopeSystemComponent.h"
#include "RopeHookActor.h"
#include "RopeRenderComponent.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "DrawDebugHelpers.h"
#include "Net/UnrealNetwork.h"

URopeSystemComponent::URopeSystemComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	SetIsReplicatedByDefault(true);
}

void URopeSystemComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(URopeSystemComponent, CurrentLength);
	DOREPLIFETIME(URopeSystemComponent, BendPoints);
	DOREPLIFETIME(URopeSystemComponent, RopeState);
	DOREPLIFETIME(URopeSystemComponent, CurrentHook);
}

void URopeSystemComponent::BeginPlay()
{
	Super::BeginPlay();

	RenderComponent = GetOwner() ? GetOwner()->FindComponentByClass<URopeRenderComponent>() : nullptr;

	if (ACharacter* OwnerChar = Cast<ACharacter>(GetOwner()))
	{
		if (UCharacterMovementComponent* MoveComp = OwnerChar->GetCharacterMovement())
		{
			DefaultBrakingDeceleration = MoveComp->BrakingDecelerationFalling;
		}
	}

	// Start Timer for physics updates (Server only for gameplay logic)
	// Only if bUseSubsteppedPhysics is TRUE
	// Timer removed for Tick-based physics (Fix Stutter)
	/*
	if (GetOwner() && GetOwner()->HasAuthority() && bUseSubsteppedPhysics)
	{
		GetWorld()->GetTimerManager().SetTimer(PhysicsTimerHandle, this, &URopeSystemComponent::PhysicsTick, 1.0f / PhysicsUpdateRate, true);
	}
	*/
}

void URopeSystemComponent::TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// Lightweight visual updates only
	if (RopeState == ERopeState::Idle) return;

    // --- WRAPPING LOGIC (Native) ---
    if (GetOwner()->HasAuthority())
    {
        if (RopeState == ERopeState::Flying && CurrentHook)
        {
            // Flying Wraps: Check between LastFixedPoint and Hook
            // In Flying, BendPoints ordered Player -> Hook. 
            // Start is Last Bend or Player. Target is Hook.
            FVector StartPos = BendPoints.Num() > 0 ? BendPoints.Last() : GetOwner()->GetActorLocation();
            CheckForWrapping(StartPos, CurrentHook->GetActorLocation());
            
            // Server: Check for hook impact
            if (CurrentHook->HasImpacted())
            {
                TransitionToAttached(CurrentHook->GetImpactResult());
            }
            
            // Note: UpdateRopeVisual handles the Flying render (Hook -> Player)
            // But if we wrapped, we have bends. 
            // We need to ensure UpdateRopeVisual knows about the Hook position for the last segment.
            // Currently it probably uses Player for last segment? 
            // Let's check UpdateRopeVisual.
        }
        else if (RopeState == ERopeState::Attached)
        {
            // Standard Wrapping: Check between LastFixedPoint and Player
            // Attached order: Anchor -> Bends -> Player
            // LastFixed is BendPoints[Num-2] (because Num-1 is Player, moving)
            if (BendPoints.Num() >= 2)
            {
                const FVector LastFixed = BendPoints[BendPoints.Num()-2];
                const FVector PlayerPos = GetOwner()->GetActorLocation();
                
                // 1. Wrap
                if (!CheckForWrapping(LastFixed, PlayerPos)) 
                {
                    // 2. Unwrap
                    CheckForUnwrapping(PlayerPos);
                }
            }
            
            // Guard against massive lag spikes for physics
            if (DeltaTime <= 0.1f)
            {
                PerformPhysics(DeltaTime);
            }
            
            // Always update player position for visual smoothness
		    UpdatePlayerPosition();
		    
		    // Debug HUD
            if (bShowDebug && GEngine)
            {
                GEngine->AddOnScreenDebugMessage(
                    1, 0.f, FColor::Yellow,
                    FString::Printf(TEXT("Rope Length: %.1f / %.1f | BendPoints: %d"), 
                        CurrentLength, MaxLength, BendPoints.Num()));
            }
        }
    }

	// Visual update (client + server)
	UpdateRopeVisual();
}

// Timer-based physics (Server only, called at PhysicsUpdateRate Hz)
// Timer-based physics tick (called at PhysicsUpdateRate Hz)
void URopeSystemComponent::PhysicsTick()
{
	PerformPhysics(1.0f / PhysicsUpdateRate);
}

void URopeSystemComponent::PerformPhysics(float DeltaTime)
{
	if (RopeState != ERopeState::Attached) return;

	// Heavy physics calculations
	ApplyForcesToPlayer();

	// Blueprint event for wrap/unwrap logic (Server authoritative)
	OnRopeTickAttached(DeltaTime);
}

void URopeSystemComponent::OnRep_BendPoints()
{
	// Force update the visual component when the server sends new topology
	UpdateRopeVisual();
}

// ===================================================================
// ACTIONS
// ===================================================================

void URopeSystemComponent::FireHook(const FVector& Direction)
{
	if (!GetOwner()->HasAuthority())
	{
		ServerFireHook(Direction);
		// Optional: Client prediction here (spawn fake hook)
		return;
	}
	ServerFireHook(Direction);
}

void URopeSystemComponent::ServerFireHook_Implementation(const FVector& Direction)
{
	if (!HookClass || !GetWorld())
	{
		UE_LOG(LogTemp, Error, TEXT("FireHook: HookClass or World is null"));
		return;
	}

	AActor* Owner = GetOwner();
	if (!Owner)
	{
		UE_LOG(LogTemp, Error, TEXT("FireHook: Owner is null"));
		return;
	}

    // --- Reset Existing Rope Logic ---
    if (CurrentHook)
    {
        CurrentHook->Destroy();
        CurrentHook = nullptr;
    }
    if (RenderComponent)
    {
        RenderComponent->ResetRope();
    }
    BendPoints.Reset();
    RopeState = ERopeState::Idle;
    // ---------------------------------

	const FVector SpawnLocation = Owner->GetActorLocation() + Direction * 50.f;
	const FRotator SpawnRotation = Direction.Rotation();

	FActorSpawnParameters Params;
	Params.Owner = Owner;
	Params.Instigator = Cast<APawn>(Owner);

	CurrentHook = GetWorld()->SpawnActor<ARopeHookActor>(HookClass, SpawnLocation, SpawnRotation, Params);
	if (CurrentHook)
	{
		CurrentHook->Fire(Direction);
		CurrentHook->OnHookImpact.AddDynamic(this, &URopeSystemComponent::OnHookImpact);
		RopeState = ERopeState::Flying;
		
		if (bShowDebug)
		{
			UE_LOG(LogTemp, Log, TEXT("Hook fired successfully (Server)"));
		}
	}
}

void URopeSystemComponent::FireChargedHook(const FVector& Velocity)
{
	if (!GetOwner()->HasAuthority())
	{
		ServerFireChargedHook(Velocity);
		return;
	}
	ServerFireChargedHook(Velocity);
}

void URopeSystemComponent::ServerFireChargedHook_Implementation(const FVector& Velocity)
{
	UE_LOG(LogTemp, Warning, TEXT("URopeSystemComponent::ServerFireChargedHook called with Velocity: %s"), *Velocity.ToString());

	if (!HookClass || !GetWorld()) 
	{
		if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Red, TEXT("[SERVER] ERROR: HookClass or World is null!"));
		UE_LOG(LogTemp, Error, TEXT("ServerFireChargedHook: HookClass or World is null"));
		return;
	}

	AActor* Owner = GetOwner();
	if (!Owner) return;

	// Reset Logic
	if (CurrentHook)
	{
		CurrentHook->Destroy();
		CurrentHook = nullptr;
	}
	if (RenderComponent) RenderComponent->ResetRope();
	BendPoints.Reset();
	RopeState = ERopeState::Idle;

	// Spawn
	// Use a safer offset to avoid initial overlap (Capsule Radius is usually ~34-40)
	const FVector SpawnLocation = Owner->GetActorLocation() + Velocity.GetSafeNormal() * 100.f;
	const FRotator SpawnRotation = Velocity.Rotation();

	FActorSpawnParameters Params;
	Params.Owner = Owner;
	Params.Instigator = Cast<APawn>(Owner);

	CurrentHook = GetWorld()->SpawnActor<ARopeHookActor>(HookClass, SpawnLocation, SpawnRotation, Params);
	if (CurrentHook)
	{
		CurrentHook->FireVelocity(Velocity);
		CurrentHook->OnHookImpact.AddDynamic(this, &URopeSystemComponent::OnHookImpact);
		RopeState = ERopeState::Flying;
		
		if (bShowDebug)
		{
			if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Green, TEXT("[SERVER] Hook Spawned & Fired!"));
			UE_LOG(LogTemp, Warning, TEXT("ServerFireChargedHook: Hook spawned and fired successfully"));
		}
	}
	else if (bShowDebug)
	{
		if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Red, TEXT("[SERVER] ERROR: Failed to Spawn Hook!"));
		UE_LOG(LogTemp, Error, TEXT("ServerFireChargedHook: Failed to spawn Hook Actor"));
	}
}

void URopeSystemComponent::Sever()
{
    // Client-side visual reset for instant feedback
    if (RenderComponent)
    {
        RenderComponent->ResetRope();
    }

	if (!GetOwner()->HasAuthority())
	{
		ServerSever();
		return;
	}
	ServerSever();
}

void URopeSystemComponent::ServerSever_Implementation()
{
	if (CurrentHook)
	{
		CurrentHook->Destroy();
		CurrentHook = nullptr;
	}

    if (RenderComponent)
    {
        RenderComponent->ResetRope();
    }

	BendPoints.Reset();
	BendPointNormals.Reset();  // Keep normals array in sync
	CurrentLength = 0.f;
	RopeState = ERopeState::Idle;

	// Restore movement settings
	if (ACharacter* OwnerChar = Cast<ACharacter>(GetOwner()))
	{
		if (UCharacterMovementComponent* MoveComp = OwnerChar->GetCharacterMovement())
		{
			MoveComp->BrakingDecelerationFalling = DefaultBrakingDeceleration;
		}
	}

	// This event should probably be multicast if visual effects are needed
	OnRopeSevered();
}


void URopeSystemComponent::ReelIn(float DeltaTime)
{
	if (!GetOwner()->HasAuthority())
	{
		ServerReelIn(DeltaTime);
		return;
	}
	ServerReelIn(DeltaTime);
}

void URopeSystemComponent::ServerReelIn_Implementation(float DeltaTime)
{
	CurrentLength = FMath::Max(0.f, CurrentLength - ReelSpeed * DeltaTime);
}

void URopeSystemComponent::ReelOut(float DeltaTime)
{
	if (!GetOwner()->HasAuthority())
	{
		ServerReelOut(DeltaTime);
		return;
	}
	ServerReelOut(DeltaTime);
}

void URopeSystemComponent::ServerReelOut_Implementation(float DeltaTime)
{
	CurrentLength = FMath::Min(MaxLength, CurrentLength + ReelSpeed * DeltaTime);
}

// ===================================================================
// BENDPOINT MANAGEMENT
// ===================================================================

void URopeSystemComponent::AddBendPoint(const FVector& Location)
{
	// Delegate to the full version with a default normal
	AddBendPointWithNormal(Location, FVector::UpVector);
}

void URopeSystemComponent::AddBendPointWithNormal(const FVector& Location, const FVector& SurfaceNormal)
{
	if (BendPoints.Num() < 2)
	{
		UE_LOG(LogTemp, Warning, TEXT("AddBendPoint: Need at least 2 points (anchor + player)"));
		return;
	}

	// CRITICAL: Ensure normals array is same size as positions array
	// This handles the case where rope was initialized before this function was implemented
	while (BendPointNormals.Num() < BendPoints.Num())
	{
		BendPointNormals.Add(FVector::UpVector); // Fill with default normals
	}

	// Insert position before the last element (player position)
	const int32 InsertIndex = BendPoints.Num() - 1;
	BendPoints.Insert(Location, InsertIndex);
	
	// Keep normals array in sync - now safe because we ensured same size
	BendPointNormals.Insert(SurfaceNormal, InsertIndex);

	if (bShowDebug)
	{
		DrawDebugSphere(GetWorld(), Location, 12, 12, FColor::Green, false, 2.f);
		DrawDebugLine(GetWorld(), Location, Location + SurfaceNormal * 30.f, FColor::Cyan, false, 2.f);
		UE_LOG(LogTemp, Log, TEXT("WRAP: Added bendpoint at %s with normal %s"), 
			*Location.ToString(), *SurfaceNormal.ToString());
	}
}

void URopeSystemComponent::RemoveBendPointAt(int32 Index)
{
	if (!BendPoints.IsValidIndex(Index))
	{
		UE_LOG(LogTemp, Warning, TEXT("RemoveBendPointAt: Invalid index %d"), Index);
		return;
	}

	// Protect anchor (0) and player (last)
	if (Index == 0 || Index == BendPoints.Num() - 1)
	{
		UE_LOG(LogTemp, Warning, TEXT("RemoveBendPointAt: Cannot remove anchor or player point"));
		return;
	}

	BendPoints.RemoveAt(Index);
	
	// Keep normals array in sync
	if (BendPointNormals.IsValidIndex(Index))
	{
		BendPointNormals.RemoveAt(Index);
	}

	if (bShowDebug)
	{
		UE_LOG(LogTemp, Log, TEXT("UNWRAP: Removed bendpoint at index %d"), Index);
	}
}

FVector URopeSystemComponent::GetLastFixedPoint() const
{
	if (BendPoints.Num() < 2) return FVector::ZeroVector;
	return BendPoints[BendPoints.Num() - 2];
}

FVector URopeSystemComponent::GetPlayerPosition() const
{
	if (BendPoints.Num() < 1) return FVector::ZeroVector;
	return BendPoints.Last();
}

FVector URopeSystemComponent::GetAnchorPosition() const
{
	if (BendPoints.Num() < 1) return FVector::ZeroVector;
	return BendPoints[0];
}

void URopeSystemComponent::UpdatePlayerPosition()
{
	if (BendPoints.Num() < 1 || !GetOwner()) return;
	BendPoints.Last() = GetOwner()->GetActorLocation();
}


// ===================================================================
// TRACE UTILITIES
// ===================================================================

bool URopeSystemComponent::CapsuleSweepBetween(
	const FVector& Start, 
	const FVector& End, 
	FHitResult& OutHit,
	float Radius,
	bool bTraceComplex)
{
	if (!GetWorld()) return false;

	FCollisionQueryParams Params(SCENE_QUERY_STAT(RopeTrace), bTraceComplex, GetOwner());
	Params.AddIgnoredActor(GetOwner());

	FCollisionShape Capsule = FCollisionShape::MakeCapsule(Radius, Radius * 2.f);

	bool bHit = GetWorld()->SweepSingleByChannel(
		OutHit,
		Start,
		End,
		FQuat::Identity,
		RopeTraceChannel,
		Capsule,
		Params
	);

	if (bShowDebug && bHit)
	{
		DrawDebugCapsule(GetWorld(), OutHit.ImpactPoint, Radius * 2.f, Radius, FQuat::Identity, FColor::Orange, false, 1.f);
	}

	return bHit && OutHit.bBlockingHit && !OutHit.bStartPenetrating;
}

FVector URopeSystemComponent::FindLastClearPoint(
	const FVector& Start,
	const FVector& End,
	int32 Subdivisions,
	float SphereRadius)
{
	if (!GetWorld()) return Start;

	Subdivisions = FMath::Max(1, Subdivisions);
	FVector LastClear = Start;

	for (int32 i = 1; i <= Subdivisions; ++i)
	{
		const float Alpha = static_cast<float>(i) / static_cast<float>(Subdivisions);
		const FVector TestPoint = FMath::Lerp(Start, End, Alpha);

		FHitResult Hit;
		FCollisionQueryParams Params(SCENE_QUERY_STAT(RopeSubstepTrace), false, GetOwner());
		FCollisionShape Sphere = FCollisionShape::MakeSphere(SphereRadius);

		bool bHit = GetWorld()->SweepSingleByChannel(
			Hit,
			TestPoint,
			TestPoint + FVector(0, 0, 1), // Minimal sweep
			FQuat::Identity,
			RopeTraceChannel,
			Sphere,
			Params
		);

		if (!bHit || !Hit.bBlockingHit)
		{
			LastClear = TestPoint;
		}
		else
		{
			break; // Hit geometry, stop
		}

		if (bShowDebug)
		{
			DrawDebugSphere(GetWorld(), TestPoint, SphereRadius, 8, bHit ? FColor::Red : FColor::Green, false, 0.5f);
		}
	}

	return LastClear;
}

FVector URopeSystemComponent::ComputeBendPointFromHit(const FHitResult& Hit, float Offset) const
{
	return Hit.ImpactPoint + Hit.ImpactNormal * Offset;
}

// ===================================================================
// PHYSICS
// ===================================================================

void URopeSystemComponent::ApplyForcesToPlayer()
{
	ACharacter* OwnerChar = Cast<ACharacter>(GetOwner());
	if (!OwnerChar || BendPoints.Num() < 2)
	{
		return;
	}

	// Calculate total physical length
	float TotalPhysicalLength = 0.f;
	for (int32 i = 0; i < BendPoints.Num() - 1; ++i)
	{
		TotalPhysicalLength += FVector::Distance(BendPoints[i], BendPoints[i+1]);
	}

	// Force direction towards last fixed point
	const FVector PlayerPos = BendPoints.Last();
	const FVector LastFixedPoint = BendPoints[BendPoints.Num() - 2];
	const FVector DirToAnchor = (LastFixedPoint - PlayerPos).GetSafeNormal();

	// Calculate stretch
	const float Stretch = TotalPhysicalLength - CurrentLength;

	if (Stretch > 0.f)
	{
		if (UCharacterMovementComponent* MoveComp = OwnerChar->GetCharacterMovement())
		{
			// Disable air friction while swinging
			MoveComp->BrakingDecelerationFalling = 0.f;

			// Velocity clamping (prevent moving away when rope is taut)
			FVector Velocity = MoveComp->Velocity;
			const float RadialSpeed = FVector::DotProduct(Velocity, DirToAnchor);
			
			if (RadialSpeed < 0.f)
			{
				const FVector TangentVel = Velocity - RadialSpeed * DirToAnchor;
				MoveComp->Velocity = TangentVel;
			}

			// Spring force
			const FVector Pull = DirToAnchor * (Stretch * SpringStiffness);
			MoveComp->AddForce(Pull);

			// Swing torque
			const FVector Right = FVector::CrossProduct(DirToAnchor, FVector::UpVector);
			MoveComp->AddForce(Right * SwingTorque);

			// Air control
			const FVector InputDir = MoveComp->GetLastInputVector();
			if (!InputDir.IsNearlyZero())
			{
				const FVector TangentInput = FVector::VectorPlaneProject(InputDir, DirToAnchor).GetSafeNormal();
				MoveComp->AddForce(TangentInput * AirControlForce);
			}
		}
	}
}

// ===================================================================
// INTERNAL
// ===================================================================

void URopeSystemComponent::OnHookImpact(const FHitResult& Hit)
{
	TransitionToAttached(Hit);
}

void URopeSystemComponent::TransitionToAttached(const FHitResult& Hit)
{
    AActor* Owner = GetOwner();
    if (!Owner) return;

    // BendPoints.Reset(); // REMOVED: Preserve Flying Bends!

    FVector HookImpactPoint = Hit.ImpactPoint;
    FVector PlayerPosition = Owner->GetActorLocation();

    FVector OffsetDebugY = FVector(0, 0, 50); // Décalage visuel pour les traces debug

    // ==========================================================
    // DEBUG: Draw incoming impact line
    // ==========================================================
    if (bShowDebug)
    {
        DrawDebugSphere(GetWorld(), HookImpactPoint, 12.f, 12, FColor::Red, false, 5.f, 0, 1.5f);
        DrawDebugLine(GetWorld(), PlayerPosition + OffsetDebugY, HookImpactPoint + OffsetDebugY,
                      FColor::Red, false, 3.f, 0, 2.f);
        DrawDebugPoint(GetWorld(), HookImpactPoint + OffsetDebugY, 12.f, FColor::Red, false, 4.f);

        UE_LOG(LogTemp, Log, TEXT("[TransitionToAttached] HookImpact: %s"),
            *HookImpactPoint.ToString());
    }

    // ==========================================================
    // Correct Anchor using FindLastClearPoint (with debug)
    // ==========================================================

    TArray<FVector> SubstepDebugPoints; // <- Collecte des substeps

    FVector CorrectedAnchor = FindLastClearPoint(
        PlayerPosition,
        HookImpactPoint,
        25,       // Subdivisions
        5.f      // Sphere radiu
    );

    // ==========================================================
    // DEBUG: Draw Substep Points
    // ==========================================================
    if (bShowDebug)
    {
        for (int32 i = 0; i < SubstepDebugPoints.Num(); i++)
        {
            const FVector& P = SubstepDebugPoints[i];
            DrawDebugSphere(GetWorld(), P + OffsetDebugY, 6.f, 8,
                            FColor::Yellow, false, 3.f, 0, 1.f);

            if (i > 0)
            {
                DrawDebugLine(GetWorld(),
                    SubstepDebugPoints[i - 1] + OffsetDebugY,
                    P + OffsetDebugY,
                    FColor::Yellow,
                    false, 3.f, 0, 0.5f
                );
            }
        }

        UE_LOG(LogTemp, Log, TEXT("[TransitionToAttached] Substeps: %d points"), SubstepDebugPoints.Num());
    }

    // ==========================================================
    // Offset from normal
    // ==========================================================

    if (Hit.bBlockingHit && !Hit.ImpactNormal.IsNearlyZero())
    {
        CorrectedAnchor += Hit.ImpactNormal * 15.f;

        if (bShowDebug)
        {
            DrawDebugLine(GetWorld(),
                CorrectedAnchor + OffsetDebugY,
                CorrectedAnchor + OffsetDebugY + Hit.ImpactNormal * 50.f,
                FColor::Cyan,
                false, 3.f, 0, 0.5f
            );
        }
    }

    // ==========================================================
    // Final BendPoint setup (Preserve Flying Wraps!)
    // ==========================================================
    
    // 1. Capture Flying Bends (Order: Near Player -> Near Hook)
    TArray<FVector> FlyingBends = BendPoints;
    TArray<FVector> FlyingNormals = BendPointNormals;
    
    // 2. Reset
    BendPoints.Reset();
    BendPointNormals.Reset();
    
    // 3. Add Anchor (Start)
    BendPoints.Add(CorrectedAnchor);
    BendPointNormals.Add(Hit.ImpactNormal); // Anchor normal
    
    // 4. Append Flying Bends REVERSED (to match Order: Anchor -> Player)
    for (int32 i = FlyingBends.Num() - 1; i >= 0; --i)
    {
        BendPoints.Add(FlyingBends[i]);
        if (FlyingNormals.IsValidIndex(i))
        {
            BendPointNormals.Add(FlyingNormals[i]);
        }
        else
        {
             BendPointNormals.Add(FVector::UpVector);
        }
    }
    
    // 5. Add Player (End)
    BendPoints.Add(PlayerPosition);
    BendPointNormals.Add(FVector::UpVector); // Player normal (dummy)

    // Calculate total length across all bends
    float TotalDist = 0.f;
    for (int32 i = 0; i < BendPoints.Num() - 1; ++i)
    {
        TotalDist += FVector::Dist(BendPoints[i], BendPoints[i+1]);
    }
    CurrentLength = FMath::Min(MaxLength, TotalDist);

    RopeState = ERopeState::Attached;

    // ==========================================================
    // Debug final anchor
    // ==========================================================
    if (bShowDebug)
    {
        UE_LOG(LogTemp, Log, TEXT("[TransitionToAttached] CorrectedAnchor: %s"),
            *CorrectedAnchor.ToString());

        DrawDebugSphere(GetWorld(), CorrectedAnchor, 14.f, 16, FColor::Green, false, 5.f, 0, 2.f);

        DrawDebugLine(GetWorld(),
            HookImpactPoint + OffsetDebugY,
            CorrectedAnchor + OffsetDebugY,
            FColor::Blue,
            false, 3.f, 0, 2.f
        );
    }

    // Notify BP / Other systems
    OnRopeAttached(Hit);
}

// ===================================================================
// SURFACE NORMAL VALIDATION - Implementation
// ===================================================================

FVector URopeSystemComponent::CalculatePressureDirection(
    const FVector& PointA,
    const FVector& PointB,
    const FVector& PointP)
{
    // Calculate unit vectors from B towards A and P
    FVector DirToA = (PointA - PointB).GetSafeNormal();
    FVector DirToP = (PointP - PointB).GetSafeNormal();

    // The bisector (pressure direction) is the sum of these unit vectors
    // If the rope is perfectly straight (180°), this will be ZeroVector
    FVector Bisector = DirToA + DirToP;

    // Normalize to get the direction of force
    return Bisector.GetSafeNormal();
}

bool URopeSystemComponent::IsRopePullingAway(
    const FVector& PressureDir,
    const FVector& SurfaceNormal,
    float Tolerance)
{
    // Edge Case: If rope is perfectly straight, pressure dir is zero
    // In this case, geometry no longer constrains the rope -> Safe to unwrap
    if (PressureDir.IsNearlyZero(0.01f))
    {
        return true;
    }

    // Dot Product:
    // < 0 : Pressure and Normal are opposite (rope pushes INTO wall)
    // > 0 : Pressure and Normal are same direction (rope pulls AWAY from wall)
    float WallPressure = FVector::DotProduct(PressureDir, SurfaceNormal);

    // If WallPressure < Tolerance (e.g., -0.05), rope is still pressed against wall
    // We require WallPressure >= Tolerance to unwrap
    return WallPressure >= Tolerance;
}

bool URopeSystemComponent::ShouldUnwrapPhysical(
    const FVector& PrevFixed,
    const FVector& CurrentBend,
    const FVector& CurrentBendNormal,
    const FVector& PlayerPos,
    float AngleThreshold,
    bool bCheckLineTrace)
{
    // ============================================================
    // TIER 1: ANGLE CHECK (Hysteresis - Fast Rejection)
    // ============================================================
    FVector DirA = (PrevFixed - CurrentBend).GetSafeNormal();
    FVector DirP = (PlayerPos - CurrentBend).GetSafeNormal();

    float DotAlignment = FVector::DotProduct(DirA, DirP);

    // If the angle is too sharp (e.g., > 2° or Dot > -0.999), reject immediately
    // This prevents unwrapping when we're still wrapping around a corner
    if (DotAlignment > AngleThreshold)
    {
        return false; // Angle too sharp, keep bend point
    }

    // ============================================================
    // TIER 2: SURFACE NORMAL CHECK (Anti-Tunneling)
    // ============================================================
    FVector PressureDir = CalculatePressureDirection(PrevFixed, CurrentBend, PlayerPos);

    if (!IsRopePullingAway(PressureDir, CurrentBendNormal, -0.05f))
    {
        // Rope is still pushing against the wall
        // Even if angle looks flat, the physics says we're still constrained
        #if WITH_EDITOR
        if (bShowDebug && GetWorld())
        {
            // Debug: Show why we're blocked
            DrawDebugLine(GetWorld(), CurrentBend, CurrentBend + PressureDir * 50.f, 
                FColor::Red, false, 1.f, 0, 2.f);
            DrawDebugLine(GetWorld(), CurrentBend, CurrentBend + CurrentBendNormal * 50.f, 
                FColor::Blue, false, 1.f, 0, 2.f);
            DrawDebugString(GetWorld(), CurrentBend + FVector(0, 0, 30), 
                TEXT("BLOCKED: Rope Pushing"), nullptr, FColor::Red, 1.f);
        }
        #endif

        return false; // Blocked by surface normal check
    }

    // ============================================================
    // TIER 3: LINE TRACE (Final Safety - Detect Other Obstacles)
    // ============================================================
    if (bCheckLineTrace && GetWorld())
    {
        FHitResult Hit;
        FCollisionQueryParams Params(SCENE_QUERY_STAT(RopeUnwrapTrace), false, GetOwner());

        bool bBlocked = GetWorld()->LineTraceSingleByChannel(
            Hit,
            PrevFixed,
            PlayerPos,
            ECC_Visibility,
            Params
        );

        if (bBlocked && Hit.bBlockingHit)
        {
            #if WITH_EDITOR
            if (bShowDebug)
            {
                DrawDebugLine(GetWorld(), PrevFixed, PlayerPos, 
                    FColor::Orange, false, 1.f, 0, 2.f);
                DrawDebugSphere(GetWorld(), Hit.ImpactPoint, 10.f, 8, 
                    FColor::Orange, false, 1.f);
                DrawDebugString(GetWorld(), Hit.ImpactPoint + FVector(0, 0, 30), 
                    TEXT("BLOCKED: Other Obstacle"), nullptr, FColor::Orange, 1.f);
            }
            #endif

            return false; // Path blocked by another obstacle
        }
    }

    // ============================================================
    // ALL CHECKS PASSED - SAFE TO UNWRAP
    // ============================================================
    #if WITH_EDITOR
    if (bShowDebug && GetWorld())
    {
        DrawDebugLine(GetWorld(), PrevFixed, PlayerPos, 
            FColor::Green, false, 1.f, 0, 3.f);
        DrawDebugString(GetWorld(), CurrentBend + FVector(0, 0, 30), 
            TEXT("UNWRAP OK"), nullptr, FColor::Green, 1.f);
    }
    #endif

    return true;
}


void URopeSystemComponent::UpdateRopeVisual()
{
	if (!RenderComponent)
	{
		RenderComponent = GetOwner() ? GetOwner()->FindComponentByClass<URopeRenderComponent>() : nullptr;
		if (!RenderComponent) return;
	}

    TArray<FVector> PointsToRender;
    bool bShouldRender = false;
    bool bIsDeploying = false;

    // 1. Determine Points & Behavior based on State
    if (RopeState == ERopeState::Flying)
    {
        if (CurrentHook && GetOwner())
        {
            // Flying Wraps Support
            // Order: Player -> [Intermediate Bends] -> Hook
            PointsToRender.Add(GetOwner()->GetActorLocation());
            
            // Add any wrapped points
            if (BendPoints.Num() > 0)
            {
               PointsToRender.Append(BendPoints);
            }
            
            PointsToRender.Add(CurrentHook->GetActorLocation());
            
            bShouldRender = true;
            bIsDeploying = true; // Enable Dynamic RestLength
        }
    }
    else if (RopeState == ERopeState::Attached)
    {
         if (BendPoints.Num() >= 2)
         {
             // BendPoints already contains [Anchor, ... , Player]
             PointsToRender = BendPoints;
             bShouldRender = true;
             bIsDeploying = false;
         }
    }
    
    // 2. Execute Update on Render Component
    if (bShouldRender)
    {
        bool bStateChanged = (RopeState != LastRopeState);
        bool bTopologyChanged = (PointsToRender.Num() != LastPointCount);
        bool bFirstRender = !RenderComponent->IsRopeActive();
        
        // Condition for Full Rebuild:
        // - First time rendering
        // - State transition (Mode changed)
        // - Topology changed (Point count changed)
        if (bFirstRender || bStateChanged || bTopologyChanged)
        {
            RenderComponent->UpdateRope(PointsToRender, bIsDeploying);
        }
        else
        {
            // POSITION UPDATE ONLY
            // RenderComponent->UpdatePinPositions(PointsToRender); // Legacy opt
            // But we want to ensure Linkage, so UpdatePinPositions is OK if we fixed it in Render.
            // However, previous attempt used UpdateRope. 
            // The signature is UpdateRope(Points, bDeploying).
            RenderComponent->UpdateRope(PointsToRender, bIsDeploying);
        }
        
        LastPointCount = PointsToRender.Num();
    }
    else
    {
        // Go Idle
        if (RenderComponent->IsRopeActive())
        {
            RenderComponent->HideRope();
        }
        LastPointCount = 0;
    }
    
    LastRopeState = RopeState;

	// Debug rendering
	if (bShowDebug && PointsToRender.Num() > 0)
	{
		for (int32 i = 0; i < PointsToRender.Num() - 1; ++i)
		{
			DrawDebugLine(GetWorld(), PointsToRender[i], PointsToRender[i + 1], FColor::Green, false, -1.f, 0, 3.f);
		}
	}
}

// ===================================================================
// WRAPPING LOGIC
// ===================================================================

bool URopeSystemComponent::CheckForWrapping(const FVector& StartPos, const FVector& TargetPos)
{
   // 1. Simple Trace
    FHitResult Hit;
    if (CapsuleSweepBetween(StartPos, TargetPos, Hit, 5.0f, true))
    {
        // 2. Refine Corner Position
        FVector CornerPos = ComputeBendPointFromHit(Hit, 15.0f);
        
        // Filter: Is this point too close to previous ones?
        if (FVector::DistSquared(CornerPos, StartPos) < FMath::Square(20.0f)) return false;
        if (FVector::DistSquared(CornerPos, TargetPos) < FMath::Square(20.0f)) return false;

        AddBendPointWithNormal(CornerPos, Hit.ImpactNormal);
        return true;
    }
    return false;
}

bool URopeSystemComponent::CheckForUnwrapping(const FVector& TargetPos)
{
    // Need at least 1 intermediate bend point (Anchor... Bend... Player) to unwrap
    // In Attached mode (where this is called), BendPoints has endpoints.
    // [Anchor, Bend1, Bend2, Player] -> Num=4.
    // We check Bend2 (Index Num-2).
    // Prev is Bend1 (Index Num-3).
    // If BendPoints.Num() < 3, no intermediate bends.
    
    if (BendPoints.Num() < 3) return false; 
    
    // Candidate to remove is the LAST INTERMEDIATE point
    // Index: Num - 2 (since Num-1 is Player)
    const int32 CandidateIdx = BendPoints.Num() - 2;
    const FVector CandidatePos = BendPoints[CandidateIdx];
    
    // Previous Fixed (Bend before Candidate)
    const int32 PrevIdx = CandidateIdx - 1;
    const FVector PrevPos = BendPoints[PrevIdx];
    
    // Normal Check
    FVector Normal = FVector::UpVector;
    if (BendPointNormals.IsValidIndex(CandidateIdx)) Normal = BendPointNormals[CandidateIdx];
    
    // Should Unwrap?
    if (ShouldUnwrapPhysical(PrevPos, CandidatePos, Normal, TargetPos))
    {
        RemoveBendPointAt(CandidateIdx);
        return true;
    }
    
    return false;
}
