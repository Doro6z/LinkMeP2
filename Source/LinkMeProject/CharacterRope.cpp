// CharacterRope.cpp

#include "CharacterRope.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/CapsuleComponent.h"

ACharacterRope::ACharacterRope()
{
	PrimaryActorTick.bCanEverTick = true;

	// ---------------------------------------------------------------------
	// RENDER + GAMEPLAY ROPE COMPONENTS
	// ---------------------------------------------------------------------
	RopeRenderComponent = CreateDefaultSubobject<URopeRenderComponent>(TEXT("RopeRenderComponent"));
	RopeSystem = CreateDefaultSubobject<UAC_RopeSystem>(TEXT("RopeSystem"));

// ---------------------------------------------------------------------
// SPRING ARM — caméra orbitale usuelle
// ---------------------------------------------------------------------
	SpringArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("SpringArm"));
	SpringArm->SetupAttachment(GetCapsuleComponent());

	SpringArm->TargetArmLength = 350.f;
	SpringArm->bUsePawnControlRotation = true;   // orbit avec souris
	SpringArm->bEnableCameraLag = true;
	SpringArm->CameraLagSpeed = 8.f;

	// Clamp de base du SpringArm (évite des rotations extrêmes)
	SpringArm->bEnableCameraRotationLag = false;
	SpringArm->bInheritPitch = true;
	SpringArm->bInheritYaw = true;
	SpringArm->bInheritRoll = false;

	// ---------------------------------------------------------------------
	// CAMERA
	// ---------------------------------------------------------------------
	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(SpringArm);
	Camera->bUsePawnControlRotation = false;

	// ---------------------------------------------------------------------
	// DÉCORRÉLER PLAYER ↔ CAMÉRA
	// ---------------------------------------------------------------------
	bUseControllerRotationYaw = false;
	bUseControllerRotationPitch = false;
	bUseControllerRotationRoll = false;

	// Rotation personnage dépend de la direction du mouvement
	GetCharacterMovement()->bOrientRotationToMovement = true;
	GetCharacterMovement()->RotationRate = FRotator(0.0f, 540.0f, 0.0f);
}

void ACharacterRope::BeginPlay()
{
	Super::BeginPlay();
}

void ACharacterRope::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
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

	// Clamp sur le Pitch (important : Unreal pitch inversé)
	// Pitch descend vers le bas → valeurs négatives
	ControlRot.Pitch = FMath::ClampAngle(ControlRot.Pitch, -65.f, -10.f);

	Controller->SetControlRotation(ControlRot);
}

void ACharacterRope::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	// Exemple d’inputs (à adapter si tu utilises Enhanced Input)
	// PlayerInputComponent->BindAxis("Turn", this, &APawn::AddControllerYawInput);
	// PlayerInputComponent->BindAxis("LookUp", this, &ACharacterRope::AddControllerPitchInput);
}
