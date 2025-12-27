#include "Debug/DebugStrideWheelComponent.h"
#include "DrawDebugHelpers.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"

UDebugStrideWheelComponent::UDebugStrideWheelComponent() {
  PrimaryComponentTick.bCanEverTick = true;
  PrimaryComponentTick.bStartWithTickEnabled = true;
  bTickInEditor = true; // Allow visibility in Editor viewport
}

void UDebugStrideWheelComponent::BeginPlay() { Super::BeginPlay(); }

void UDebugStrideWheelComponent::TickComponent(
    float DeltaTime, ELevelTick TickType,
    FActorComponentTickFunction *ThisTickFunction) {
  Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

  if (!bDrawWheel) {
    return;
  }

  // Calculate rotation based on velocity (Runtime only)
  if (GetWorld() && GetWorld()->IsGameWorld()) {
    const ACharacter *OwnerCharacter = Cast<ACharacter>(GetOwner());
    if (OwnerCharacter) {
      const float Speed = OwnerCharacter->GetVelocity().Size2D();

      // Circumference = 2 * PI * Radius = StrideLength
      const float WheelRadius = StrideLength / (2.0f * PI);

      // AngularVelocity = LinearVelocity / Radius
      const float AngularVelocity = Speed / WheelRadius;
      CurrentRotation -= AngularVelocity * DeltaTime; // Roll forward
      CurrentRotation = FMath::Fmod(CurrentRotation, 2.0f * PI);
    }
  }

  // Draw Logic (Runtime & Editor)
  const float WheelRadius = StrideLength / (2.0f * PI);

  // Use Component Transform for placement
  const FVector Center = GetComponentLocation();
  const FQuat WorldRot = GetComponentQuat();
  const FVector Forward = WorldRot.GetForwardVector();
  const FVector Up = WorldRot.GetUpVector();
  const FVector Right = WorldRot.GetRightVector();

  DrawWheelDebug(WheelRadius, Center, Forward, Up, Right);
}

void UDebugStrideWheelComponent::DrawWheelDebug(float WheelRadius,
                                                const FVector &Center,
                                                const FVector &Forward,
                                                const FVector &Up,
                                                const FVector &Right) {
  const UWorld *World = GetWorld();
  if (!World) {
    return;
  }

  // Line 1: Spoke aligned with rotation
  const FVector Spoke1Dir =
      Forward * FMath::Cos(CurrentRotation) + Up * FMath::Sin(CurrentRotation);
  const FVector Line1Start = Center + Spoke1Dir * WheelRadius;
  const FVector Line1End = Center - Spoke1Dir * WheelRadius;

  // Line 2: Spoke perpendicular
  const FVector Spoke2Dir = Forward * FMath::Cos(CurrentRotation + PI * 0.5f) +
                            Up * FMath::Sin(CurrentRotation + PI * 0.5f);
  const FVector Line2Start = Center + Spoke2Dir * WheelRadius;
  const FVector Line2End = Center - Spoke2Dir * WheelRadius;

  DrawDebugLine(World, Line1Start, Line1End, WheelColor, false, -1.0f, 0, 2.0f);
  DrawDebugLine(World, Line2Start, Line2End, WheelColor, false, -1.0f, 0, 2.0f);

  // Contact Point (Bottom relative to component Up)
  const FVector ContactPoint = Center - Up * WheelRadius;
  DrawDebugSphere(World, ContactPoint, 5.0f, 4, FColor::Red, false, -1.0f, 0,
                  1.0f);

  // Circle Outline (Vertical Plane defined by Forward and Up, Normal is Right)
  // DrawDebugCircle(..., YAxis, ZAxis). To draw in X-Z plane, YAxis=Forward,
  // ZAxis=Up.
  DrawDebugCircle(World, Center, WheelRadius, 32, WheelColor, false, -1.0f, 0,
                  1.5f, Forward, Up);
}
