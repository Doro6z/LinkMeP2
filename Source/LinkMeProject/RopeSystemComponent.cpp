// RopeSystemComponent.cpp

#include "RopeSystemComponent.h"
#include "RopeHookActor.h"
#include "RopeRenderComponent.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"

UAC_RopeSystem::UAC_RopeSystem()
{
PrimaryComponentTick.bCanEverTick = true;
}

void UAC_RopeSystem::BeginPlay()
{
Super::BeginPlay();

RenderComponent = GetOwner() ? GetOwner()->FindComponentByClass<URopeRenderComponent>() : nullptr;
}

void UAC_RopeSystem::TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

if (RopeState == ERopeState::Idle)
{
return;
}

if (RopeState == ERopeState::Flying)
{
if (CurrentHook && CurrentHook->HasImpacted())
{
TransitionToAttached(CurrentHook->GetImpactResult());
}
}

if (RopeState == ERopeState::Attached)
{
ManageBendPoints();
ManageRopeLength(DeltaTime);
ApplyForcesToPlayer();
}

UpdateRopeVisual();
}

void UAC_RopeSystem::FireHook(const FVector& Direction)
{
if (!HookClass || !GetWorld())
{
return;
}

AActor* Owner = GetOwner();
if (!Owner)
{
return;
}

const FVector SpawnLocation = Owner->GetActorLocation() + Direction * 50.f;
const FRotator SpawnRotation = Direction.Rotation();

FActorSpawnParameters Params;
Params.Owner = Owner;
Params.Instigator = Cast<APawn>(Owner);

CurrentHook = GetWorld()->SpawnActor<ARopeHookActor>(HookClass, SpawnLocation, SpawnRotation, Params);
if (CurrentHook)
{
CurrentHook->Fire(Direction);
    CurrentHook->OnHookImpact.AddDynamic(this, &UAC_RopeSystem::OnHookImpact);
RopeState = ERopeState::Flying;
BendPoints.Reset();
}
}

void UAC_RopeSystem::Sever()
{
if (CurrentHook)
{
CurrentHook->Destroy();
CurrentHook = nullptr;
}

BendPoints.Reset();
CurrentLength = 0.f;
RopeState = ERopeState::Idle;
}

void UAC_RopeSystem::OnHookImpact(const FHitResult& Hit)
{
TransitionToAttached(Hit);
}

void UAC_RopeSystem::TransitionToAttached(const FHitResult& Hit)
{
AActor* Owner = GetOwner();
if (!Owner)
{
return;
}

BendPoints.Reset();
BendPoints.Add(Hit.ImpactPoint + Hit.ImpactNormal * BendOffset); // anchor
BendPoints.Add(Owner->GetActorLocation());

CurrentLength = FMath::Min(MaxLength, (Owner->GetActorLocation() - BendPoints[0]).Size());
RopeState = ERopeState::Attached;
}

void UAC_RopeSystem::ManageBendPoints()
{
if (BendPoints.Num() == 0)
{
return;
}

const FVector PlayerPos = GetOwner()->GetActorLocation();
const FVector LastPoint = BendPoints.Last();

FHitResult Hit;
if (LineTrace(LastPoint, PlayerPos, Hit))
{
const FVector NewPoint = Hit.ImpactPoint + Hit.ImpactNormal * BendOffset;
BendPoints.Add(NewPoint);
}

if (BendPoints.Num() > 1)
{
const FVector PreLast = BendPoints[BendPoints.Num() - 2];
if (!LineTrace(PreLast, PlayerPos, Hit))
{
BendPoints.RemoveAt(BendPoints.Num() - 1);
}
}

// Always keep last point close to player location
if (BendPoints.Num() > 0)
{
BendPoints.Last() = PlayerPos;
}
}

void UAC_RopeSystem::ManageRopeLength(float DeltaTime)
{
    (void)DeltaTime;

    float RopeLength = 0.f;
for (int32 Index = 0; Index < BendPoints.Num() - 1; ++Index)
{
RopeLength += (BendPoints[Index + 1] - BendPoints[Index]).Size();
}

if (CurrentLength <= 0.f)
{
CurrentLength = MaxLength;
}

if (RopeLength > CurrentLength)
{
const float Excess = RopeLength - CurrentLength;
const FVector Dir = (BendPoints[0] - GetOwner()->GetActorLocation()).GetSafeNormal();
const FVector Force = Dir * (Excess * SpringStiffness);
if (ACharacter* Character = Cast<ACharacter>(GetOwner()))
{
if (UCharacterMovementComponent* MoveComp = Character->GetCharacterMovement())
{
MoveComp->AddForce(Force);
}
}
}
}

void UAC_RopeSystem::ApplyForcesToPlayer()
{
ACharacter* OwnerChar = Cast<ACharacter>(GetOwner());
if (!OwnerChar || BendPoints.Num() == 0)
{
return;
}

const FVector Anchor = BendPoints[0];
const FVector PlayerPos = OwnerChar->GetActorLocation();
const FVector Dir = (Anchor - PlayerPos).GetSafeNormal();
const float Distance = (Anchor - PlayerPos).Size();

const float Stretch = Distance - CurrentLength;
if (Stretch > 0.f)
{
if (UCharacterMovementComponent* MoveComp = OwnerChar->GetCharacterMovement())
{
FVector Velocity = MoveComp->Velocity;
const FVector TangentVel = Velocity - FVector::DotProduct(Velocity, Dir) * Dir;
MoveComp->Velocity = TangentVel;

const FVector Pull = Dir * (Stretch * SpringStiffness);
MoveComp->AddForce(Pull);

const FVector Right = FVector::CrossProduct(Dir, FVector::UpVector);
MoveComp->AddForce(Right * SwingTorque);
}
}
}

void UAC_RopeSystem::UpdateRopeVisual()
{
if (!RenderComponent)
{
RenderComponent = GetOwner() ? GetOwner()->FindComponentByClass<URopeRenderComponent>() : nullptr;
}

if (RenderComponent && BendPoints.Num() > 0)
{
RenderComponent->RefreshFromBendPoints(BendPoints);
RenderComponent->Simulate(GetWorld() ? GetWorld()->GetDeltaSeconds() : 0.f);
}
}

bool UAC_RopeSystem::LineTrace(const FVector& Start, const FVector& End, FHitResult& OutHit) const
{
FCollisionQueryParams Params(SCENE_QUERY_STAT(RopeTrace), false, GetOwner());
return GetWorld()->LineTraceSingleByChannel(OutHit, Start, End, ECC_Visibility, Params);
}
