#pragma once

#include "CoreMinimal.h"
#include "AimingComponent.h"
#include "TPSAimingComponent.generated.h"

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


	/** Start Focus mode (activates camera effects) */
	UFUNCTION(BlueprintCallable, Category = "TPS Aiming")
	void StartFocus();

	/** Stop Focus mode (deactivates camera effects) */
	UFUNCTION(BlueprintCallable, Category = "TPS Aiming")
	void StopFocus();

	/** Returns true if currently focusing (camera effects active) */
	UFUNCTION(BlueprintPure, Category = "TPS Aiming")
	bool IsFocusing() const { return bIsFocusing; }

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
	void UpdateMagnetism(float DeltaTime);
	AActor* FindBestMagnetismTarget(const FVector& CamLoc, const FVector& CamForward) const;

	// Magnetism state
	FVector MagnetizedTargetLocation;
	bool bHasMagnetizedTarget = false;

	UPROPERTY()
	AActor* CurrentMagnetizedActor = nullptr;

	// Focus state
	bool bIsFocusing = false;
};
