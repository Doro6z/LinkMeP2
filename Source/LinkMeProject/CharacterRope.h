// CharacterRope.h

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "HookChargeComponent.h"

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
	class UTPSAimingComponent* AimingComponent;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Camera", meta=(ClampMin="-180.0", ClampMax="180.0"))
	float MinCameraPitch = -89.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Camera", meta=(ClampMin="-180.0", ClampMax="180.0"))
	float MaxCameraPitch = 89.0f;

	// ===================================================================
	// TPS CAMERA CONFIGURATION (for TPSAimingComponent)
	// ===================================================================

	/** Initial camera offset when not aiming (Right, Forward, Up) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="TPS Camera")
	FVector InitialCameraOffset = FVector(0, 80, 60);

	/** Shoulder offset when aiming (Right, Forward, Up) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="TPS Camera")
	FVector AimingShoulderOffset = FVector(50, 60, -20);

	/** Field of view when aiming */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="TPS Camera")
	float AimingFOV = 70.0f;

	/** Speed of FOV interpolation */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="TPS Camera")
	float FOVTransitionSpeed = 10.0f;

	/** Speed of camera offset interpolation */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="TPS Camera")
	float CameraOffsetTransitionSpeed = 10.0f;

	// ===================================================================
	// MAGNETISM CONFIGURATION (for TPSAimingComponent)
	// ===================================================================

	/** Enable target magnetism (snap to hookable targets) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="TPS Magnetism")
	bool bEnableMagnetism = true;

	/** Maximum range to detect hookable targets */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="TPS Magnetism")
	float MagnetismRange = 3000.0f;

	/** Cone angle for target detection (degrees) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="TPS Magnetism")
	float MagnetismConeAngle = 15.0f;

	/** Strength of magnetism snap (0-1) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="TPS Magnetism")
	float MagnetismStrength = 0.5f;

	/** Animation State: True when aiming/preparing to fire hook. Used by AnimBP. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Animation")
	bool bIsPreparingHook;

	UFUNCTION(BlueprintCallable, Category="Aiming")
	void StartAiming();

	UFUNCTION(BlueprintCallable, Category="Aiming")
	void StopAiming();

	/** Start Focus mode (precise aiming with camera effects). Automatically calls StartAiming. */
	UFUNCTION(BlueprintCallable, Category="Aiming")
	void StartFocus();

	/** Stop Focus mode (camera effects off, but can still be aiming) */
	UFUNCTION(BlueprintCallable, Category="Aiming")
	void StopFocus();

	/** Get the direction to fire the hook (uses aiming component) */
	UFUNCTION(BlueprintPure, Category="Aiming")
	FVector GetFireDirection() const;

	// ===================================================================
	// HOOK CHARGE SYSTEM
	// ===================================================================
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Hook Charge")
	UHookChargeComponent* HookChargeComponent;

	UFUNCTION(BlueprintCallable, Category="Hook Charge")
	void StartChargingHook();

	UFUNCTION(BlueprintCallable, Category="Hook Charge")
	void CancelHookCharge();

	/** Release the charged hook */
	UFUNCTION(BlueprintCallable, Category="Hook Charge")
	void FireChargedHook();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Hook Charge|Visualization")
	bool bShowTrajectoryWhileCharging = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Hook Charge|Visualization")
	float TrajectoryUpdateFrequency = 0.05f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Hook Charge|Visualization")
	FLinearColor TrajectoryColorNormal = FLinearColor::Yellow;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Hook Charge|Visualization")
	FLinearColor TrajectoryColorPerfect = FLinearColor::Green;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Hook Charge|Visualization")
	FLinearColor TrajectoryColorUnreachable = FLinearColor::Red;

	// Reticle for Focus Mode
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Hook Charge|Visualization")
	TSubclassOf<AActor> FocusReticleClass;

protected:
	void UpdateTrajectoryVisualization(float DeltaTime);
	void UpdateFocusReticle();
	FVector GetProjectileStartLocation() const;

	UPROPERTY()
	AActor* FocusReticleInstance = nullptr;

	float TimeSinceLastTrajectoryUpdate = 0.0f;

protected:
	virtual void BeginPlay() override;

// Override pour clamp pitch
virtual void AddControllerPitchInput(float Value) override;

public:
virtual void Tick(float DeltaTime) override;

virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;
};
