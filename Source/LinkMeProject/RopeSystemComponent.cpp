// RopeSystemComponent.cpp - REFACTORED FOR BLUEPRINT LOGIC

#include "RopeSystemComponent.h"
#include "RopeHookActor.h"
#include "RopeRenderComponent.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "DrawDebugHelpers.h"

URopeSystemComponent::URopeSystemComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
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
}

void URopeSystemComponent::TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// State machine
	if (RopeState == ERopeState::Idle)
	{
		return;
	}

	if (RopeState == ERopeState::Flying)
	{
		if (CurrentHook && CurrentHook->HasImpacted())
		{
			TransitionToAttached(CurrentHook->GetImpactResult());
		}
	}

	if (RopeState == ERopeState::Attached)
	{
		// Always update player position
		UpdatePlayerPosition();

		// Apply physics forces
		ApplyForcesToPlayer();

		// CRITICAL: Call Blueprint event for wrap/unwrap logic
		OnRopeTickAttached(DeltaTime);

		// Debug display
		if (bShowDebug && GEngine)
		{
			GEngine->AddOnScreenDebugMessage(
				1, 0.f, FColor::Yellow,
				FString::Printf(TEXT("Rope Length: %.1f / %.1f | BendPoints: %d"), 
					CurrentLength, MaxLength, BendPoints.Num()));
		}
	}

	UpdateRopeVisual();
}

// ===================================================================
// ACTIONS
// ===================================================================

void URopeSystemComponent::FireHook(const FVector& Direction)
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
		BendPoints.Reset();
		UE_LOG(LogTemp, Log, TEXT("Hook fired successfully"));
	}
}

void URopeSystemComponent::Sever()
{
	if (CurrentHook)
	{
		CurrentHook->Destroy();
		CurrentHook = nullptr;
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

	OnRopeSevered();
}

void URopeSystemComponent::ReelIn(float DeltaTime)
{
	CurrentLength = FMath::Max(0.f, CurrentLength - ReelSpeed * DeltaTime);
}

void URopeSystemComponent::ReelOut(float DeltaTime)
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
	BendPoints.Add(ComputeBendPointFromHit(Hit, 15.f)); // Anchor
	BendPoints.Add(Owner->GetActorLocation()); // Player

	CurrentLength = FMath::Min(MaxLength, (Owner->GetActorLocation() - BendPoints[0]).Size());
	RopeState = ERopeState::Attached;

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

	if (RenderComponent && BendPoints.Num() > 0)
	{
		RenderComponent->RefreshFromBendPoints(BendPoints);
		RenderComponent->Simulate(GetWorld() ? GetWorld()->GetDeltaSeconds() : 0.f);
	}
}
