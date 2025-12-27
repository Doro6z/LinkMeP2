// RopeHookActor.h

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "RopeHookActor.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnHookImpactSignature,
                                            const FHitResult &, ImpactResult);

class USphereComponent;
class UStaticMeshComponent;
class UProjectileMovementComponent;

UCLASS()
class LINKMEPROJECT_API ARopeHookActor : public AActor {
  GENERATED_BODY()

public:
  ARopeHookActor();

  virtual void Tick(float DeltaTime) override;

  /** Fire hook forward with an impulse. */
  UFUNCTION(BlueprintCallable, Category = "Rope")
  void Fire(const FVector &Direction);

  /** Fire hook with a specific velocity vector. */
  UFUNCTION(BlueprintCallable, Category = "Rope")
  void FireVelocity(const FVector &Velocity);

  /** Has the hook impacted something? */
  bool HasImpacted() const { return bImpacted; }

  /** Latest impact result. */
  const FHitResult &GetImpactResult() const { return ImpactResult; }

  /** Broadcast when the hook hits an obstacle. */
  UPROPERTY(BlueprintAssignable, Category = "Rope")
  FOnHookImpactSignature OnHookImpact;

  /** Called when the rope is detached (orphaned) from the player. Use this to
   * spawn visuals. */
  UFUNCTION(BlueprintImplementableEvent, Category = "Rope")
  void OnRopeDetached();

  /**
   * Updates the hook mesh orientation to follow velocity with a weighted feel.
   * Call this during tick before impact.
   */
  UFUNCTION(BlueprintCallable, Category = "Rope")
  void UpdateHookOrientation(const FVector &Velocity, float DeltaTime);

  // Exposed Publicly for RopeSystemComponent
  UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Movement")
  class UProjectileMovementComponent *ProjectileMovement;

protected:
  virtual void BeginPlay() override;

  UFUNCTION()
  void HandleHookImpact(UPrimitiveComponent *HitComp, AActor *OtherActor,
                        UPrimitiveComponent *OtherComp, FVector NormalImpulse,
                        const FHitResult &Hit);

protected:
  UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Rope")
  UStaticMeshComponent *HookMesh;

  UPROPERTY()
  USphereComponent *CollisionComponent;

  FTimerHandle SafeLaunchTimerHandle;

  UFUNCTION()
  void ReEnablePawnCollision();

  UPROPERTY(EditAnywhere, Category = "Rope")
  float LaunchImpulse = 3500.f;

  bool bImpacted = false;
  FHitResult ImpactResult;
};
