// LinkMeGameMode.h

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "LinkMeGameMode.generated.h"

/**
 * Custom GameMode to handle Beta Test logic (Loading delays, etc.)
 */
UCLASS()
class LINKMEPROJECT_API ALinkMeGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
    ALinkMeGameMode();

    // Simplified - no delayed spawn logic needed
};
