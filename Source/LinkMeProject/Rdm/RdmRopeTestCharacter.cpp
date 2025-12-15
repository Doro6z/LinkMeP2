// RdmRopeTestCharacter.cpp

#include "Rdm/RdmRopeTestCharacter.h"

ARdmRopeTestCharacter::ARdmRopeTestCharacter()
{
    PrimaryActorTick.bCanEverTick = false;

    // RopeRenderComponent = CreateDefaultSubobject<URopeRenderComponent>(TEXT("RopeRender"));
    // Removed to allow Blueprint-based Render Component (BPC_RopeRender) implementation.


    RopeSystem = CreateDefaultSubobject<URopeSystemComponent>(TEXT("RopeSystem"));
}

void ARdmRopeTestCharacter::BeginPlay()
{
    Super::BeginPlay();

    if (bFireOnBeginPlay && RopeSystem)
    {
        const FVector FireDir = InitialFireDirection.GetSafeNormal(UE_SMALL_NUMBER);
        if (!FireDir.IsNearlyZero())
        {
            RopeSystem->FireHook(FireDir);
        }
    }
}
