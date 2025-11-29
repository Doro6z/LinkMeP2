// RopeSystemComponent.cpp

#include "RopeSystemComponent.h"
#include "RopeHookActor.h"
#include "RopeRenderComponent.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "DrawDebugHelpers.h"
#include "RopeMeshUtils.h"

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
		// Main gameplay logic
		ManageBendPoints(DeltaTime);
		ApplyForcesToPlayer();
		
		if (bShowDebug && GEngine)
		{
			GEngine->AddOnScreenDebugMessage(
				1,
				0.f,
				FColor::Yellow,
				FString::Printf(TEXT("Rope Length: %.1f / %.1f | BendPoints: %d"), 
					CurrentLength, MaxLength, BendPoints.Num()));
		}
	}

	UpdateRopeVisual();
}

void URopeSystemComponent::FireHook(const FVector& Direction)
{
	UE_LOG(LogTemp, Warning, TEXT("FireHook called with Direction: %s"), *Direction.ToString());

	if (!HookClass)
	{
		UE_LOG(LogTemp, Error, TEXT("FireHook: HookClass not assigned!"));
		return;
	}

	if (!GetWorld())
	{
		UE_LOG(LogTemp, Error, TEXT("FireHook: World is null!"));
		return;
	}

	AActor* Owner = GetOwner();
	if (!Owner)
	{
		UE_LOG(LogTemp, Error, TEXT("FireHook: Owner is null!"));
		return;
	}

	const FVector SpawnLocation = Owner->GetActorLocation() + Direction * 50.f;
	const FRotator SpawnRotation = Direction.Rotation();

	FActorSpawnParameters Params;
	Params.Owner = Owner;
	Params.Instigator = Cast<APawn>(Owner);

	UE_LOG(LogTemp, Warning, TEXT("Attempting to spawn hook at: %s"), *SpawnLocation.ToString());

	CurrentHook = GetWorld()->SpawnActor<ARopeHookActor>(HookClass, SpawnLocation, SpawnRotation, Params);
	if (CurrentHook)
	{
		UE_LOG(LogTemp, Warning, TEXT("Hook spawned successfully!"));
		CurrentHook->Fire(Direction);
		CurrentHook->OnHookImpact.AddDynamic(this, &URopeSystemComponent::OnHookImpact);
		RopeState = ERopeState::Flying;
		BendPoints.Reset();
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("Failed to spawn hook!"));
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
	LastWrapPosition = FVector::ZeroVector;

	// Restore movement settings
	if (ACharacter* OwnerChar = Cast<ACharacter>(GetOwner()))
	{
		if (UCharacterMovementComponent* MoveComp = OwnerChar->GetCharacterMovement())
		{
			MoveComp->BrakingDecelerationFalling = DefaultBrakingDeceleration;
		}
	}
}

void URopeSystemComponent::OnHookImpact(const FHitResult& Hit)
{
	TransitionToAttached(Hit);
}

void URopeSystemComponent::TransitionToAttached(const FHitResult& Hit)
{
	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return;
	}

	BendPoints.Reset();
	BendPoints.Add(Hit.ImpactPoint + Hit.ImpactNormal * BendOffset); // anchor
	BendPoints.Add(Owner->GetActorLocation());

	CurrentLength = FMath::Min(MaxLength, (Owner->GetActorLocation() - BendPoints[0]).Size());
	RopeState = ERopeState::Attached;
	LastWrapPosition = FVector::ZeroVector;
}

void URopeSystemComponent::ApplyForcesToPlayer()
{
	ACharacter* OwnerChar = Cast<ACharacter>(GetOwner());
	if (!OwnerChar || BendPoints.Num() < 2)
	{
		return;
	}

	// 1. Calculate Total Physical Length of the rope (Sum of all segments)
	float TotalPhysicalLength = 0.f;
	for (int32 i = 0; i < BendPoints.Num() - 1; ++i)
	{
		TotalPhysicalLength += FVector::Distance(BendPoints[i], BendPoints[i+1]);
	}

	// 2. Determine Force Direction (Towards the Last Fixed Point)
	// BendPoints: [Anchor, ..., LastFixed, Player]
	const FVector PlayerPos = BendPoints.Last(); // Should be player pos
	const FVector LastFixedPoint = BendPoints[BendPoints.Num() - 2];
	const FVector DirToAnchor = (LastFixedPoint - PlayerPos).GetSafeNormal();

	// 3. Calculate Stretch
	const float Stretch = TotalPhysicalLength - CurrentLength;

	if (Stretch > 0.f)
	{
		if (UCharacterMovementComponent* MoveComp = OwnerChar->GetCharacterMovement())
		{
			// Disable air friction while swinging to preserve momentum
			MoveComp->BrakingDecelerationFalling = 0.f;

			// --- Velocity Clamping (Reel In Fix) ---
			FVector Velocity = MoveComp->Velocity;
			const float RadialSpeed = FVector::DotProduct(Velocity, DirToAnchor);
			
			if (RadialSpeed < 0.f)
			{
				// We are moving away, but the rope is taut. Kill the outward velocity.
				const FVector TangentVel = Velocity - RadialSpeed * DirToAnchor;
				MoveComp->Velocity = TangentVel;
			}

			// --- Apply Spring Force ---
			const FVector Pull = DirToAnchor * (Stretch * SpringStiffness);
			MoveComp->AddForce(Pull);

			// --- Swing Torque ---
			const FVector Right = FVector::CrossProduct(DirToAnchor, FVector::UpVector);
			MoveComp->AddForce(Right * SwingTorque);

			// --- Air Control ---
			const FVector InputDir = MoveComp->GetLastInputVector();
			if (!InputDir.IsNearlyZero())
			{
				const FVector TangentInput = FVector::VectorPlaneProject(InputDir, DirToAnchor).GetSafeNormal();
				MoveComp->AddForce(TangentInput * AirControlForce);
			}
		}
	}
}

void URopeSystemComponent::ReelIn(float DeltaTime)
{
	// Simply reduce the allowed length. The physics force will pull the player.
	CurrentLength = FMath::Max(0.f, CurrentLength - ReelSpeed * DeltaTime);
}

void URopeSystemComponent::ReelOut(float DeltaTime)
{
	// Increase allowed length.
	CurrentLength = FMath::Min(MaxLength, CurrentLength + ReelSpeed * DeltaTime);
}

FVector URopeSystemComponent::GetLastFixedPoint() const
{
	if (BendPoints.Num() < 2) return FVector::ZeroVector;
	return BendPoints[BendPoints.Num() - 2];
}

void URopeSystemComponent::ManageBendPoints(float DeltaTime)
{
	if (BendPoints.Num() < 2) return;

	// Update cooldowns
	WrapCooldownTimer -= DeltaTime;
	UnwrapCooldownTimer -= DeltaTime;

	// 1. Always update the last point to the player's current location
	if (AActor* Owner = GetOwner())
	{
		BendPoints.Last() = Owner->GetActorLocation();
	}

	// 2. Check for Wrapping (adding new bend points)
	CheckForWrapping(DeltaTime);

	// 3. Check for Unwrapping (removing bend points)
	CheckForUnwrapping(DeltaTime);
}

void URopeSystemComponent::CheckForWrapping(float DeltaTime)
{
	if (!GetWorld() || BendPoints.Num() < 2)
		return;

	// Safety cap
	if (BendPoints.Num() >= 30)
		return;

	// Cooldown to prevent immediate re-wrap after unwrap (optional, can be removed if robust)
	if (WrapCooldownTimer > 0.f)
		return;

	const FVector PlayerPos = BendPoints.Last();
	const FVector LastFixed = GetLastFixedPoint();

	// Geometric Sweep: LastFixed -> Player
	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(RopeWrapTrace), false, GetOwner());
	Params.bTraceComplex = true;
	Params.AddIgnoredActor(GetOwner());

	FCollisionShape Capsule = FCollisionShape::MakeCapsule(RopeRadius, RopeRadius * 2.f);

	if (!GetWorld()->SweepSingleByChannel(
		Hit,
		LastFixed,
		PlayerPos,
		FQuat::Identity,
		ECC_Visibility,
		Capsule,
		Params))
	{
		return;
	}

	if (!Hit.bBlockingHit || Hit.bStartPenetrating)
		return;

	// Compute precise bendpoint
	FVector NewBend = ComputeWarpBendPoint(LastFixed, PlayerPos, Hit);

	// Minimal distance check to avoid stacking (e.g. 1cm)
	if (FVector::DistSquared(NewBend, LastFixed) < 1.0f)
		return;

	// Add bendpoint
	BendPoints.Insert(NewBend, BendPoints.Num() - 1);

	// Set small cooldowns
	WrapCooldownTimer = 0.05f; 
	UnwrapCooldownTimer = 0.05f;

	if (bShowDebug)
	{
		DrawDebugSphere(GetWorld(), NewBend, 12, 12, FColor::Green, false, 3.f);
		UE_LOG(LogTemp, Warning, TEXT("WRAP: Added %s"), *NewBend.ToString());
	}
}

void URopeSystemComponent::CheckForUnwrapping(float DeltaTime)
{
	// Need at least 3 points: Anchor, [Bendpoint], Player
	if (BendPoints.Num() < 3) 
		return;

	if (UnwrapCooldownTimer > 0.f)
		return;

	// Points:
	// P = Player (Last)
	// B = Last Fixed Bendpoint (Num-2)
	// A = Previous Fixed Bendpoint (Num-3)
	
	const FVector P = BendPoints.Last();
	const FVector A = BendPoints[BendPoints.Num() - 3];

	// Line of Sight Check: Can Player see A?
	FHitResult LOSHit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(RopeUnwrapTrace), false, GetOwner());
	FCollisionShape Sphere = FCollisionShape::MakeSphere(RopeRadius * 0.5f);
	
	// Sweep from P to A
	bool bBlocked = GetWorld()->SweepSingleByChannel(
		LOSHit, 
		P, 
		A, 
		FQuat::Identity, 
		ECC_Visibility, 
		Sphere, 
		Params
	);

	if (!bBlocked)
	{
		// Path is clear -> Unwrap B
		BendPoints.RemoveAt(BendPoints.Num() - 2);
		
		UnwrapCooldownTimer = 0.05f;
		WrapCooldownTimer = 0.05f; // Prevent immediate re-wrap

		if (bShowDebug)
		{
			DrawDebugLine(GetWorld(), P, A, FColor::Cyan, false, 0.5f, 0, 2.f);
			UE_LOG(LogTemp, Log, TEXT("UNWRAP: Removed bendpoint"));
		}
	}
}

FVector URopeSystemComponent::RefineImpactPoint(const FVector& Start, const FVector& End, const FHitResult& InitialHit)
{
	// Dichotomic search to find the point just before collision
	FVector SafePoint = Start;
	FVector HitPoint = InitialHit.ImpactPoint;
	
	// 4 iterations to refine
	for(int i=0; i<4; ++i)
	{
		FVector Mid = (SafePoint + HitPoint) * 0.5f;
		FHitResult Hit;
		
		// Trace from Start to Mid to see if it's clear
		if(GetWorld()->LineTraceSingleByChannel(Hit, SafePoint, Mid, ECC_Visibility))
		{
			// Hit something, so the obstacle is closer than Mid
			HitPoint = Hit.ImpactPoint;
		}
		else
		{
			// Clear, so SafePoint can advance to Mid
			SafePoint = Mid;
		}
	}
	
	// Offset slightly along normal to prevent clipping
	return SafePoint + InitialHit.ImpactNormal * BendOffset;
}

FVector URopeSystemComponent::ComputeWarpBendPoint(const FVector& Start, const FVector& End, const FHitResult& Hit)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return Hit.ImpactPoint;
	}

	// 1. Triangle du Hit via RopeMeshUtils (StaticMesh / ProceduralMesh)
	const FTriangleData Tri = URopeMeshUtils::GetTriangleFromHit(Hit);
	if (!Tri.bValid)
	{
		// Si on n'a pas de triangle exploitable, on retombe sur ton ancienne méthode
		return RefineImpactPoint(Start, End, Hit);
	}

	// 2. Edge la plus proche de l’impact
	FVector EdgeA, EdgeB;
	URopeMeshUtils::GetClosestEdgeOnTriangle(
		Hit.ImpactPoint,
		Tri.A, Tri.B, Tri.C,
		EdgeA, EdgeB
	);

	// 3. Projection sur l’arête
	FVector NewPoint = URopeMeshUtils::ClosestPointOnSegment(EdgeA, EdgeB, Hit.ImpactPoint);

	// 4. Push-out de base dans la direction de la normale
	const FVector Normal = Hit.ImpactNormal.GetSafeNormal();
	const float BasePush = FMath::Max(BendOffset, 5.f);
	NewPoint += Normal * BasePush;

	// 5. Micro-correction si, malgré tout, on reste "dans" le mesh
	for (int32 Iter = 0; Iter < 3; ++Iter)
	{
		FHitResult TestHit;
		const FVector TestStart = NewPoint;
		const FVector TestEnd   = NewPoint - Normal * (BasePush * 1.5f);

		const bool bInside = World->LineTraceSingleByChannel(
			TestHit,
			TestStart,
			TestEnd,
			ECC_Visibility
		);

		if (!bInside || !TestHit.bBlockingHit)
		{
			break; // on sort
		}

		NewPoint += Normal * BasePush;
	}

	if (bShowDebug)
	{
		// Edge en rouge
		DrawDebugLine(World, EdgeA, EdgeB, FColor::Red, false, 2.f, 0, 2.f);
		// Nouveau bendpoint en violet
		DrawDebugSphere(World, NewPoint, 12.f, 12, FColor::Purple, false, 2.f);
	}

	return NewPoint;
}

void URopeSystemComponent::UpdateRopeVisual()
{
	if (bShowDebug && BendPoints.Num() > 0)
	{
		// Draw gameplay bend points (GREEN lines, RED spheres for points)
		for (int32 i = 0; i < BendPoints.Num() - 1; ++i)
		{
			DrawDebugLine(GetWorld(), BendPoints[i], BendPoints[i + 1], FColor::Green, false, -1.f, 0, 3.f);
			
			// First point (anchor) is YELLOW, others are RED
			FColor PointColor = (i == 0) ? FColor::Yellow : FColor::Red;
			DrawDebugSphere(GetWorld(), BendPoints[i], 12.f, 12, PointColor, false, -1.f, 0, 2.f);
		}
		// Player position (last bend point) is BLUE
		DrawDebugSphere(GetWorld(), BendPoints.Last(), 12.f, 12, FColor::Blue, false, -1.f, 0, 2.f);
		
		// Draw anchor label
		if (BendPoints.Num() > 0)
		{
			DrawDebugString(GetWorld(), BendPoints[0] + FVector(0, 0, 50), TEXT("ANCHOR (FIXED)"), nullptr, FColor::Yellow, -1.f, true);
		}
	}

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
