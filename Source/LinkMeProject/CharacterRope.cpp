// CharacterRope.cpp

#include "CharacterRope.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/CapsuleComponent.h"

#include "AimingComponent.h"

ACharacterRope::ACharacterRope()
{
	PrimaryActorTick.bCanEverTick = true;



	
	AimingComponent = CreateDefaultSubobject<UAimingComponent>(TEXT("AimingComponent"));

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
	
	// Initialize Default Offset
	if (SpringArm)
	{
		DefaultCameraOffset = SpringArm->SocketOffset;
	}
}

#include "Kismet/GameplayStatics.h"

void ACharacterRope::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// Camera Transition Logic
	if (AimingComponent && SpringArm)
	{
		FVector TargetOffset = AimingComponent->IsAiming() ? AimingCameraOffset : DefaultCameraOffset;
		SpringArm->SocketOffset = FMath::VInterpTo(SpringArm->SocketOffset, TargetOffset, DeltaTime, CameraTransitionSpeed);
	}

	// Trajectory Visualization
	if (bIsPreparingHook && AimingComponent)
	{
		FPredictProjectilePathParams PredictParams;
		PredictParams.StartLocation = GetMesh()->GetSocketLocation(TEXT("hand_r")); // Approximate hand pos
		if (PredictParams.StartLocation.IsZero()) PredictParams.StartLocation = GetActorLocation();
		
		PredictParams.LaunchVelocity = GetControlRotation().Vector() * 2000.f; // Arbitrary speed for vis
		PredictParams.MaxSimTime = 2.0f;
		PredictParams.ProjectileRadius = 5.0f;
		PredictParams.bTraceWithCollision = true;
		PredictParams.bTraceComplex = false;
		PredictParams.DrawDebugType = EDrawDebugTrace::ForOneFrame;
		PredictParams.DrawDebugTime = 0.0f;
		PredictParams.SimFrequency = 15.0f;
		PredictParams.ObjectTypes.Add(UEngineTypes::ConvertToObjectType(ECC_WorldStatic));

		FPredictProjectilePathResult PredictResult;
		UGameplayStatics::PredictProjectilePath(GetWorld(), PredictParams, PredictResult);
	}
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

void ACharacterRope::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
Super::SetupPlayerInputComponent(PlayerInputComponent);

// Exemple d’inputs (à adapter si tu utilises Enhanced Input)
// PlayerInputComponent->BindAxis("Turn", this, &APawn::AddControllerYawInput);
// PlayerInputComponent->BindAxis("LookUp", this, &ACharacterRope::AddControllerPitchInput);
}
