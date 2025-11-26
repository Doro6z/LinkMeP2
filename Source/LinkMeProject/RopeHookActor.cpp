// RopeHookActor.cpp

#include "RopeHookActor.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SphereComponent.h"
#include "GameFramework/Actor.h"
#include "PhysicsEngine/BodyInstance.h"

ARopeHookActor::ARopeHookActor()
{
PrimaryActorTick.bCanEverTick = true;

USphereComponent* Sphere = CreateDefaultSubobject<USphereComponent>(TEXT("HookCollision"));
Sphere->InitSphereRadius(12.f);
Sphere->SetSimulatePhysics(true);
Sphere->SetNotifyRigidBodyCollision(true);
Sphere->SetCollisionProfileName(TEXT("PhysicsActor"));
Sphere->OnComponentHit.AddDynamic(this, &ARopeHookActor::HandleHookImpact);
CollisionComponent = Sphere;

HookMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("HookMesh"));
HookMesh->SetupAttachment(CollisionComponent);
HookMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void ARopeHookActor::BeginPlay()
{
Super::BeginPlay();
}

void ARopeHookActor::Tick(float DeltaTime)
{
Super::Tick(DeltaTime);
}

void ARopeHookActor::Fire(const FVector& Direction)
{
if (UPrimitiveComponent* Primitive = Cast<UPrimitiveComponent>(CollisionComponent))
{
Primitive->AddImpulse(Direction.GetSafeNormal() * LaunchImpulse, NAME_None, true);
}
}

void ARopeHookActor::HandleHookImpact(UPrimitiveComponent* HitComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit)
{
if (bImpacted)
{
return;
}

bImpacted = true;
ImpactResult = Hit;
OnHookImpact.Broadcast(Hit);

if (UPrimitiveComponent* Primitive = Cast<UPrimitiveComponent>(CollisionComponent))
{
Primitive->SetSimulatePhysics(false);
}
}
