// CharacterRope.cpp

#include "CharacterRope.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Net/UnrealNetwork.h" // For DOREPLLIFETIME

#include "AimingComponent.h"
#include "TPSAimingComponent.h"

ACharacterRope::ACharacterRope() {
  PrimaryActorTick.bCanEverTick = true;

  // Create TPS Aiming Component
  AimingComponent =
      CreateDefaultSubobject<UTPSAimingComponent>(TEXT("AimingComponent"));

  // Create Hook Charge Component
  HookChargeComponent =
      CreateDefaultSubobject<UHookChargeComponent>(TEXT("HookChargeComponent"));

  // Create Camera Manager Component
  CameraManager =
      CreateDefaultSubobject<URopeCameraManager>(TEXT("CameraManager"));

  // Create Inertial Movement Component
  InertialMovementComp = CreateDefaultSubobject<UInertialMovementComponent>(
      TEXT("InertialMovementComp"));

  // Character orientation driven by movement (typical third-person setup).
  bUseControllerRotationYaw = false;
  bUseControllerRotationPitch = false;
  bUseControllerRotationRoll = false;
  GetCharacterMovement()->bOrientRotationToMovement = true;
  GetCharacterMovement()->RotationRate = FRotator(0.0f, 540.0f, 0.0f);

  // Position the mesh like the default UE third-person template so it stays
  // visible.
  GetMesh()->SetRelativeLocation(FVector(0.0f, 0.0f, -90.0f));
  GetMesh()->SetRelativeRotation(FRotator(0.0f, -90.0f, 0.0f));

  // Default Locomotion State
  CurrentStance = EMonkeyStance::Quadruped;
  CurrentGait = EMonkeyGait::Walk;
  bIsSprinting = false;
  bIsWalking = false;
}

void ACharacterRope::GetLifetimeReplicatedProps(
    TArray<FLifetimeProperty> &OutLifetimeProps) const {
  Super::GetLifetimeReplicatedProps(OutLifetimeProps);

  DOREPLIFETIME(ACharacterRope, CurrentStance);
}

void ACharacterRope::BeginPlay() {
  Super::BeginPlay();

  // Configure TPS Aiming Component (magnetism settings)
  if (AimingComponent) {
    AimingComponent->bEnableMagnetism = bEnableMagnetism;
    AimingComponent->MagnetismRange = MagnetismRange;
    AimingComponent->MagnetismConeAngle = MagnetismConeAngle;
    AimingComponent->MagnetismStrength = MagnetismStrength;
  }
}

void ACharacterRope::Landed(const FHitResult &Hit) {
  Super::Landed(Hit);

  // Apply LandingImpact camera effect based on impact velocity
  if (CameraManager && GetCharacterMovement()) {
    float ImpactVelocity = FMath::Abs(GetCharacterMovement()->Velocity.Z);

    if (ImpactVelocity > 500.f) {
      // Map velocity to FOV intensity (-3 to -10)
      float Intensity = FMath::GetMappedRangeValueClamped(
          FVector2D(500.f, 1500.f), FVector2D(-3.f, -10.f), ImpactVelocity);
      CameraManager->ApplyTransientEffect(FName("LandingImpact"), Intensity,
                                          FVector::ZeroVector, 0.15f);
    }
  }
}

#include "Kismet/GameplayStatics.h"

void ACharacterRope::Tick(float DeltaTime) {
  Super::Tick(DeltaTime);

  // Update Locomotion (Client & Server run this for prediction)
  UpdateLocomotionSpeed(DeltaTime);

  // Update Procedural Animation (IK, Lean, Swing, Landing)
  UpdateProceduralAnimation(DeltaTime);

  // NOTE: Camera logic is now handled by PlayerCameraManager

  // Hook Charge Visualization (Trajectory & Reticle)
  // Only show when actively charging (State == Charging)
  if (HookChargeComponent && HookChargeComponent->IsCharging()) {
    // Update Trajectory
    if (bShowTrajectoryWhileCharging) {
      TimeSinceLastTrajectoryUpdate += DeltaTime;
      if (TimeSinceLastTrajectoryUpdate >= TrajectoryUpdateFrequency) {
        UpdateTrajectoryVisualization(DeltaTime);
        TimeSinceLastTrajectoryUpdate = 0.0f;
      }
    }

    // Update Reticle
    UpdateFocusReticle();
  } else if (HookChargeComponent && !HookChargeComponent->IsCharging()) {
    // Ensure Reticle is hidden if not charging
    if (FocusReticleInstance && !FocusReticleInstance->IsHidden()) {
      FocusReticleInstance->SetActorHiddenInGame(true);
    }
  }

  // Legacy Trajectory (Fallback if not charging but aiming?
  // Actually, we want trajectory primarily during CHARGE now as per request.
  // "Lorsqu'il charge, on voit la trajectoire que suivra le lancé s'augmenter."
  // So we disable the old aiming-only static trajectory.)
}

void ACharacterRope::StartAiming() {
  if (AimingComponent) {
    AimingComponent->StartAiming();
  }

  bIsPreparingHook = true;

  // Orient character to camera view
  bUseControllerRotationYaw = true;
  GetCharacterMovement()->bOrientRotationToMovement = false;
}

void ACharacterRope::StopAiming() {
  if (AimingComponent) {
    AimingComponent->StopAiming();
  }

  bIsPreparingHook = false;

  // Restore movement orientation
  bUseControllerRotationYaw = false;
  GetCharacterMovement()->bOrientRotationToMovement = true;
}

FVector ACharacterRope::GetFireDirection() const {
  if (AimingComponent) {
    return AimingComponent->GetAimDirection();
  }

  // Fallback: use controller rotation
  return GetControlRotation().Vector();
}

void ACharacterRope::StartFocus() {
  if (UTPSAimingComponent *TPSComp =
          Cast<UTPSAimingComponent>(AimingComponent)) {
    TPSComp->StartFocus();
  }

  // Notify CameraManager to enable aiming mode
  if (CameraManager) {
    CameraManager->SetAiming(true);
  }
}

void ACharacterRope::StopFocus() {
  if (UTPSAimingComponent *TPSComp =
          Cast<UTPSAimingComponent>(AimingComponent)) {
    TPSComp->StopFocus();
  }

  // Notify CameraManager to disable aiming mode
  if (CameraManager) {
    CameraManager->SetAiming(false);
  }
}

// ===================================================================
// HOOK CHARGE SYSTEM & VISUALIZATION
// ===================================================================

void ACharacterRope::StartChargingHook() {
  if (GEngine)
    GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Cyan,
                                     TEXT("[INPUT] StartChargingHook Pressed"));

  if (!HookChargeComponent || !AimingComponent)
    return;

  UTPSAimingComponent *TPSComp = Cast<UTPSAimingComponent>(AimingComponent);
  bool bFocus = TPSComp ? TPSComp->IsFocusing() : false;
  FVector StartLoc = GetProjectileStartLocation();
  FVector TargetLoc = AimingComponent->GetTargetLocation();

  HookChargeComponent->StartCharging(bFocus, TargetLoc, StartLoc);
}

void ACharacterRope::CancelHookCharge() {
  if (GEngine)
    GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Orange,
                                     TEXT("[INPUT] CancelHookCharge Called"));

  if (HookChargeComponent) {
    HookChargeComponent->CancelCharging();
  }

  if (FocusReticleInstance) {
    FocusReticleInstance->SetActorHiddenInGame(true);
  }
}

void ACharacterRope::FireChargedHook() {
  if (GEngine)
    GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Cyan,
                                     TEXT("[INPUT] FireChargedHook Released"));
  UE_LOG(LogTemp, Warning, TEXT("ACharacterRope::FireChargedHook called"));

  if (!HookChargeComponent || !AimingComponent) {
    if (GEngine)
      GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Red,
                                       TEXT("ERROR: Missing Components!"));
    UE_LOG(LogTemp, Error, TEXT("FireChargedHook: Missing Component"));
    return;
  }

  // Initialize to Zero
  FVector OutVelocity = FVector::ZeroVector;
  bool bValid = HookChargeComponent->StopChargingAndGetVelocity(OutVelocity);

  UE_LOG(LogTemp, Warning,
         TEXT("FireChargedHook: StopCharging returned %s, Velocity: %s"),
         bValid ? TEXT("TRUE") : TEXT("FALSE"), *OutVelocity.ToString());

  // Hide reticle
  if (FocusReticleInstance)
    FocusReticleInstance->SetActorHiddenInGame(true);

  if (bValid) {
    // Si le composant n'a pas pu calculer de vélocité (ex: Mode Manuel), on le
    // fait ici. On utilise SizeSquared pour éviter les problèmes de précision
    // (IsNearlyZero)
    if (OutVelocity.SizeSquared() < 1000.0f) {
      // Fallback manuel : Direction caméra * Vitesse de charge
      float Speed = HookChargeComponent->GetCurrentLaunchSpeed();
      FVector FireDir = GetFireDirection();

      UE_LOG(LogTemp, Warning,
             TEXT("FireChargedHook: Using Fallback Manual Fire. Dir: %s, "
                  "Speed: %f"),
             *FireDir.ToString(), Speed);

      if (FireDir.IsZero()) {
        // Emergency Fallback: Actor Forward
        FireDir = GetActorForwardVector();
        UE_LOG(LogTemp, Error,
               TEXT("FireChargedHook: FireDirection was ZERO! Using Actor "
                    "Forward."));
        if (GEngine)
          GEngine->AddOnScreenDebugMessage(
              -1, 5.f, FColor::Red, TEXT("ERROR: FireDirection is ZERO!"));
      }

      OutVelocity = FireDir * Speed;
    }

    UE_LOG(LogTemp, Warning,
           TEXT("FireChargedHook: Final Velocity to Fire: %s"),
           *OutVelocity.ToString());

    // Fire!

    // Fire!
    if (URopeSystemComponent *RopeSys =
            FindComponentByClass<URopeSystemComponent>()) {
      if (GEngine)
        GEngine->AddOnScreenDebugMessage(
            -1, 2.0f, FColor::Green,
            TEXT("Calling RopeSys->FireChargedHook..."));
      UE_LOG(LogTemp, Warning,
             TEXT("FireChargedHook: Calling RopeSystem->FireChargedHook"));
      RopeSys->FireChargedHook(OutVelocity);
    } else {
      if (GEngine)
        GEngine->AddOnScreenDebugMessage(
            -1, 5.0f, FColor::Red,
            TEXT("ERROR: RopeSystemComponent Not Found!"));
      UE_LOG(
          LogTemp, Error,
          TEXT("FireChargedHook: RopeSystemComponent NOT FOUND on Character"));
    }
  } else {
    if (GEngine)
      GEngine->AddOnScreenDebugMessage(
          -1, 2.0f, FColor::Red,
          FString::Printf(TEXT("Charge Too Low: %f < %f"),
                          HookChargeComponent->GetChargeRatio(),
                          HookChargeComponent->MinChargeThreshold));
    UE_LOG(LogTemp, Warning,
           TEXT("FireChargedHook: Charge was invalid/too low"));
  }

  bIsPreparingHook = false; // Reset Anim state
}

FVector ACharacterRope::GetProjectileStartLocation() const {
  if (GetMesh()->DoesSocketExist(TEXT("hand_r"))) {
    return GetMesh()->GetSocketLocation(TEXT("hand_r"));
  }
  return GetActorLocation();
}

void ACharacterRope::UpdateTrajectoryVisualization(float DeltaTime) {
  if (!HookChargeComponent || !AimingComponent)
    return;

  // Determine Velocity to visualize
  float CurrentSpeed = HookChargeComponent->GetCurrentLaunchSpeed();
  FVector LaunchVelocity = GetFireDirection() * CurrentSpeed;

  // Si Focus mode et Reachable, on pourrait vouloir visualiser la trajectoire
  // "idéale" calculée par l'algo Mais HookChargeComponent ne stocke pas le
  // vecteur vélocité idéal publiquement pour l'instant.. Pour l'instant,
  // visualiser GetFireDirection() * CurrentSpeed est une bonne approximation
  // car en Aim mode, la caméra regarde vers la cible.

  FVector StartLoc = GetProjectileStartLocation();

  // Configurer prédiction
  FPredictProjectilePathParams PredictParams;
  PredictParams.StartLocation = StartLoc;
  PredictParams.LaunchVelocity = LaunchVelocity;
  PredictParams.bTraceWithCollision = true;
  PredictParams.bTraceComplex = false;
  PredictParams.ProjectileRadius = 5.0f;
  PredictParams.MaxSimTime = 3.0f;
  PredictParams.SimFrequency = 15.0f;
  PredictParams.TraceChannel =
      ECC_WorldStatic; // Should use HookChargeComponent->ProjectileTraceChannel
  PredictParams.ActorsToIgnore.Add(this);

  // Couleur selon état
  FLinearColor TraceColor = TrajectoryColorNormal;

  if (UTPSAimingComponent *TPSComp =
          Cast<UTPSAimingComponent>(AimingComponent)) {
    if (TPSComp->IsFocusing()) {
      if (HookChargeComponent->IsTargetReachable() == false) {
        TraceColor = TrajectoryColorUnreachable;
      } else if (HookChargeComponent->IsChargePerfect()) {
        TraceColor = TrajectoryColorPerfect;
      }
    }
  }

  // Disable built-in debug drawing to handle color manually
  PredictParams.DrawDebugType = EDrawDebugTrace::None;

  FPredictProjectilePathResult PredictResult;
  if (UGameplayStatics::PredictProjectilePath(GetWorld(), PredictParams,
                                              PredictResult)) {
    // Draw trajectory manually
    for (int32 i = 0; i < PredictResult.PathData.Num() - 1; ++i) {
      DrawDebugLine(GetWorld(), PredictResult.PathData[i].Location,
                    PredictResult.PathData[i + 1].Location,
                    TraceColor.ToFColor(true), false,
                    TrajectoryUpdateFrequency + 0.02f, 0,
                    3.0f // Thickness
      );
    }

    // Draw Impact point if hit
    if (PredictResult.HitResult.bBlockingHit) {
      DrawDebugSphere(GetWorld(), PredictResult.HitResult.ImpactPoint, 10.0f,
                      12, TraceColor.ToFColor(true), false,
                      TrajectoryUpdateFrequency + 0.02f);
    }
  }
}

void ACharacterRope::UpdateFocusReticle() {
  UTPSAimingComponent *TPSComp = Cast<UTPSAimingComponent>(AimingComponent);
  if (!TPSComp || !TPSComp->IsFocusing()) {
    if (FocusReticleInstance)
      FocusReticleInstance->SetActorHiddenInGame(true);
    return;
  }

  // Obtenir position cible depuis l'aiming component
  // (Note: C'est le point visé par le rayon, pas forcément le point d'impact du
  // projectile)
  FVector TargetLoc = TPSComp->GetTargetLocation();

  // Spawn reticle if needed
  if (!FocusReticleInstance && FocusReticleClass) {
    FActorSpawnParameters SpawnParams;
    SpawnParams.Owner = this;
    FocusReticleInstance = GetWorld()->SpawnActor<AActor>(
        FocusReticleClass, TargetLoc, FRotator::ZeroRotator, SpawnParams);
  }

  if (FocusReticleInstance) {
    FocusReticleInstance->SetActorHiddenInGame(false);
    FocusReticleInstance->SetActorLocation(TargetLoc);

    // Ici on pourrait ajouter une logique pour le scale/pulse si ChargePerfect
    // ex: FocusReticleInstance->SetActorScale3D(...)
  }
}

void ACharacterRope::SetupPlayerInputComponent(
    UInputComponent *PlayerInputComponent) {
  Super::SetupPlayerInputComponent(PlayerInputComponent);

  // Bindings - Assurez-vous d'avoir les Action mappings dans l'éditeur ou
  // EnhancedInput ! PlayerInputComponent->BindAction("Fire", IE_Pressed, this,
  // &ACharacterRope::StartChargingHook);
  // PlayerInputComponent->BindAction("Fire", IE_Released, this,
  // &ACharacterRope::FireChargedHook);
  // PlayerInputComponent->BindAction("Focus", IE_Pressed, this,
  // &ACharacterRope::StartFocus);
  // PlayerInputComponent->BindAction("Focus", IE_Released, this,
  // &ACharacterRope::StopFocus);

  // Locomotion Bindings
  PlayerInputComponent->BindAction("StanceSwitch", IE_Pressed, this,
                                   &ACharacterRope::ToggleStance);
  PlayerInputComponent->BindAction("Sprint", IE_Pressed, this,
                                   &ACharacterRope::StartSprint);
  PlayerInputComponent->BindAction("Sprint", IE_Released, this,
                                   &ACharacterRope::StopSprint);

  PlayerInputComponent->BindAction("Walk", IE_Pressed, this,
                                   &ACharacterRope::StartWalking);
  PlayerInputComponent->BindAction("Walk", IE_Released, this,
                                   &ACharacterRope::StopWalking);
}

// ============================================================================
// LOCOMOTION IMPLEMENTATION
// ============================================================================

void ACharacterRope::ToggleStance() {
  EMonkeyStance NewStance = (CurrentStance == EMonkeyStance::Quadruped)
                                ? EMonkeyStance::Biped
                                : EMonkeyStance::Quadruped;
  TrySetStance(NewStance);
}

bool ACharacterRope::TrySetStance(EMonkeyStance NewStance) {
  // Restriction: Can only become Biped if room to stand up
  if (NewStance == EMonkeyStance::Biped &&
      CurrentStance == EMonkeyStance::Quadruped) {
    if (!CanStandUp()) {
      if (GEngine)
        GEngine->AddOnScreenDebugMessage(
            -1, 2.0f, FColor::Red, TEXT("Cannot Stand Up: Ceiling Obstructed"));
      return false;
    }
  }

  SetStance(NewStance);
  return true;
}

bool ACharacterRope::CanStandUp() const {
  if (!GetWorld())
    return false;

  // Check collision above head
  const float TraceDist =
      (BipedCapsuleSize.Y - QuadrupedCapsuleSize.Y) + 20.0f; // Delta + buffer
  const FVector Start = GetActorLocation();
  const FVector End = Start + FVector(0.f, 0.f, BipedCapsuleSize.Y + 20.f);

  // Using Capsule Trace for better volume check
  FCollisionShape Shape = FCollisionShape::MakeCapsule(
      BipedCapsuleSize.X, (BipedCapsuleSize.Y - QuadrupedCapsuleSize.Y));

  FCollisionQueryParams Params;
  Params.AddIgnoredActor(this);

  // Simple Line Trace for Ceiling
  bool bHit =
      GetWorld()->LineTraceTestByChannel(Start, End, ECC_Visibility, Params);

  return !bHit;
}

void ACharacterRope::UpdateCapsuleSize(EMonkeyStance NewStance) {
  if (!GetCapsuleComponent())
    return;

  float OldHeight = GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
  float TargetRadius, TargetHeight;

  if (NewStance == EMonkeyStance::Quadruped) {
    TargetRadius = QuadrupedCapsuleSize.X;
    TargetHeight = QuadrupedCapsuleSize.Y;
  } else {
    TargetRadius = BipedCapsuleSize.X;
    TargetHeight = BipedCapsuleSize.Y;
  }

  // 1. Resize Capsule
  GetCapsuleComponent()->SetCapsuleSize(TargetRadius, TargetHeight);

  // 2. Adjust Mesh Relative Location to keep feet at bottom (-HalfHeight)
  GetMesh()->SetRelativeLocation(FVector(0.f, 0.f, -TargetHeight));

  // 3. Adjust Actor World Location to prevent sinking/popping
  // If we grow (Height increases), Center moves UP.
  // If we stay at same Loc, feet move DOWN -> Penetration.
  // Correction = NewHeight - OldHeight
  if (GetCharacterMovement() && !GetCharacterMovement()->IsFalling()) {
    float HeightDelta = TargetHeight - OldHeight;
    AddActorWorldOffset(FVector(0.f, 0.f, HeightDelta));
  }
}

void ACharacterRope::SetStance(EMonkeyStance NewStance) {
  if (CurrentStance == NewStance)
    return;

  // 1. Immediate Local Prediction
  LocalSetStance(NewStance);

  // 2. Server RPC (if we are client)
  if (!HasAuthority()) {
    ServerSetStance(NewStance);
  }
}

void ACharacterRope::ServerSetStance_Implementation(EMonkeyStance NewStance) {
  // Server applies state -> triggers Replication -> triggers
  // OnRep_CurrentStance on clients
  CurrentStance = NewStance;
  LocalSetStance(NewStance);
}

void ACharacterRope::LocalSetStance(EMonkeyStance NewStance) {
  EMonkeyStance OldStance = CurrentStance;
  CurrentStance = NewStance;

  // Trigger Event for AnimBP / VFX
  OnStanceChanged.Broadcast(OldStance, NewStance);

  // Update Capsule Physics/Viz
  UpdateCapsuleSize(NewStance);
}

void ACharacterRope::OnRep_CurrentStance(EMonkeyStance OldStance) {
  // Called on Clients when Server updates CurrentStance
  // We call LocalSetStance to trigger events/FX
  // Note: We might have already predicted this locally, so check valid
  // transition? For now, simple override is safest to ensure sync.
  if (CurrentStance != OldStance) {
    LocalSetStance(CurrentStance);
  }
}

void ACharacterRope::StartSprint() {
  bIsSprinting = true;
  if (!HasAuthority())
    ServerSetSprint(true);
}

void ACharacterRope::StopSprint() {
  bIsSprinting = false;
  if (!HasAuthority())
    ServerSetSprint(false);
}

void ACharacterRope::ServerSetSprint_Implementation(bool bNewSprinting) {
  bIsSprinting = bNewSprinting;
}

void ACharacterRope::StartWalking() {
  bIsWalking = true;
  if (!HasAuthority())
    ServerSetWalk(true);
}

void ACharacterRope::StopWalking() {
  bIsWalking = false;
  if (!HasAuthority())
    ServerSetWalk(false);
}

void ACharacterRope::ServerSetWalk_Implementation(bool bNewWalking) {
  bIsWalking = bNewWalking;
}

void ACharacterRope::UpdateLocomotionSpeed(float DeltaTime) {
  if (!GetCharacterMovement())
    return;

  // 1. Determine Target Max Speed based on Stance & Input
  float TargetMaxSpeed = 0.f;

  if (CurrentStance == EMonkeyStance::Quadruped) {
    if (bIsSprinting)
      TargetMaxSpeed = QuadrupedSpeeds.Z; // Sprint
    else if (bIsWalking)
      TargetMaxSpeed = QuadrupedSpeeds.X; // Walk (Explicit Input)
    else
      TargetMaxSpeed = QuadrupedSpeeds.Y; // Jog (Default move)
  } else                                  // Biped
  {
    if (bIsSprinting)
      TargetMaxSpeed = BipedSpeeds.Z;
    else
      TargetMaxSpeed = BipedSpeeds.X; // Walk (Default Biped)
    // Note: Biped has no "Jog" really mapped by default to non-sprint, but we
    // use X (Walk) as base. If we want Biped Jog, we should use Y. Let's assume
    // Biped Default IS Jog (Y) for consistency? User said: "Biped Speeds:
    // Walk=200, Jog=450, Sprint=700" If Biped default is "Combat/Interaction",
    // maybe it is Jog (450)? Let's stick to Plan: "BipedSpeeds (Walk=200,
    // Jog=450, Sprint=700)" Logic update:
    if (bIsSprinting)
      TargetMaxSpeed = BipedSpeeds.Z;
    else if (bIsWalking)
      TargetMaxSpeed = BipedSpeeds.X;
    else
      TargetMaxSpeed = BipedSpeeds.Y; // Default Biped = Jog (450)
  }

  // 2. Interpolate MaxWalkSpeed for smoothness (prevents instant snap)
  float CurrentMax = GetCharacterMovement()->MaxWalkSpeed;

  // Only interp if significant difference to avoid micro-adjustments
  if (!FMath::IsNearlyEqual(CurrentMax, TargetMaxSpeed, 1.0f)) {
    // Interp Speed: 10.0f = snappy but smooth
    float NewMax =
        FMath::FInterpTo(CurrentMax, TargetMaxSpeed, DeltaTime, 10.0f);
    GetCharacterMovement()->MaxWalkSpeed = NewMax;
  }

  // 3. Calculate Gait for Animation
  float VelocitySize = GetVelocity().Size();

  // Update Stride Length from Curve
  if (StrideCurveQuadruped) {
    CurrentStrideLength = StrideCurveQuadruped->GetFloatValue(VelocitySize);
  }

  // Hysteresis can be added here if needed
  if (VelocitySize < 10.f)
    CurrentGait = EMonkeyGait::Walk; // Idle actually
  else if (VelocitySize <= (CurrentStance == EMonkeyStance::Quadruped
                                ? QuadrupedSpeeds.X
                                : BipedSpeeds.X) +
                               50.f)
    CurrentGait = EMonkeyGait::Walk;
  else if (VelocitySize <= (CurrentStance == EMonkeyStance::Quadruped
                                ? QuadrupedSpeeds.Y
                                : BipedSpeeds.Y) +
                               50.f)
    CurrentGait = EMonkeyGait::Jog;
  else
    CurrentGait = EMonkeyGait::Sprint;
}

// ============================================================================
// PROCEDURAL ANIMATION
// ============================================================================

void ACharacterRope::UpdateProceduralAnimation(float DeltaTime) {
  // ----- IK OFFSET CALCULATION -----
  // Disable IK when falling/jumping to prevent feet stretching to ground
  const bool bIsInAir =
      GetCharacterMovement() && GetCharacterMovement()->IsFalling();

  if (bEnableIK && !bIsInAir) {
    USkeletalMeshComponent *MeshComp = GetMesh();
    if (MeshComp) {
      // Expected floor Z (bottom of capsule)
      const float ExpectedFloorZ =
          GetActorLocation().Z -
          GetCapsuleComponent()->GetScaledCapsuleHalfHeight();

      // Helper lambda to calculate IK offset for a limb
      auto CalculateLimbOffset = [this, MeshComp,
                                  ExpectedFloorZ](const FName &BoneName,
                                                  FLimbIKData &OutData) {
        const FVector BoneLocation = MeshComp->GetSocketLocation(BoneName);

        // Trace from bone down
        const FVector TraceStart =
            BoneLocation + FVector(0, 0, IK_TraceDistance);
        const FVector TraceEnd = BoneLocation - FVector(0, 0, IK_TraceDistance);

        FHitResult HitResult;
        FCollisionQueryParams QueryParams;
        QueryParams.AddIgnoredActor(this);

        const bool bHit = GetWorld()->LineTraceSingleByChannel(
            HitResult, TraceStart, TraceEnd, ECC_Visibility, QueryParams);

        if (bHit) {
          OutData.bHitGround = true;
          // Calculate offset: difference between actual ground and expected
          // floor
          const float FloorDelta = HitResult.ImpactPoint.Z - ExpectedFloorZ;
          OutData.EffectorOffset = FVector(0, 0, FloorDelta + IK_FootOffset);

          // Rotation from ground normal
          const FVector GroundNormal = HitResult.ImpactNormal;
          const FVector Forward = GetActorForwardVector();
          const FVector Right =
              FVector::CrossProduct(GroundNormal, Forward).GetSafeNormal();
          const FVector AdjustedForward =
              FVector::CrossProduct(Right, GroundNormal).GetSafeNormal();
          OutData.TargetRotation =
              FRotationMatrix::MakeFromXZ(AdjustedForward, GroundNormal)
                  .Rotator();
          OutData.Alpha = 1.0f;
        } else {
          OutData.bHitGround = false;
          OutData.EffectorOffset = FVector::ZeroVector;
          OutData.TargetRotation = FRotator::ZeroRotator;
          OutData.Alpha = 0.0f;
        }
      };

      // Calculate foot offsets
      CalculateLimbOffset(FName("foot_l1"), ProceduralData.Foot_L);
      CalculateLimbOffset(FName("foot_r1"), ProceduralData.Foot_R);

      // Calculate hand offsets (Quadruped only)
      if (CurrentStance == EMonkeyStance::Quadruped) {
        CalculateLimbOffset(FName("hand_l"), ProceduralData.Hand_L);
        CalculateLimbOffset(FName("hand_r"), ProceduralData.Hand_R);
      } else {
        ProceduralData.Hand_L.Alpha = 0.0f;
        ProceduralData.Hand_R.Alpha = 0.0f;
      }

      // Pelvis offset: use the minimum of both feet to prevent hyperextension
      ProceduralData.PelvisOffset =
          FMath::Min(ProceduralData.Foot_L.EffectorOffset.Z,
                     ProceduralData.Foot_R.EffectorOffset.Z);
    }
  } else if (bIsInAir) {
    // Reset IK when in air
    ProceduralData.Foot_L.Alpha = 0.0f;
    ProceduralData.Foot_R.Alpha = 0.0f;
    ProceduralData.Hand_L.Alpha = 0.0f;
    ProceduralData.Hand_R.Alpha = 0.0f;
    ProceduralData.PelvisOffset = 0.0f;
  }

  // ----- INERTIAL BANKING & ACCELERATION TILT -----
  if (InertialMovementComp) {
    const FInertiaState &State = InertialMovementComp->GetInertiaState();
    ProceduralData.LeanAmount.Roll = State.LeanRoll;
    ProceduralData.LeanAmount.Pitch = State.LeanPitch;
    ProceduralData.LeanAmount.Yaw =
        State.TorsoTwistYaw; // or 0.f if twist is handled separately
  }

  // ----- SWING PHASE (TODO: Integrate with RopeSystem) -----
  // ProceduralData.SwingPhase = ...;
  // ProceduralData.SwingIntensity = ...;

  // ----- LANDING ALPHA (TODO: Integrate with CMC) -----
  // Decay landing alpha
  ProceduralData.LandingAlpha =
      FMath::FInterpTo(ProceduralData.LandingAlpha, 0.0f, DeltaTime, 5.0f);
}
