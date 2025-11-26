// CharacterRope.cpp

#include "CharacterRope.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/CapsuleComponent.h"

ACharacterRope::ACharacterRope()
{
PrimaryActorTick.bCanEverTick = true;

// Rope gameplay + cosmetic components (kept minimal).
RopeRenderComponent = CreateDefaultSubobject<URopeRenderComponent>(TEXT("RopeRenderComponent"));
RopeSystem = CreateDefaultSubobject<UAC_RopeSystem>(TEXT("RopeSystem"));

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
