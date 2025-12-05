// CharacterRope.h

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/SpringArmComponent.h"

#include "RopeSystemComponent.h"

#include "CharacterRope.generated.h"

	UCLASS()
class LINKMEPROJECT_API ACharacterRope : public ACharacter
{
GENERATED_BODY()

public:
ACharacterRope();

protected:
UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Camera")
USpringArmComponent* SpringArm;

UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Camera")
UCameraComponent* Camera;


		
	/** Aiming Component for target detection and state */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Aiming")
	class UAimingComponent* AimingComponent;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Camera", meta=(ClampMin="-180.0", ClampMax="180.0"))
	float MinCameraPitch = -89.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Camera", meta=(ClampMin="-180.0", ClampMax="180.0"))
	float MaxCameraPitch = 89.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Camera")
	FVector DefaultCameraOffset = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Camera")
	FVector AimingCameraOffset = FVector(0.f, 50.f, 30.f); // Right and Up shift

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Camera")
	float CameraTransitionSpeed = 10.0f;

	/** Animation State: True when aiming/preparing to fire hook. Used by AnimBP. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Animation")
	bool bIsPreparingHook;

	UFUNCTION(BlueprintCallable, Category="Aiming")
	void StartAiming();

	UFUNCTION(BlueprintCallable, Category="Aiming")
	void StopAiming();

protected:
virtual void BeginPlay() override;

// Override pour clamp pitch
virtual void AddControllerPitchInput(float Value) override;

public:
virtual void Tick(float DeltaTime) override;

virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;
};
