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
	RootComponent = Sphere; // IMPORTANT: Set as root for physics to work

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
	UE_LOG(LogTemp, Warning, TEXT("Hook Fire called with Direction: %s"), *Direction.ToString());
	if (CollisionComponent)
	{
		CollisionComponent->AddImpulse(Direction * LaunchImpulse);
		UE_LOG(LogTemp, Warning, TEXT("Impulse applied: %f"), LaunchImpulse);
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("CollisionComponent is null in Fire!"));
	}
}

void ARopeHookActor::HandleHookImpact(UPrimitiveComponent* HitComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit)
{
	UE_LOG(LogTemp, Warning, TEXT("HandleHookImpact called! Hit actor: %s, Hit component: %s"), 
		OtherActor ? *OtherActor->GetName() : TEXT("NULL"), 
		OtherComp ? *OtherComp->GetName() : TEXT("NULL"));

	if (bImpacted) 
	{
		UE_LOG(LogTemp, Warning, TEXT("Already impacted, ignoring."));
		return;
	}

	bImpacted = true;
	ImpactResult = Hit;

	// Stop physics
	if (CollisionComponent)
	{
		CollisionComponent->SetSimulatePhysics(false);
		CollisionComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		UE_LOG(LogTemp, Warning, TEXT("Physics stopped on hook."));
	}

	// Attach to hit object (only if it's not us!)
	if (Hit.Component.IsValid() && Hit.Component.Get() != CollisionComponent)
	{
		AttachToComponent(Hit.Component.Get(), FAttachmentTransformRules::KeepWorldTransform, Hit.BoneName);
		UE_LOG(LogTemp, Warning, TEXT("Hook attached to: %s"), *Hit.Component->GetName());
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("Cannot attach to hit component (null or self)"));
	}

	// Notify listeners
	if (OnHookImpact.IsBound())
	{
		OnHookImpact.Broadcast(Hit);
		UE_LOG(LogTemp, Warning, TEXT("OnHookImpact broadcasted!"));
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("OnHookImpact has no listeners!"));
	}
}
