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

	/** Max distance for the grapple hook */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aiming")
	float MaxRange = 2000.0f;

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
};
