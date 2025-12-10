#include "AimingComponent.h"
#include "GameFramework/Actor.h"
#include "GameFramework/PlayerController.h"
#include "Engine/World.h"
#include "Camera/CameraComponent.h"
#include "Kismet/GameplayStatics.h"
#include "DrawDebugHelpers.h"

UAimingComponent::UAimingComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	bIsAiming = false;
	bHasValidTarget = false;
}

void UAimingComponent::BeginPlay()
{
	Super::BeginPlay();
}

void UAimingComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!bIsAiming)
	{
		if (bHasValidTarget) // Was valid, now not aiming
		{
			OnTargetLost.Broadcast();
		}
		bHasValidTarget = false;
		CurrentTargetActor = nullptr;
		return;
	}

	// Perform trace from Camera center
	APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
	if (!PC) return;

	FVector CamLoc;
	FRotator CamRot;
	PC->GetPlayerViewPoint(CamLoc, CamRot);

	const FVector Start = CamLoc;
	const FVector End = Start + (CamRot.Vector() * MaxRange);

	FHitResult Hit;
	FCollisionQueryParams Params;
	Params.AddIgnoredActor(GetOwner());
	
	// Also ignore all components attached to owner (mesh, capsule, etc.)
	if (AActor* Owner = GetOwner())
	{
		TArray<UActorComponent*> Components;
		Owner->GetComponents(Components);
		for (UActorComponent* Comp : Components)
		{
			if (UPrimitiveComponent* PrimComp = Cast<UPrimitiveComponent>(Comp))
			{
				Params.AddIgnoredComponent(PrimComp);
			}
		}
	}

	bool bHit = false;

	// Use SphereTrace if Radius is defined
	if (AimingRadius > 0.0f)
	{
		FCollisionShape Sphere = FCollisionShape::MakeSphere(AimingRadius);
		bHit = GetWorld()->SweepSingleByChannel(Hit, Start, End, FQuat::Identity, AimingTraceChannel, Sphere, Params);
	}
	else
	{
		bHit = GetWorld()->LineTraceSingleByChannel(Hit, Start, End, AimingTraceChannel, Params);
	}

	// Debug Draw & Verbose Logs
	if (bShowDebug)
	{
		// Log Trace Info
		UE_LOG(LogTemp, Warning, TEXT("[Aiming] Trace Start: %s, End: %s, Hit: %s"), 
			*Start.ToString(), *End.ToString(), bHit ? *Hit.ImpactPoint.ToString() : TEXT("None"));

		if (bHit)
		{
			UE_LOG(LogTemp, Warning, TEXT("[Aiming] Hit Actor: %s, Component: %s, Distance: %f"), 
				*GetNameSafe(Hit.GetActor()), *GetNameSafe(Hit.GetComponent()), Hit.Distance);
		}

		if (AimingRadius > 0.0f)
		{
			// Draw capsule representing the sweep
			DrawDebugCapsule(GetWorld(), (Start + End) * 0.5f, MaxRange * 0.5f, AimingRadius, CamRot.Quaternion(), bHit ? FColor::Green : FColor::Red, false, -1.0f);
		}
		else
		{
			DrawDebugLine(GetWorld(), Start, End, bHit ? FColor::Green : FColor::Red, false, -1.0f, 0, 1.0f);
		}
		
		if (bHit)
		{
			DrawDebugSphere(GetWorld(), Hit.ImpactPoint, 10.0f, 12, FColor::Yellow, false, -1.0f);
		}
	}

	AActor* NewTargetActor = bHit ? Hit.GetActor() : nullptr;

	if (bHit)
	{
		bHasValidTarget = true;
		CurrentTargetLocation = Hit.ImpactPoint;

		// Handle Target Change
		if (NewTargetActor != CurrentTargetActor)
		{
			// Switched targets (or acquired new one)
			CurrentTargetActor = NewTargetActor;
			OnTargetAcquired.Broadcast(CurrentTargetLocation, CurrentTargetActor);
		}
	}
	else
	{
		// Target Lost
		if (bHasValidTarget)
		{
			OnTargetLost.Broadcast();
		}

		bHasValidTarget = false;
		CurrentTargetLocation = End; 
		CurrentTargetActor = nullptr;
	}
}

void UAimingComponent::StartAiming()
{
	bIsAiming = true;
}

void UAimingComponent::StopAiming()
{
	bIsAiming = false;
	bHasValidTarget = false;
}

bool UAimingComponent::GetCurrentTarget(FVector& OutTargetLocation) const
{
	OutTargetLocation = CurrentTargetLocation;
	return bHasValidTarget;
}

FVector UAimingComponent::GetTargetLocation() const
{
	return CurrentTargetLocation;
}

FVector UAimingComponent::GetAimDirection() const
{
	APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
	if (!PC) 
	{
		return FVector::ForwardVector;
	}

	FVector CamLoc;
	FRotator CamRot;
	PC->GetPlayerViewPoint(CamLoc, CamRot);

	// If we have a valid target, aim towards it
	if (bHasValidTarget)
	{
		FVector Direction = (CurrentTargetLocation - CamLoc).GetSafeNormal();
		return Direction;
	}

	// Otherwise return camera forward
	return CamRot.Vector();
}
