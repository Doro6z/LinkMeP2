#include "AimingComponent.h"
#include "GameFramework/Actor.h"
#include "GameFramework/PlayerController.h"
#include "Engine/World.h"
#include "Camera/CameraComponent.h"
#include "Kismet/GameplayStatics.h"

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
		bHasValidTarget = false;
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

	if (GetWorld()->LineTraceSingleByChannel(Hit, Start, End, AimingTraceChannel, Params))
	{
		bHasValidTarget = true;
		CurrentTargetLocation = Hit.ImpactPoint;
	}
	else
	{
		bHasValidTarget = false;
		CurrentTargetLocation = End; // Or zero, depending on UI needs
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
