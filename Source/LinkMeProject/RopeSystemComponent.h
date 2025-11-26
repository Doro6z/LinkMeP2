// RopeSystemComponent.h

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "RopeSystemComponent.generated.h"

class ARopeHookActor;
class URopeRenderComponent;

UENUM(BlueprintType)
enum class ERopeState : uint8
{
Idle,
Flying,
Attached
};

/**
 * Gameplay rope brain: manages bend points, length, forces, and hook lifecycle.
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class LINKMEPROJECT_API UAC_RopeSystem : public UActorComponent
{
GENERATED_BODY()

public:
UAC_RopeSystem();

virtual void BeginPlay() override;
virtual void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

/** Launch a grapple in the given direction. */
UFUNCTION(BlueprintCallable, Category="Rope")
void FireHook(const FVector& Direction);

/** Sever current rope. */
UFUNCTION(BlueprintCallable, Category="Rope")
void Sever();

/** Returns gameplay bend points (anchor to owner). */
const TArray<FVector>& GetBendPoints() const { return BendPoints; }

protected:
void ManageBendPoints();
void ManageRopeLength(float DeltaTime);
void ApplyForcesToPlayer();
void UpdateRopeVisual();
bool LineTrace(const FVector& Start, const FVector& End, FHitResult& OutHit) const;
    UFUNCTION()
    void OnHookImpact(const FHitResult& Hit);
void TransitionToAttached(const FHitResult& Hit);

protected:
UPROPERTY(EditAnywhere, Category="Rope|Setup")
TSubclassOf<ARopeHookActor> HookClass;

UPROPERTY(EditAnywhere, Category="Rope|Setup")
float MaxLength = 1500.f;

UPROPERTY(EditAnywhere, Category="Rope|Setup")
float BendOffset = 5.f;

UPROPERTY(EditAnywhere, Category="Rope|Force")
float SpringStiffness = 1600.f;

UPROPERTY(EditAnywhere, Category="Rope|Force")
float SwingTorque = 40000.f;

UPROPERTY(VisibleAnywhere, Category="Rope|State")
float CurrentLength = 0.f;

UPROPERTY(VisibleAnywhere, Category="Rope|State")
TArray<FVector> BendPoints;

UPROPERTY(VisibleAnywhere, Category="Rope|State")
ERopeState RopeState = ERopeState::Idle;

UPROPERTY(Transient)
ARopeHookActor* CurrentHook = nullptr;

UPROPERTY(Transient)
URopeRenderComponent* RenderComponent = nullptr;
};
