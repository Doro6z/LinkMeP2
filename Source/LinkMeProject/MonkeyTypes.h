// MonkeyTypes.h
// Shared types for Monkey character animation and procedural systems

#pragma once

#include "CoreMinimal.h"
#include "MonkeyTypes.generated.h"

// ============================================================================
// IK DATA (Per-Limb)
// ============================================================================

/** Data for individual limb IK (foot/hand) */
USTRUCT(BlueprintType)
struct FLimbIKData {
  GENERATED_BODY()

  /** Offset to apply to animated position (World Space Z) */
  UPROPERTY(BlueprintReadOnly, Category = "IK")
  FVector EffectorOffset = FVector::ZeroVector;

  /** Target rotation aligned to surface normal (World Space) */
  UPROPERTY(BlueprintReadOnly, Category = "IK")
  FRotator TargetRotation = FRotator::ZeroRotator;

  /** Alpha for blending IK (0=None, 1=Full) */
  UPROPERTY(BlueprintReadOnly, Category = "IK")
  float Alpha = 0.0f;

  /** Whether the trace hit ground */
  UPROPERTY(BlueprintReadOnly, Category = "IK")
  bool bHitGround = false;
};

// ============================================================================
// UNIFIED PROCEDURAL DATA
// ============================================================================

/** All procedural animation data, calculated in Character, consumed by
 * AnimInstance */
USTRUCT(BlueprintType)
struct FProceduralAnimData {
  GENERATED_BODY()

  // ----- IK -----

  UPROPERTY(BlueprintReadOnly, Category = "IK")
  FLimbIKData Foot_L;

  UPROPERTY(BlueprintReadOnly, Category = "IK")
  FLimbIKData Foot_R;

  UPROPERTY(BlueprintReadOnly, Category = "IK")
  FLimbIKData Hand_L;

  UPROPERTY(BlueprintReadOnly, Category = "IK")
  FLimbIKData Hand_R;

  /** Pelvis vertical offset (for hip adjustment on slopes) */
  UPROPERTY(BlueprintReadOnly, Category = "IK")
  float PelvisOffset = 0.0f;

  // ----- LEAN -----

  /** Body lean based on velocity direction */
  UPROPERTY(BlueprintReadOnly, Category = "Lean")
  FRotator LeanAmount = FRotator::ZeroRotator;

  // ----- SWING -----

  /** Swing phase (0-1) during rope attachment */
  UPROPERTY(BlueprintReadOnly, Category = "Swing")
  float SwingPhase = 0.0f;

  /** Swing intensity (normalized tension) */
  UPROPERTY(BlueprintReadOnly, Category = "Swing")
  float SwingIntensity = 0.0f;

  // ----- LANDING -----

  /** Landing impact alpha (1.0 on land, decays to 0) */
  UPROPERTY(BlueprintReadOnly, Category = "Landing")
  float LandingAlpha = 0.0f;
};

// ============================================================================
// LEGACY (Deprecated - kept for compatibility during transition)
// ============================================================================

/** @deprecated Use FLimbIKData instead */
USTRUCT(BlueprintType)
struct FFootIKData {
  GENERATED_BODY()

  UPROPERTY(BlueprintReadOnly, Category = "IK")
  FVector TargetLocation = FVector::ZeroVector;

  UPROPERTY(BlueprintReadOnly, Category = "IK")
  FVector JointTargetLocation = FVector::ZeroVector;

  UPROPERTY(BlueprintReadOnly, Category = "IK")
  FRotator TargetRotation = FRotator::ZeroRotator;

  UPROPERTY(BlueprintReadOnly, Category = "IK")
  float BlendWeight = 0.0f;

  UPROPERTY(BlueprintReadOnly, Category = "IK")
  bool bHitGround = false;
};
