// RopeSystemComponent.cpp

#include "RopeSystemComponent.h"
#include "RopeHookActor.h"
#include "RopeRenderComponent.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
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
                ManageBendPoints(DeltaTime);

                ApplyForcesToPlayer();
		
                if (GEngine)
                {
                        GEngine->AddOnScreenDebugMessage(1, 0.f, FColor::Yellow, FString::Printf(TEXT("Rope Length: %.1f / %.1f | BendPoints: %d"),
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
LastPlayerLocation = Owner->GetActorLocation();
WrapCooldownTimer = 0.f;
UnwrapCooldownTimer = 0.f;
}

void URopeSystemComponent::ManageBendPoints(float DeltaTime)
{
        AActor* Owner = GetOwner();
        if (!Owner || BendPoints.Num() == 0)
        {
                return;
        }

        // Cooldowns avoid oscillations when hugging corners.
        WrapCooldownTimer = FMath::Max(0.f, WrapCooldownTimer - DeltaTime);
        UnwrapCooldownTimer = FMath::Max(0.f, UnwrapCooldownTimer - DeltaTime);

        const FVector PlayerPos = Owner->GetActorLocation();
        if (LastPlayerLocation.IsNearlyZero())
        {
                LastPlayerLocation = PlayerPos;
        }

        // Guarantee the last element is always the player location.
        if (BendPoints.Num() == 1)
        {
                BendPoints.Add(PlayerPos);
        }
        else
        {
                BendPoints.Last() = PlayerPos;
        }

        // Early out if the rope somehow lost its anchor.
        if (BendPoints.Num() < 2)
        {
                return;
        }

        const int32 LastFixedIndex = BendPoints.Num() - 2;
        const FVector LastFixedPoint = BendPoints[LastFixedIndex];

        // --- Wrapping: find a new bend between the last fixed point and the player ---
        if (WrapCooldownTimer <= 0.f && BendPoints.Num() < MaxBendPoints)
        {
                FHitResult Hit;
                if (SweepForHit(LastFixedPoint, PlayerPos, Hit))
                {
                        const float ImpactDistance = FVector::Distance(LastFixedPoint, Hit.ImpactPoint);
                        const FVector IncomingDir = (PlayerPos - LastFixedPoint).GetSafeNormal();
                        const float AngleFromNormal = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(FVector::DotProduct(-IncomingDir, Hit.ImpactNormal), -1.f, 1.f)));

                        const bool bLongEnough = ImpactDistance > MinSegmentLength;
                        const bool bCornerSharpEnough = AngleFromNormal >= CornerThresholdDegrees;

                        if (bLongEnough && bCornerSharpEnough)
                        {
                                FVector RefinedPoint = Hit.ImpactPoint;
                                FVector RefinedNormal = Hit.ImpactNormal;
                                RefineImpactPoint(LastFixedPoint, PlayerPos, RefinedPoint, RefinedNormal);

                                const FVector BendPoint = RefinedPoint + RefinedNormal * BendOffset;
                                BendPoints.Insert(BendPoint, BendPoints.Num() - 1);

                                WrapCooldownTimer = WrapCooldown;
                                UnwrapCooldownTimer = FMath::Max(UnwrapCooldownTimer, WrapCooldown * 0.5f); // avoid instant unwrap

                                if (bShowDebug)
                                {
                                        DrawDebugSphere(GetWorld(), BendPoint, 15.f, 12, FColor::Magenta, false, 0.1f, 0, 2.f);
                                        DrawDebugLine(GetWorld(), LastFixedPoint, BendPoint, FColor::Yellow, false, 0.2f, 0, 2.f);
                                }
                        }
                }
        }

        // --- Unwrapping: remove the latest bend once the player crosses the plane and regains sight ---
        if (BendPoints.Num() > 2 && UnwrapCooldownTimer <= 0.f)
        {
                const FVector LastBend = BendPoints[BendPoints.Num() - 2];
                const FVector PreviousFixed = BendPoints[BendPoints.Num() - 3];

                FHitResult VisibilityHit;
                const bool bBlocked = SweepForHit(PlayerPos, PreviousFixed, VisibilityHit);

                const float Dot = FVector::DotProduct((PlayerPos - LastBend).GetSafeNormal(), (PreviousFixed - LastBend).GetSafeNormal());
                const bool bCrossedPlane = Dot > UnwrapDotThreshold;

                if (!bBlocked && bCrossedPlane)
                {
                        BendPoints.RemoveAt(BendPoints.Num() - 2);
                        UnwrapCooldownTimer = UnwrapCooldown;
                        WrapCooldownTimer = FMath::Max(WrapCooldownTimer, UnwrapCooldown * 0.5f);

                        if (bShowDebug)
                        {
                                DrawDebugSphere(GetWorld(), PreviousFixed, 18.f, 12, FColor::Blue, false, 0.1f, 0, 2.f);
                        }
                }
        }

        // Recompute current length from fixed bend points (gameplay length authority)
        CurrentLength = 0.f;
        for (int32 i = 0; i < BendPoints.Num() - 1; ++i)
        {
                CurrentLength += FVector::Distance(BendPoints[i], BendPoints[i + 1]);
        }

        CurrentLength = FMath::Min(CurrentLength, MaxLength);

        LastPlayerLocation = PlayerPos;
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
	// We want to pull the player towards LastFixed (Index Num-2)
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
			// Only clamp velocity if we are moving AWAY from the anchor point.
			// If we are moving TOWARDS it (Reeling In), allow the velocity!
			// Dot Product: Velocity . DirToAnchor
			// > 0 : Moving TOWARDS anchor (Good)
			// < 0 : Moving AWAY from anchor (Bad, clamp it)
			
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
	CurrentLength = FMath::Max(0.f, CurrentLength - ReelSpeed * DeltaTime);
}

void URopeSystemComponent::ReelOut(float DeltaTime)
{
	CurrentLength = FMath::Min(MaxLength, CurrentLength + ReelSpeed * DeltaTime);
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

bool URopeSystemComponent::SweepForHit(const FVector& Start, const FVector& End, FHitResult& OutHit) const
{
        if (!GetWorld())
        {
                return false;
        }

        FCollisionQueryParams Params(SCENE_QUERY_STAT(RopeSweep), false, GetOwner());
        Params.AddIgnoredActor(CurrentHook);

        const float HalfHeight = RopeRadius * 2.f;
        const FCollisionShape Capsule = FCollisionShape::MakeCapsule(RopeRadius, HalfHeight);
        return GetWorld()->SweepSingleByChannel(OutHit, Start, End, FQuat::Identity, ECC_Visibility, Capsule, Params);
}

void URopeSystemComponent::RefineImpactPoint(const FVector& Start, const FVector& End, FVector& OutPoint, FVector& OutNormal) const
{
        FVector SegmentStart = Start;
        FVector SegmentEnd = End;
        OutPoint = End;
        OutNormal = FVector::UpVector;

        // A few iterations of binary search along the swept segment to find a stable contact point.
        for (int32 Iter = 0; Iter < 4; ++Iter)
        {
                FHitResult Hit;
                if (SweepForHit(SegmentStart, SegmentEnd, Hit))
                {
                        OutPoint = Hit.ImpactPoint;
                        OutNormal = Hit.ImpactNormal;

                        // Shrink search range towards the impact to converge.
                        SegmentEnd = Hit.ImpactPoint - Hit.ImpactNormal * RopeRadius;
                }
                else
                {
                        SegmentStart = (SegmentStart + SegmentEnd) * 0.5f;
                }
        }
}

// ========== OLD BEND POINT SYSTEM (Legacy, for reference) ==========

void URopeSystemComponent::ManageBendPointsOld()
{
	// This is the old LineTrace-based system that had penetration issues
	// Kept for reference, not called anymore
}

// ========== HELPER FUNCTIONS ==========

bool URopeSystemComponent::LineTrace(const FVector& Start, const FVector& End, FHitResult& OutHit) const
{
	FCollisionQueryParams Params(SCENE_QUERY_STAT(RopeTrace), false, GetOwner());
	
	// Reverted to LineTrace because SweepSingleByChannel was starting inside geometry (due to small BendOffset)
	// causing immediate blocking hits at Distance 0 which were ignored or invalid.
	// To fix visual clipping, we will rely on a larger BendOffset or visual-only offset in the renderer.
	return GetWorld()->LineTraceSingleByChannel(OutHit, Start, End, ECC_Visibility, Params);
}
