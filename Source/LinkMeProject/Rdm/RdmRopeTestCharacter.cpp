// RdmRopeTestCharacter.cpp

#include "Rdm/RdmRopeTestCharacter.h"

ARdmRopeTestCharacter::ARdmRopeTestCharacter()
{
    PrimaryActorTick.bCanEverTick = false;

    RopeRenderComponent = CreateDefaultSubobject<URopeRenderComponent>(TEXT("RopeRender"));


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
