#include "TPSAimingComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Camera/CameraComponent.h"
#include "Kismet/GameplayStatics.h"
#include "DrawDebugHelpers.h"

UTPSAimingComponent::UTPSAimingComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UTPSAimingComponent::BeginPlay()
{
	Super::BeginPlay();

	// Auto-find SpringArm and Camera if not manually set
	AActor* Owner = GetOwner();
	if (Owner && !SpringArm)
	{
		SpringArm = Owner->FindComponentByClass<USpringArmComponent>();
	}

	if (Owner && !Camera)
	{
		Camera = Owner->FindComponentByClass<UCameraComponent>();
	}

	// Store default FOV
	if (Camera)
	{
		DefaultFOV = Camera->FieldOfView;
	}

	// Initialize camera offset
	if (SpringArm)
	{
		SpringArm->SocketOffset = InitialCameraOffset;
	}
}

void UTPSAimingComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	// Call base tick (performs basic line/sphere trace)
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// Update OTS camera positioning
	UpdateOTSCamera(DeltaTime);

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

void UTPSAimingComponent::UpdateOTSCamera(float DeltaTime)
{
	if (!SpringArm || !Camera) return;

	// Camera effects only active when FOCUSING, not just aiming
	FVector TargetOffset = bIsFocusing ? AimingShoulderOffset : InitialCameraOffset;

	// Apply shoulder swap (flip Y axis)
	if (bShoulderSwapped)
	{
		TargetOffset.Y = -TargetOffset.Y;
	}

	// Interpolate camera offset
	SpringArm->SocketOffset = FMath::VInterpTo(
		SpringArm->SocketOffset,
		TargetOffset,
		DeltaTime,
		CameraOffsetTransitionSpeed
	);

	// Interpolate FOV (only when focusing)
	float TargetFOV = bIsFocusing ? AimingFOV : DefaultFOV;
	Camera->FieldOfView = FMath::FInterpTo(
		Camera->FieldOfView,
		TargetFOV,
		DeltaTime,
		FOVTransitionSpeed
	);
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

void UTPSAimingComponent::ToggleShoulderSwap()
{
	bShoulderSwapped = !bShoulderSwapped;
}

void UTPSAimingComponent::SetOwningSpringArm(USpringArmComponent* InSpringArm)
{
	SpringArm = InSpringArm;
}

void UTPSAimingComponent::SetOwningCamera(UCameraComponent* InCamera)
{
	Camera = InCamera;
	if (Camera)
	{
		DefaultFOV = Camera->FieldOfView;
	}
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
