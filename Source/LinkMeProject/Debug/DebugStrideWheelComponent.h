#pragma once

#include "Components/SceneComponent.h"
#include "CoreMinimal.h"
#include "DebugStrideWheelComponent.generated.h"

/**
 * Debug component that visualizes a "stride wheel" to help tune StrideLength.
 * The wheel rotates based on character velocity. When the wheel's contact point
 * matches the foot plant timing, StrideLength is correctly calibrated.
 *
 * Relationship: WheelRadius = StrideLength / (2 * PI)
 */
UCLASS(ClassGroup = (Debug), meta = (BlueprintSpawnableComponent))
class LINKMEPROJECT_API UDebugStrideWheelComponent : public USceneComponent {
  GENERATED_BODY()

public:
  UDebugStrideWheelComponent();

  virtual void
  TickComponent(float DeltaTime, ELevelTick TickType,
                FActorComponentTickFunction *ThisTickFunction) override;

  /** The stride length from AnimBP. Wheel radius is derived: Radius =
   * StrideLength / (2*PI) */
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stride Wheel")
  float StrideLength = 200.0f;

  // DrawOffset removed: Use Component Relative Location instead.

  /** Color of the wheel lines */
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stride Wheel")
  FColor WheelColor = FColor::Cyan;

  /** Enable/disable drawing */
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stride Wheel")
  bool bDrawWheel = true;

  /** Multiplier for the visual radius only. Does not affect logic. */
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stride Wheel")
  float VisualScale = 1.0f;

protected:
  virtual void BeginPlay() override;

private:
  /** Current rotation angle in radians */
  float CurrentRotation = 0.0f;

  /** Draws the wheel as crossed lines */
  void DrawWheelDebug(float WheelRadius, const FVector &Center,
                      const FVector &Forward, const FVector &Up,
                      const FVector &Right);
};
