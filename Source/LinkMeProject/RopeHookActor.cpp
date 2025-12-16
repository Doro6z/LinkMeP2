// RopeHookActor.cpp

#include "RopeHookActor.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SphereComponent.h"
#include "Components/CapsuleComponent.h" // Added missing include
#include "GameFramework/Actor.h"
#include "PhysicsEngine/BodyInstance.h"
#include "GameFramework/Character.h" // Added missing include

ARopeHookActor::ARopeHookActor()
{
	PrimaryActorTick.bCanEverTick = true;

	// Collision
	USphereComponent* Sphere = CreateDefaultSubobject<USphereComponent>(TEXT("HookCollision"));
	Sphere->InitSphereRadius(12.f);
	// REMOVED: Sphere->SetSimulatePhysics(true);
	// REMOVED: Sphere->SetNotifyRigidBodyCollision(true); // Handled by Projectile
	Sphere->SetCollisionProfileName(TEXT("BlockAllDynamic")); // Changed from PhysicsActor
	Sphere->OnComponentHit.AddDynamic(this, &ARopeHookActor::HandleHookImpact);
	CollisionComponent = Sphere;
	RootComponent = Sphere;

	// Mesh
	HookMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("HookMesh"));
	HookMesh->SetupAttachment(CollisionComponent);
	HookMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	// Movement (PROJECTILE SYSTEM)
	ProjectileMovement = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("ProjectileMovement"));
	ProjectileMovement->UpdatedComponent = CollisionComponent;
	ProjectileMovement->InitialSpeed = LaunchImpulse; // 3500.f
	ProjectileMovement->MaxSpeed = LaunchImpulse;
	ProjectileMovement->bRotationFollowsVelocity = true;
	ProjectileMovement->bSweepCollision = true; // ANTI-TUNNELING FIX
	ProjectileMovement->ProjectileGravityScale = 1.0f;
	ProjectileMovement->bShouldBounce = false;
}

void ARopeHookActor::BeginPlay()
{
	Super::BeginPlay();

	// Ignore collision with owner to prevent immediate self-hit
	if (GetOwner())
	{
		if (CollisionComponent)
		{
			// Correct method name: IgnoreActorWhenMoving
			CollisionComponent->IgnoreActorWhenMoving(GetOwner(), true);
			
			// Si le Owner est un Character, ignorer aussi sa capsule spécifiquement si besoin
			if (ACharacter* Char = Cast<ACharacter>(GetOwner()))
			{
				CollisionComponent->IgnoreComponentWhenMoving(Char->GetCapsuleComponent(), true);
				if (Char->GetMesh())
				{
					CollisionComponent->IgnoreComponentWhenMoving(Char->GetMesh(), true);
				}
			}
		}
	}
}

void ARopeHookActor::Tick(float DeltaTime)
{
Super::Tick(DeltaTime);
}

void ARopeHookActor::Fire(const FVector& Direction)
{
	UE_LOG(LogTemp, Warning, TEXT("Hook Fire called with Direction: %s"), *Direction.ToString());
	if (ProjectileMovement)
	{
		ProjectileMovement->Velocity = Direction * ProjectileMovement->InitialSpeed;
		ProjectileMovement->Activate();
		UE_LOG(LogTemp, Warning, TEXT("Projectile Activated with Speed: %f"), ProjectileMovement->InitialSpeed);
	}
}

void ARopeHookActor::FireVelocity(const FVector& Velocity)
{
	UE_LOG(LogTemp, Warning, TEXT("Hook FireVelocity called: %s"), *Velocity.ToString());
	if (ProjectileMovement)
	{
		ProjectileMovement->Velocity = Velocity;
		ProjectileMovement->Activate();
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

	// Stop projectile
	if (ProjectileMovement)
	{
		ProjectileMovement->StopMovementImmediately();
		ProjectileMovement->SetComponentTickEnabled(false);
		UE_LOG(LogTemp, Warning, TEXT("Projectile movement stopped on hook."));
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

void ARopeHookActor::UpdateHookOrientation(const FVector& Velocity, float DeltaTime)
{
	if (bImpacted || Velocity.IsNearlyZero()) return;

	FRotator TargetRot = Velocity.Rotation();
	FRotator NewRot = FMath::RInterpTo(GetActorRotation(), TargetRot, DeltaTime, 15.0f);
	SetActorRotation(NewRot);
}
