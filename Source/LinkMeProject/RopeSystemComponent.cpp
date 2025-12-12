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
	if (GetOwner() && GetOwner()->HasAuthority() && bUseSubsteppedPhysics)
	{
		GetWorld()->GetTimerManager().SetTimer(
			PhysicsTimerHandle,
			this,
			&URopeSystemComponent::PhysicsTick,
			1.0f / PhysicsUpdateRate,
			true // Loop
		);
	}
}

void URopeSystemComponent::TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// Lightweight visual updates only
	if (RopeState == ERopeState::Idle) return;

	// If NOT using substepped physics (Timer), run physics on Tick
	if (!bUseSubsteppedPhysics && GetOwner() && GetOwner()->HasAuthority() && RopeState == ERopeState::Attached)
	{
		PerformPhysics(DeltaTime);
	}

	if (RopeState == ERopeState::Flying)
	{
		// Server: Check for hook impact
		if (GetOwner()->HasAuthority() && CurrentHook && CurrentHook->HasImpacted())
		{
			TransitionToAttached(CurrentHook->GetImpactResult());
		}
	}

	if (RopeState == ERopeState::Attached)
	{
		// Always update player position for visual smoothness
		UpdatePlayerPosition();

		// Debug HUD (non-spammy)
		if (bShowDebug && GEngine)
		{
			GEngine->AddOnScreenDebugMessage(
				1, 0.f, FColor::Yellow,
				FString::Printf(TEXT("Rope Length: %.1f / %.1f | BendPoints: %d"), 
					CurrentLength, MaxLength, BendPoints.Num()));
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
	if (BendPoints.Num() < 2)
	{
		UE_LOG(LogTemp, Warning, TEXT("AddBendPoint: Need at least 2 points (anchor + player)"));
		return;
	}

	// Insert before the last element (player position)
	BendPoints.Insert(Location, BendPoints.Num() - 1);

	if (bShowDebug)
	{
		DrawDebugSphere(GetWorld(), Location, 12, 12, FColor::Green, false, 2.f);
		UE_LOG(LogTemp, Log, TEXT("WRAP: Added bendpoint at %s"), *Location.ToString());
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

    BendPoints.Reset();

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
    // Final BendPoint setup
    // ==========================================================
    BendPoints.Add(CorrectedAnchor);
    BendPoints.Add(PlayerPosition);

    CurrentLength = FMath::Min(
        MaxLength,
        (PlayerPosition - CorrectedAnchor).Size()
    );

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


void URopeSystemComponent::UpdateRopeVisual()
{
	// Debug rendering
	if (bShowDebug && BendPoints.Num() > 0)
	{
		for (int32 i = 0; i < BendPoints.Num() - 1; ++i)
		{
			DrawDebugLine(GetWorld(), BendPoints[i], BendPoints[i + 1], FColor::Green, false, -1.f, 0, 3.f);
			
			FColor PointColor = (i == 0) ? FColor::Yellow : FColor::Red;
			DrawDebugSphere(GetWorld(), BendPoints[i], 12.f, 12, PointColor, false, -1.f, 0, 2.f);
		}
		DrawDebugSphere(GetWorld(), BendPoints.Last(), 12.f, 12, FColor::Blue, false, -1.f, 0, 2.f);
	}

	// Update render component
	if (!RenderComponent)
	{
		RenderComponent = GetOwner() ? GetOwner()->FindComponentByClass<URopeRenderComponent>() : nullptr;
	}

	if (RenderComponent)
	{
		FVector PlayerPos = GetOwner() ? GetOwner()->GetActorLocation() : BendPoints.Last();
		RenderComponent->UpdateVisualSegments(BendPoints, PlayerPos, CurrentLength, MaxLength);
	}
}
