#pragma once

#include "CoreMinimal.h"
#include "AimingComponent.h"
#include "TPSAimingComponent.generated.h"

class USpringArmComponent;
class UCameraComponent;

/**
 * TPS Aiming Component with Over-The-Shoulder camera and target magnetism.
 * Designed for ergonomic third-person grappling hook gameplay.
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class LINKMEPROJECT_API UTPSAimingComponent : public UAimingComponent
{
	GENERATED_BODY()

public:
	UTPSAimingComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void BeginPlay() override;

	// Override aiming API to return magnetized targets
	virtual FVector GetTargetLocation() const override;
	virtual FVector GetAimDirection() const override;

	/** Toggle between left and right shoulder */
	UFUNCTION(BlueprintCallable, Category = "TPS Aiming")
	void ToggleShoulderSwap();

	/** Start Focus mode (activates camera effects) */
	UFUNCTION(BlueprintCallable, Category = "TPS Aiming")
	void StartFocus();

	/** Stop Focus mode (deactivates camera effects) */
	UFUNCTION(BlueprintCallable, Category = "TPS Aiming")
	void StopFocus();

	/** Returns true if currently focusing (camera effects active) */
	UFUNCTION(BlueprintPure, Category = "TPS Aiming")
	bool IsFocusing() const { return bIsFocusing; }

	/** Set the SpringArm to control for camera offset */
	void SetOwningSpringArm(USpringArmComponent* InSpringArm);

	/** Set the Camera component to control FOV */
	void SetOwningCamera(UCameraComponent* InCamera);

	// ===================================================================
	// OTS CAMERA CONFIGURATION
	// ===================================================================

	/** Initial camera offset when not aiming (Right, Forward, Up) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TPS Camera")
	FVector InitialCameraOffset = FVector(0, 80, 60);

	/** Shoulder offset when aiming (Right, Forward, Up) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TPS Camera")
	FVector AimingShoulderOffset = FVector(50, 60, -20);

	/** Field of view when aiming */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TPS Camera", meta=(ClampMin="30", ClampMax="120"))
	float AimingFOV = 70.0f;

	/** Speed of FOV interpolation */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TPS Camera")
	float FOVTransitionSpeed = 10.0f;

	/** Speed of camera offset interpolation */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TPS Camera")
	float CameraOffsetTransitionSpeed = 10.0f;

	// ===================================================================
	// MAGNETISM CONFIGURATION
	// ===================================================================

	/** Enable target magnetism (snap to hookable targets) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TPS Magnetism")
	bool bEnableMagnetism = true;

	/** Maximum range to detect hookable targets */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TPS Magnetism")
	float MagnetismRange = 3000.0f;

	/** Cone angle for target detection (degrees) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TPS Magnetism", meta=(ClampMin="1", ClampMax="90"))
	float MagnetismConeAngle = 15.0f;

	/** Strength of magnetism snap (0-1) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TPS Magnetism", meta=(ClampMin="0", ClampMax="1"))
	float MagnetismStrength = 0.5f;

	/** Tag used to identify hookable actors */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TPS Magnetism")
	FName HookableTag = TEXT("Hookable");

protected:
	void UpdateOTSCamera(float DeltaTime);
	void UpdateMagnetism(float DeltaTime);
	AActor* FindBestMagnetismTarget(const FVector& CamLoc, const FVector& CamForward) const;

protected:
	UPROPERTY()
	USpringArmComponent* SpringArm = nullptr;

	UPROPERTY()
	UCameraComponent* Camera = nullptr;

	// Magnetism state
	FVector MagnetizedTargetLocation;
	bool bHasMagnetizedTarget = false;

	UPROPERTY()
	AActor* CurrentMagnetizedActor = nullptr;

	// Camera state
	float DefaultFOV = 90.0f;
	bool bShoulderSwapped = false;
	bool bIsFocusing = false;
};
