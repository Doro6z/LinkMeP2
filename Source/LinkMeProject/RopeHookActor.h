// RopeHookActor.h

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RopeHookActor.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnHookImpactSignature, const FHitResult&, ImpactResult);

UCLASS()
class LINKMEPROJECT_API ARopeHookActor : public AActor
{
GENERATED_BODY()

public:
ARopeHookActor();

virtual void Tick(float DeltaTime) override;

/** Fire hook forward with an impulse. */
void Fire(const FVector& Direction);

/** Has the hook impacted something? */
bool HasImpacted() const { return bImpacted; }

/** Latest impact result. */
const FHitResult& GetImpactResult() const { return ImpactResult; }

/** Broadcast when the hook hits an obstacle. */
UPROPERTY(BlueprintAssignable, Category="Rope")
FOnHookImpactSignature OnHookImpact;

protected:
virtual void BeginPlay() override;

UFUNCTION()
void HandleHookImpact(UPrimitiveComponent* HitComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit);

protected:
UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Rope")
UStaticMeshComponent* HookMesh;

UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Rope")
UPrimitiveComponent* CollisionComponent;

UPROPERTY(EditAnywhere, Category="Rope")
float LaunchImpulse = 3500.f;

bool bImpacted = false;
FHitResult ImpactResult;
};
