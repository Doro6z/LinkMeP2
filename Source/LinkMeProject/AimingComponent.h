#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "AimingComponent.generated.h"

UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class LINKMEPROJECT_API UAimingComponent : public UActorComponent
{
	GENERATED_BODY()

public:	
	UAimingComponent();

protected:
	virtual void BeginPlay() override;

public:	
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/** Starts the aiming mode (zooms camera, shows reticle) */
	UFUNCTION(BlueprintCallable, Category = "Aiming")
	void StartAiming();

	/** Stops the aiming mode */
	UFUNCTION(BlueprintCallable, Category = "Aiming")
	void StopAiming();

	/** Returns true if currently aiming */
	UFUNCTION(BlueprintPure, Category = "Aiming")
	bool IsAiming() const { return bIsAiming; }

	/** Returns the current valid target location (if any) */
	UFUNCTION(BlueprintPure, Category = "Aiming")
	bool GetCurrentTarget(FVector& OutTargetLocation) const;

	/** Returns the target location for aiming. Virtual to allow derived classes to apply magnetism, etc. */
	UFUNCTION(BlueprintPure, Category = "Aiming")
	virtual FVector GetTargetLocation() const;

	/** Returns the direction to aim/fire. Virtual to allow derived classes to modify aiming behavior. */
	UFUNCTION(BlueprintPure, Category = "Aiming")
	virtual FVector GetAimDirection() const;

	/** Max distance for the grapple hook */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aiming")
	float MaxRange = 2000.0f;

	/** Radius for the aiming trace (SphereTrace). Set > 0 to enable. Makes aiming easier. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aiming")
	float AimingRadius = 0.0f;

	/** If true, enables Verbose Logs and Visual Debugging (Lines/Spheres) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug")
	bool bShowDebug = false;

	/** Event broadcast when a valid target is acquired */
	DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnTargetAcquired, const FVector&, Location, AActor*, TargetActor);
	UPROPERTY(BlueprintAssignable, Category = "Aiming")
	FOnTargetAcquired OnTargetAcquired;

	/** Event broadcast when the target is lost */
	DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnTargetLost);
	UPROPERTY(BlueprintAssignable, Category = "Aiming")
	FOnTargetLost OnTargetLost;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Aiming")
	bool bIsAiming;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Aiming")
	bool bHasValidTarget;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Aiming")
	FVector CurrentTargetLocation;

	/** Trace channel to use for aiming checks */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aiming")
	TEnumAsByte<ECollisionChannel> AimingTraceChannel = ECC_Visibility;

	// Internal tracker to prevent spamming delegates
	UPROPERTY()
	AActor* CurrentTargetActor;
};
