// RdmRopeTestCharacter.h

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "RopeRenderComponent.h"
#include "RopeSystemComponent.h"
#include "RdmRopeTestCharacter.generated.h"

/**
 * Minimal test character that auto-fires the rope on BeginPlay.
 * Drop in any map to validate the rope system without setting up inputs.
 */
UCLASS()
class LINKMEPROJECT_API ARdmRopeTestCharacter : public ACharacter
{
    GENERATED_BODY()

public:
    ARdmRopeTestCharacter();

protected:
    virtual void BeginPlay() override;

    /** Optional: automatically fire rope on BeginPlay. */
    UPROPERTY(EditAnywhere, Category="Rope|Test")
    bool bFireOnBeginPlay = true;

    /** Direction for the initial fire when bFireOnBeginPlay is true. */
    UPROPERTY(EditAnywhere, Category="Rope|Test")
    FVector InitialFireDirection = FVector(1.f, 0.f, 0.2f);

    /** Cosmetic rope rendering (client-side only). */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Rope")
    URopeRenderComponent* RopeRenderComponent;

    /** Gameplay rope brain. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Rope")
    URopeSystemComponent* RopeSystem;
};
