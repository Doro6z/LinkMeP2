// RopeTypes.h
#pragma once

#include "CoreMinimal.h"
#include "RopeTypes.generated.h"

/** Un point de changement de direction sur un obstacle (edge lock) */
USTRUCT(BlueprintType)
struct FRopeBendpoint {
  GENERATED_BODY();

  UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
  FVector Position = FVector::ZeroVector;

  /** Surface normal at the wrap point - used for pressure direction validation
   */
  UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
  FVector SurfaceNormal = FVector::UpVector;

  /** Whether this bend point has a valid surface normal */
  UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
  bool bHasValidNormal = false;

  // Infos de debug / pour retrouver l'edge
  UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
  int32 TriangleIndex = INDEX_NONE;

  UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
  FVector EdgeA = FVector::ZeroVector;

  UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
  FVector EdgeB = FVector::ZeroVector;

  // Pour du multi-edges plus tard (graphe d'adjacence)
  UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
  TObjectPtr<UPrimitiveComponent> HitComponent = nullptr;

  // Constructors
  FRopeBendpoint() = default;

  FRopeBendpoint(const FVector &InPosition,
                 const FVector &InNormal = FVector::UpVector)
      : Position(InPosition), SurfaceNormal(InNormal),
        bHasValidNormal(!InNormal.IsNearlyZero()) {}
};

/** Segment géométrique corde (pour debug / draw) */
USTRUCT(BlueprintType)
struct FRopeSegment {
  GENERATED_BODY();

  UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
  FVector Start = FVector::ZeroVector;

  UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
  FVector End = FVector::ZeroVector;
};

/** Paramètres de tension physique */
USTRUCT(BlueprintType)
struct FRopeTensionSettings {
  GENERATED_BODY();

  // Longueur max corde
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rope")
  float MaxLength = 800.f;

  // Force appliquée quand on dépasse la longueur
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rope")
  float TensionStiffness = 4000.f;

  // Facteur de rebond radial
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rope")
  float BounceFactor = 1.0f;

  // Friction quand on est en butée
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rope")
  float TangentialFriction = 0.1f;
};
