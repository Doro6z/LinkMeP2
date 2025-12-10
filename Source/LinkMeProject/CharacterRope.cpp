// CharacterRope.cpp

#include "CharacterRope.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/CapsuleComponent.h"

#include "AimingComponent.h"
#include "TPSAimingComponent.h"

ACharacterRope::ACharacterRope()
{
	PrimaryActorTick.bCanEverTick = true;

	// Create TPS Aiming Component
	AimingComponent = CreateDefaultSubobject<UTPSAimingComponent>(TEXT("AimingComponent"));
	
	// Create Hook Charge Component
	HookChargeComponent = CreateDefaultSubobject<UHookChargeComponent>(TEXT("HookChargeComponent"));


	// Orbit camera setup.
	SpringArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("SpringArm"));
	SpringArm->SetupAttachment(GetCapsuleComponent());
	SpringArm->TargetArmLength = 400.f;
	SpringArm->bUsePawnControlRotation = true;
	SpringArm->bEnableCameraLag = true;
	SpringArm->CameraLagSpeed = 8.f;
	SpringArm->bInheritPitch = true;
	SpringArm->bInheritYaw = true;
	SpringArm->bInheritRoll = false;

	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(SpringArm, USpringArmComponent::SocketName);
	Camera->bUsePawnControlRotation = false;

	// Character orientation driven by movement (typical third-person setup).
	bUseControllerRotationYaw = false;
	bUseControllerRotationPitch = false;
	bUseControllerRotationRoll = false;
	GetCharacterMovement()->bOrientRotationToMovement = true;
	GetCharacterMovement()->RotationRate = FRotator(0.0f, 540.0f, 0.0f);

	// Position the mesh like the default UE third-person template so it stays visible.
	GetMesh()->SetRelativeLocation(FVector(0.0f, 0.0f, -90.0f));
	GetMesh()->SetRelativeRotation(FRotator(0.0f, -90.0f, 0.0f));
}

void ACharacterRope::BeginPlay()
{
	Super::BeginPlay();
	
	// Configure TPS Aiming Component
	if (AimingComponent)
	{
		AimingComponent->SetOwningSpringArm(SpringArm);
		AimingComponent->SetOwningCamera(Camera);
		
		// Transfer configuration from Character to Component
		AimingComponent->InitialCameraOffset = InitialCameraOffset;
		AimingComponent->AimingShoulderOffset = AimingShoulderOffset;
		AimingComponent->AimingFOV = AimingFOV;
		AimingComponent->FOVTransitionSpeed = FOVTransitionSpeed;
		AimingComponent->CameraOffsetTransitionSpeed = CameraOffsetTransitionSpeed;
		AimingComponent->bEnableMagnetism = bEnableMagnetism;
		AimingComponent->MagnetismRange = MagnetismRange;
		AimingComponent->MagnetismConeAngle = MagnetismConeAngle;
		AimingComponent->MagnetismStrength = MagnetismStrength;
	}
}

#include "Kismet/GameplayStatics.h"

void ACharacterRope::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// NOTE: Camera offset logic is now handled by TPSAimingComponent
	// No need to manually interpolate SpringArm->SocketOffset here

	// Hook Charge Visualization (Trajectory & Reticle)
	// Only show when actively charging (State == Charging)
	if (HookChargeComponent && HookChargeComponent->IsCharging())
	{
		// Update Trajectory
		if (bShowTrajectoryWhileCharging)
		{
			TimeSinceLastTrajectoryUpdate += DeltaTime;
			if (TimeSinceLastTrajectoryUpdate >= TrajectoryUpdateFrequency)
			{
				UpdateTrajectoryVisualization(DeltaTime);
				TimeSinceLastTrajectoryUpdate = 0.0f;
			}
		}

		// Update Reticle
		UpdateFocusReticle();
	}
	else if (HookChargeComponent && !HookChargeComponent->IsCharging())
	{
		// Ensure Reticle is hidden if not charging
		if (FocusReticleInstance && !FocusReticleInstance->IsHidden())
		{
			FocusReticleInstance->SetActorHiddenInGame(true);
		}
	}

	// Legacy Trajectory (Fallback if not charging but aiming? 
	// Actually, we want trajectory primarily during CHARGE now as per request.
	// "Lorsqu'il charge, on voit la trajectoire que suivra le lancé s'augmenter."
	// So we disable the old aiming-only static trajectory.)
}

void ACharacterRope::StartAiming()
{
	if (AimingComponent)
	{
		AimingComponent->StartAiming();
	}
	
	bIsPreparingHook = true;
	
	// Orient character to camera view
	bUseControllerRotationYaw = true;
	GetCharacterMovement()->bOrientRotationToMovement = false;
}

void ACharacterRope::StopAiming()
{
	if (AimingComponent)
	{
		AimingComponent->StopAiming();
	}

	bIsPreparingHook = false;

	// Restore movement orientation
	bUseControllerRotationYaw = false;
	GetCharacterMovement()->bOrientRotationToMovement = true;
}

FVector ACharacterRope::GetFireDirection() const
{
	if (AimingComponent)
	{
		return AimingComponent->GetAimDirection();
	}
	
	// Fallback: use controller rotation
	return GetControlRotation().Vector();
}

void ACharacterRope::StartFocus()
{
	if (UTPSAimingComponent* TPSComp = Cast<UTPSAimingComponent>(AimingComponent))
	{
		TPSComp->StartFocus();
	}
}

void ACharacterRope::StopFocus()
{
	if (UTPSAimingComponent* TPSComp = Cast<UTPSAimingComponent>(AimingComponent))
	{
		TPSComp->StopFocus();
	}
}

// ---------------------------------------------------------------------
// CLAMP DU PITCH : caméra légèrement top-down
//
// Valeurs recommandées :
// -65° = vue top-down douce
// -10° = permet un léger tilt vers le haut
// ---------------------------------------------------------------------
void ACharacterRope::AddControllerPitchInput(float Value)
{
	Super::AddControllerPitchInput(Value);

	if (!Controller) return;

	FRotator ControlRot = Controller->GetControlRotation();

	// Clamp sur le Pitch avec les variables exposées
	// Pitch descend vers le bas -> valeurs négatives
	// Note: Unreal's control rotation pitch is often normalized to 0-360 or -180/180 depending on context,
	// but standard ClampAngle handles the wrapping correctly if we treat it as degrees.
	// However, for standard 3rd person, we usually want to clamp between a negative (looking down) and positive (looking up).
	
	// Normalize pitch to -180 to 180 range for easier clamping logic if needed, 
	// but FMath::ClampAngle handles it.
	// Let's ensure we use the user defined limits.
	
	// Important: Unreal's Pitch is inverted in some contexts (0 is horizon, -90 is up, 90 is down? No, usually 90 is up, -90 is down).
	// Let's stick to the standard behavior:
	// -90 (looking straight down) to 90 (looking straight up).
	// The previous code had -125 to -10 which was very restrictive and weirdly offset.
	
	// We will just let the base class handle the input, and then clamp the result.
	// Actually, PlayerController usually handles the clamping via ViewPitchMin/Max.
	// But since we are overriding it here, let's apply our own clamp.
	
	ControlRot.Pitch = FMath::ClampAngle(ControlRot.Pitch, MinCameraPitch, MaxCameraPitch);

	Controller->SetControlRotation(ControlRot);
}

// ===================================================================
// HOOK CHARGE SYSTEM & VISUALIZATION
// ===================================================================

void ACharacterRope::StartChargingHook()
{
	if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Cyan, TEXT("[INPUT] StartChargingHook Pressed"));
	
	if (!HookChargeComponent || !AimingComponent) return;

	UTPSAimingComponent* TPSComp = Cast<UTPSAimingComponent>(AimingComponent);
	bool bFocus = TPSComp ? TPSComp->IsFocusing() : false;
	FVector StartLoc = GetProjectileStartLocation();
	FVector TargetLoc = AimingComponent->GetTargetLocation();

	HookChargeComponent->StartCharging(bFocus, TargetLoc, StartLoc);
}

void ACharacterRope::CancelHookCharge()
{
	if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Orange, TEXT("[INPUT] CancelHookCharge Called"));

	if (HookChargeComponent)
	{
		HookChargeComponent->CancelCharging();
	}
	
	if (FocusReticleInstance)
	{
		FocusReticleInstance->SetActorHiddenInGame(true);
	}
}

void ACharacterRope::FireChargedHook()
{
	if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Cyan, TEXT("[INPUT] FireChargedHook Released"));
	UE_LOG(LogTemp, Warning, TEXT("ACharacterRope::FireChargedHook called"));

	if (!HookChargeComponent || !AimingComponent) 
	{
		if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Red, TEXT("ERROR: Missing Components!"));
		UE_LOG(LogTemp, Error, TEXT("FireChargedHook: Missing Component"));
		return;
	}

	// Initialize to Zero
	FVector OutVelocity = FVector::ZeroVector;
	bool bValid = HookChargeComponent->StopChargingAndGetVelocity(OutVelocity);
	
	UE_LOG(LogTemp, Warning, TEXT("FireChargedHook: StopCharging returned %s, Velocity: %s"), 
		bValid ? TEXT("TRUE") : TEXT("FALSE"), *OutVelocity.ToString());

	// Hide reticle
	if (FocusReticleInstance) FocusReticleInstance->SetActorHiddenInGame(true);

	if (bValid)
	{
		// Si le composant n'a pas pu calculer de vélocité (ex: Mode Manuel), on le fait ici.
		// On utilise SizeSquared pour éviter les problèmes de précision (IsNearlyZero)
		if (OutVelocity.SizeSquared() < 1000.0f) 
		{
			// Fallback manuel : Direction caméra * Vitesse de charge
			float Speed = HookChargeComponent->GetCurrentLaunchSpeed();
			FVector FireDir = GetFireDirection();
			
			UE_LOG(LogTemp, Warning, TEXT("FireChargedHook: Using Fallback Manual Fire. Dir: %s, Speed: %f"), *FireDir.ToString(), Speed);
			
			if (FireDir.IsZero())
			{
				// Emergency Fallback: Actor Forward
				FireDir = GetActorForwardVector();
				UE_LOG(LogTemp, Error, TEXT("FireChargedHook: FireDirection was ZERO! Using Actor Forward."));
				if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Red, TEXT("ERROR: FireDirection is ZERO!"));
			}
			
			OutVelocity = FireDir * Speed;
		}
		
		UE_LOG(LogTemp, Warning, TEXT("FireChargedHook: Final Velocity to Fire: %s"), *OutVelocity.ToString());
		
		// Fire!
		
		// Fire!
		if (URopeSystemComponent* RopeSys = FindComponentByClass<URopeSystemComponent>())
		{
			if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Green, TEXT("Calling RopeSys->FireChargedHook..."));
			UE_LOG(LogTemp, Warning, TEXT("FireChargedHook: Calling RopeSystem->FireChargedHook"));
			RopeSys->FireChargedHook(OutVelocity);
		}
		else
		{
			if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Red, TEXT("ERROR: RopeSystemComponent Not Found!"));
			UE_LOG(LogTemp, Error, TEXT("FireChargedHook: RopeSystemComponent NOT FOUND on Character"));
		}
	}
	else
	{
		if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Red, FString::Printf(TEXT("Charge Too Low: %f < %f"), HookChargeComponent->GetChargeRatio(), HookChargeComponent->MinChargeThreshold));
		UE_LOG(LogTemp, Warning, TEXT("FireChargedHook: Charge was invalid/too low"));
	}
	
	bIsPreparingHook = false; // Reset Anim state
}

FVector ACharacterRope::GetProjectileStartLocation() const
{
	if (GetMesh()->DoesSocketExist(TEXT("hand_r")))
	{
		return GetMesh()->GetSocketLocation(TEXT("hand_r"));
	}
	return GetActorLocation();
}

void ACharacterRope::UpdateTrajectoryVisualization(float DeltaTime)
{
	if (!HookChargeComponent || !AimingComponent) return;

	// Determine Velocity to visualize
	float CurrentSpeed = HookChargeComponent->GetCurrentLaunchSpeed();
	FVector LaunchVelocity = GetFireDirection() * CurrentSpeed;
	
	// Si Focus mode et Reachable, on pourrait vouloir visualiser la trajectoire "idéale" calculée par l'algo
	// Mais HookChargeComponent ne stocke pas le vecteur vélocité idéal publiquement pour l'instant..
	// Pour l'instant, visualiser GetFireDirection() * CurrentSpeed est une bonne approximation
	// car en Aim mode, la caméra regarde vers la cible.
	
	FVector StartLoc = GetProjectileStartLocation();

	// Configurer prédiction
	FPredictProjectilePathParams PredictParams;
	PredictParams.StartLocation = StartLoc;
	PredictParams.LaunchVelocity = LaunchVelocity;
	PredictParams.bTraceWithCollision = true;
	PredictParams.bTraceComplex = false;
	PredictParams.ProjectileRadius = 5.0f;
	PredictParams.MaxSimTime = 3.0f;
	PredictParams.SimFrequency = 15.0f; 
	PredictParams.TraceChannel = ECC_WorldStatic; // Should use HookChargeComponent->ProjectileTraceChannel
	PredictParams.ActorsToIgnore.Add(this);

	// Couleur selon état
	FLinearColor TraceColor = TrajectoryColorNormal;

	if (UTPSAimingComponent* TPSComp = Cast<UTPSAimingComponent>(AimingComponent))
	{
		if (TPSComp->IsFocusing())
		{
			if (HookChargeComponent->IsTargetReachable() == false)
			{
				TraceColor = TrajectoryColorUnreachable;
			}
			else if (HookChargeComponent->IsChargePerfect())
			{
				TraceColor = TrajectoryColorPerfect;
			}
		}
	}

	// Disable built-in debug drawing to handle color manually
	PredictParams.DrawDebugType = EDrawDebugTrace::None;

	FPredictProjectilePathResult PredictResult;
	if (UGameplayStatics::PredictProjectilePath(GetWorld(), PredictParams, PredictResult))
	{
		// Draw trajectory manually
		for (int32 i = 0; i < PredictResult.PathData.Num() - 1; ++i)
		{
			DrawDebugLine(
				GetWorld(), 
				PredictResult.PathData[i].Location, 
				PredictResult.PathData[i+1].Location, 
				TraceColor.ToFColor(true), 
				false, 
				TrajectoryUpdateFrequency + 0.02f, 
				0, 
				3.0f // Thickness
			);
		}
		
		// Draw Impact point if hit
		if (PredictResult.HitResult.bBlockingHit)
		{
			DrawDebugSphere(
				GetWorld(),
				PredictResult.HitResult.ImpactPoint,
				10.0f,
				12,
				TraceColor.ToFColor(true),
				false,
				TrajectoryUpdateFrequency + 0.02f
			);
		}
	}
}

void ACharacterRope::UpdateFocusReticle()
{
	UTPSAimingComponent* TPSComp = Cast<UTPSAimingComponent>(AimingComponent);
	if (!TPSComp || !TPSComp->IsFocusing())
	{
		if (FocusReticleInstance) FocusReticleInstance->SetActorHiddenInGame(true);
		return;
	}

	// Obtenir position cible depuis l'aiming component
	// (Note: C'est le point visé par le rayon, pas forcément le point d'impact du projectile)
	FVector TargetLoc = TPSComp->GetTargetLocation();

	// Spawn reticle if needed
	if (!FocusReticleInstance && FocusReticleClass)
	{
		FActorSpawnParameters SpawnParams;
		SpawnParams.Owner = this;
		FocusReticleInstance = GetWorld()->SpawnActor<AActor>(FocusReticleClass, TargetLoc, FRotator::ZeroRotator, SpawnParams);
	}

	if (FocusReticleInstance)
	{
		FocusReticleInstance->SetActorHiddenInGame(false);
		FocusReticleInstance->SetActorLocation(TargetLoc);
		
		// Ici on pourrait ajouter une logique pour le scale/pulse si ChargePerfect
		// ex: FocusReticleInstance->SetActorScale3D(...)
	}
}

void ACharacterRope::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	// Bindings - Assurez-vous d'avoir les Action mappings dans l'éditeur ou EnhancedInput !
	// PlayerInputComponent->BindAction("Fire", IE_Pressed, this, &ACharacterRope::StartChargingHook);
	// PlayerInputComponent->BindAction("Fire", IE_Released, this, &ACharacterRope::FireChargedHook);
	// PlayerInputComponent->BindAction("Focus", IE_Pressed, this, &ACharacterRope::StartFocus);
	// PlayerInputComponent->BindAction("Focus", IE_Released, this, &ACharacterRope::StopFocus);
}
