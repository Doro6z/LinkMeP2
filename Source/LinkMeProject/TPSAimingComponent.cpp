#include "TPSAimingComponent.h"
#include "Kismet/GameplayStatics.h"
#include "DrawDebugHelpers.h"

UTPSAimingComponent::UTPSAimingComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UTPSAimingComponent::BeginPlay()
{
	Super::BeginPlay();
}

void UTPSAimingComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	// Call base tick (performs basic line/sphere trace)
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// Update magnetism if aiming
	if (bIsAiming)
	{
		UpdateMagnetism(DeltaTime);
	}
	else
	{
		bHasMagnetizedTarget = false;
		CurrentMagnetizedActor = nullptr;
	}
}


void UTPSAimingComponent::UpdateMagnetism(float DeltaTime)
{
	if (!bEnableMagnetism)
	{
		bHasMagnetizedTarget = false;
		CurrentMagnetizedActor = nullptr;
		return;
	}

	APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
	if (!PC) return;

	FVector CamLoc;
	FRotator CamRot;
	PC->GetPlayerViewPoint(CamLoc, CamRot);
	FVector CamForward = CamRot.Vector();

	// Find best target
	AActor* BestTarget = FindBestMagnetismTarget(CamLoc, CamForward);

	if (BestTarget)
	{
		// Magnetize to target
		FVector IdealTarget = BestTarget->GetActorLocation();
		
		// Smooth interpolation towards target
		float InterpSpeed = MagnetismStrength * 15.0f;
		MagnetizedTargetLocation = FMath::VInterpTo(
			CurrentTargetLocation,
			IdealTarget,
			DeltaTime,
			InterpSpeed
		);

		bHasMagnetizedTarget = true;
		CurrentMagnetizedActor = BestTarget;

		// Broadcast event if target changed
		if (CurrentMagnetizedActor != CurrentTargetActor)
		{
			OnTargetAcquired.Broadcast(MagnetizedTargetLocation, CurrentMagnetizedActor);
		}
		
		// VISUAL DEBUG
		if (bShowDebug)
		{
			DrawDebugLine(GetWorld(), CurrentTargetLocation, MagnetizedTargetLocation, FColor::Cyan, false, -1.0f, 0, 2.0f);
			DrawDebugSphere(GetWorld(), MagnetizedTargetLocation, 15.0f, 8, FColor::Cyan, false, -1.0f);
			UE_LOG(LogTemp, Warning, TEXT("[TPSAiming] Magnetized to %s"), *GetNameSafe(BestTarget));
		}
	}
	else
	{
		// No magnetism target
		bHasMagnetizedTarget = false;
		MagnetizedTargetLocation = CurrentTargetLocation;
		
		if (CurrentMagnetizedActor != nullptr)
		{
			CurrentMagnetizedActor = nullptr;
		}
	}
}

AActor* UTPSAimingComponent::FindBestMagnetismTarget(const FVector& CamLoc, const FVector& CamForward) const
{
	TArray<AActor*> HookableActors;
	UGameplayStatics::GetAllActorsWithTag(GetWorld(), HookableTag, HookableActors);

	AActor* BestTarget = nullptr;
	float BestScore = FLT_MAX;

	for (AActor* Actor : HookableActors)
	{
		if (!Actor) continue;

		FVector ToTarget = Actor->GetActorLocation() - CamLoc;
		float Distance = ToTarget.Size();

		// Check range
		if (Distance > MagnetismRange) continue;

		// Check cone angle
		FVector ToTargetNorm = ToTarget.GetSafeNormal();
		float DotProduct = FVector::DotProduct(CamForward, ToTargetNorm);
		float AngleRad = FMath::Acos(DotProduct);
		float AngleDeg = FMath::RadiansToDegrees(AngleRad);

		if (AngleDeg > MagnetismConeAngle) continue;

		// Score: prefer closer and more centered targets
		float Score = Distance + (AngleDeg * 100.0f);

		if (Score < BestScore)
		{
			BestScore = Score;
			BestTarget = Actor;
		}
	}

	return BestTarget;
}

FVector UTPSAimingComponent::GetTargetLocation() const
{
	// Return magnetized target if available
	if (bHasMagnetizedTarget)
	{
		return MagnetizedTargetLocation;
	}

	// Otherwise return base target
	return Super::GetTargetLocation();
}

FVector UTPSAimingComponent::GetAimDirection() const
{
	APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
	if (!PC)
	{
		return FVector::ForwardVector;
	}

	FVector CamLoc;
	FRotator CamRot;
	PC->GetPlayerViewPoint(CamLoc, CamRot);

	// Use magnetized target if available
	FVector TargetLoc = bHasMagnetizedTarget ? MagnetizedTargetLocation : CurrentTargetLocation;

	// Aim towards target location
	if (bHasValidTarget || bHasMagnetizedTarget)
	{
		return (TargetLoc - CamLoc).GetSafeNormal();
	}

	// Otherwise return camera forward
	return CamRot.Vector();
}


void UTPSAimingComponent::StartFocus()
{
	bIsFocusing = true;
	// Focus automatically starts aiming
	if (!bIsAiming)
	{
		StartAiming();
	}
}

void UTPSAimingComponent::StopFocus()
{
	bIsFocusing = false;
	// Note: we don't stop aiming, that's controlled separately
}
