// CharacterRope.h

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/SpringArmComponent.h"
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

	virtual void BeginPlay() override;

	// Override pour clamp pitch
	virtual void AddControllerPitchInput(float Value) override;

public:
	virtual void Tick(float DeltaTime) override;

	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;
};
