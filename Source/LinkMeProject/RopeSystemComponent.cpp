// RopeSystemComponent.cpp - REFACTORED FOR BLUEPRINT LOGIC

#include "RopeSystemComponent.h"
#include "Components/CapsuleComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"
#include "RopeCameraManager.h"
#include "RopeHookActor.h"
#include "RopeRenderComponent.h"

URopeSystemComponent::URopeSystemComponent() {
  PrimaryComponentTick.bCanEverTick = true;
  SetIsReplicatedByDefault(true);
}

void URopeSystemComponent::GetLifetimeReplicatedProps(
    TArray<FLifetimeProperty> &OutLifetimeProps) const {
  Super::GetLifetimeReplicatedProps(OutLifetimeProps);

  DOREPLIFETIME(URopeSystemComponent, CurrentLength);
  DOREPLIFETIME(URopeSystemComponent, BendPoints);
  DOREPLIFETIME(URopeSystemComponent, RopeState);
  DOREPLIFETIME(URopeSystemComponent, CurrentHook);
}

void URopeSystemComponent::BeginPlay() {
  Super::BeginPlay();

  RenderComponent =
      GetOwner() ? GetOwner()->FindComponentByClass<URopeRenderComponent>()
                 : nullptr;

  if (ACharacter *OwnerChar = Cast<ACharacter>(GetOwner())) {
    if (UCharacterMovementComponent *MoveComp =
            OwnerChar->GetCharacterMovement()) {
      DefaultBrakingDeceleration = MoveComp->BrakingDecelerationFalling;
    }

    // Cache Camera Manager
    CachedCameraManager = OwnerChar->FindComponentByClass<URopeCameraManager>();
  }

  // Start Timer for physics updates (Server only for gameplay logic)
  // Only if bUseSubsteppedPhysics is TRUE
  // Timer removed for Tick-based physics (Fix Stutter)
  /*
  if (GetOwner() && GetOwner()->HasAuthority() && bUseSubsteppedPhysics)
  {
          GetWorld()->GetTimerManager().SetTimer(PhysicsTimerHandle, this,
  &URopeSystemComponent::PhysicsTick, 1.0f / PhysicsUpdateRate, true);
  }
  */
}

void URopeSystemComponent::TickComponent(
    float DeltaTime, enum ELevelTick TickType,
    FActorComponentTickFunction *ThisTickFunction) {
  Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

  // Lightweight visual updates only
  // Lightweight visual updates only
  // FIXED: Must tick if RenderComponent is active to allow hiding it
  bool bIsVisualActive = RenderComponent && RenderComponent->IsRopeActive();
  if (RopeState == ERopeState::Idle && !bIsVisualActive)
    return;

  // --- REFACTORED FOR BP API ---
  if (GetOwner()->HasAuthority()) {
    if (RopeState == ERopeState::Flying && CurrentHook) {
      // SAFETY: MaxLength Check
      float Distance = FVector::Dist(GetOwner()->GetActorLocation(),
                                     CurrentHook->GetActorLocation());
      if (Distance > MaxLength) {
        Detach();
        return;
      }

      // API Call: Let Blueprint handle collision/wrap logic
      OnRopeTickFlying(DeltaTime);

      // Server: Check for hook impact
      if (CurrentHook->HasImpacted()) {
        TransitionToAttached(CurrentHook->GetImpactResult());
      }
    } else if (RopeState == ERopeState::Attached) {
      // API Call: Let Blueprint handle wrap/unwrap logic
      OnRopeTickAttached(DeltaTime);

      // Guard against massive lag spikes for physics
      if (DeltaTime <= 0.1f) {
        PerformPhysics(DeltaTime);
      }

      // Always update player position for visual smoothness
      UpdatePlayerPosition();

      // Ensure Camera State is Sync
      if (CachedCameraManager &&
          CachedCameraManager->GetCurrentState() != ECameraState::Swinging) {
        CachedCameraManager->SetState(ECameraState::Swinging);
      }

      // Debug HUD
      if (bShowDebug && GEngine) {
        GEngine->AddOnScreenDebugMessage(
            1, 0.f, FColor::Yellow,
            FString::Printf(TEXT("Rope Length: %.1f / %.1f | BendPoints: %d"),
                            CurrentLength, MaxLength, BendPoints.Num()));
      }

      // Apex Window Detection
      UpdateApexDetection(DeltaTime);
    }
  }

  // Visual update (client + server)
  UpdateRopeVisual();
}

// Timer-based physics (Server only, called at PhysicsUpdateRate Hz)
// Timer-based physics tick (called at PhysicsUpdateRate Hz)
void URopeSystemComponent::PhysicsTick() {
  PerformPhysics(1.0f / PhysicsUpdateRate);
}

void URopeSystemComponent::PerformPhysics(float DeltaTime) {
  if (RopeState != ERopeState::Attached)
    return;

  // Heavy physics calculations
  ApplyForcesToPlayer();

  // Blueprint event for wrap/unwrap logic (Server authoritative)
  OnRopeTickAttached(DeltaTime);
}

void URopeSystemComponent::OnRep_BendPoints() {
  // Force update the visual component when the server sends new topology
  UpdateRopeVisual();
}

// ===================================================================
// PHYSICS HELPERS
// ===================================================================

void URopeSystemComponent::ReelInToFirstBendPoint() {
  if (!CurrentHook || BendPoints.Num() == 0)
    return;

  // 1. Stop projectile movement
  if (CurrentHook->ProjectileMovement) {
    CurrentHook->ProjectileMovement->StopMovementImmediately();
    CurrentHook->ProjectileMovement->Deactivate();
  }

  // 2. Get anchor position (first bendpoint created during flying wrap)
  FVector AnchorPos = BendPoints[0];

  // 2.5 Update Rope Length to prevent physics snap
  // We set the rope length to the current distance so it doesn't instantly pull
  // the player
  float NewLength = FVector::Dist(AnchorPos, GetOwner()->GetActorLocation());
  CurrentLength = NewLength;

  // 3. Teleport hook to anchor
  CurrentHook->SetActorLocation(AnchorPos);

  // 4. Clear flying bendpoints
  BendPoints.Empty();
  BendPointNormals.Empty();

  // 5. Transition to Attached state
  RopeState = ERopeState::Attached;

  // 6. Initialize attached bendpoints array [Anchor, Player]
  BendPoints.Add(AnchorPos);
  BendPoints.Add(GetOwner()->GetActorLocation());
  BendPointNormals.Add(FVector::UpVector);
  BendPointNormals.Add(FVector::UpVector);

  // Debug
  if (bShowDebug) {
    DrawDebugSphere(GetWorld(), AnchorPos, 20, 12, FColor::Green, false, 2.f);
    UE_LOG(LogTemp, Log, TEXT("REEL-IN: Hook anchored at %s"),
           *AnchorPos.ToString());
  }
}

void URopeSystemComponent::FireHook(const FVector &Direction) {
  if (!GetOwner()->HasAuthority()) {
    ServerFireHook(Direction);
    // Optional: Client prediction here (spawn fake hook)
    return;
  }
  ServerFireHook(Direction);
}

void URopeSystemComponent::ServerFireHook_Implementation(
    const FVector &Direction) {
  if (!HookClass || !GetWorld()) {
    UE_LOG(LogTemp, Error, TEXT("FireHook: HookClass or World is null"));
    return;
  }

  AActor *Owner = GetOwner();
  if (!Owner) {
    UE_LOG(LogTemp, Error, TEXT("FireHook: Owner is null"));
    return;
  }

  // --- Reset Existing Rope Logic ---
  if (CurrentHook) {
    CurrentHook->Destroy();
    CurrentHook = nullptr;
  }
  if (RenderComponent) {
    RenderComponent->ResetRope();
  }
  BendPoints.Reset();
  RopeState = ERopeState::Idle;
  // ---------------------------------

  FVector SpawnLocation = Owner->GetActorLocation() + Direction * 50.f;
  FRotator SpawnRotation = Direction.Rotation();

  // Socket Spawn Logic
  if (HandSocketName != NAME_None) {
    if (ACharacter *Char = Cast<ACharacter>(Owner)) {
      if (USkeletalMeshComponent *Mesh = Char->GetMesh()) {
        if (Mesh->DoesSocketExist(HandSocketName)) {
          SpawnLocation = Mesh->GetSocketLocation(HandSocketName);
          // We still align rotation with the aim direction, but originating
          // from the hand
          SpawnRotation = Direction.Rotation();
        }
      }
    }
  }

  FActorSpawnParameters Params;
  Params.Owner = Owner;
  Params.Instigator = Cast<APawn>(Owner);
  Params.SpawnCollisionHandlingOverride =
      ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

  CurrentHook = GetWorld()->SpawnActorDeferred<ARopeHookActor>(
      HookClass, FTransform(SpawnRotation, SpawnLocation), Owner,
      Cast<APawn>(Owner), ESpawnActorCollisionHandlingMethod::AlwaysSpawn);

  if (CurrentHook) {
    // PRE-INIT CONFIGURATION (CRITICAL FOR COLLISION IGNORE)
    if (UPrimitiveComponent *RootComp =
            Cast<UPrimitiveComponent>(CurrentHook->GetRootComponent())) {
      RootComp->IgnoreActorWhenMoving(Owner, true);
      // Explicitly ignore the CapsuleComponent AND Mesh (if spawning from hand
      // socket)
      if (ACharacter *Char = Cast<ACharacter>(Owner)) {
        if (UCapsuleComponent *Capsule = Char->GetCapsuleComponent()) {
          RootComp->IgnoreComponentWhenMoving(Capsule, true);
        }
        if (USkeletalMeshComponent *Mesh = Char->GetMesh()) {
          RootComp->IgnoreComponentWhenMoving(Mesh, true);
        }
      }
    }

    // Note: ProjectileMovement might verify updated component overlaps on init,
    // so setting ignores on the actor/root first is key. The explicit
    // MoveIgnoreActors on ProjectileMovement component must be done after
    // component creation but before activation if possible. However,
    // ProjectileMovement is created in constructor. We can't easily access the
    // component here without including the projectile header, but we can rely
    // on the Actor-level ignore we just set on the root.

    // Finish Spawning
    UGameplayStatics::FinishSpawningActor(
        CurrentHook, FTransform(SpawnRotation, SpawnLocation));

    CurrentHook->Fire(Direction);

    CurrentHook->OnHookImpact.AddDynamic(this,
                                         &URopeSystemComponent::OnHookImpact);
    RopeState = ERopeState::Flying;

    if (bShowDebug) {
      UE_LOG(LogTemp, Log, TEXT("Hook fired successfully (Server)"));
      if (GEngine)
        GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Cyan,
                                         TEXT("SERVER: Hook Fired!"));
    }
  }
}

void URopeSystemComponent::FireChargedHook(const FVector &Velocity) {
  if (!GetOwner()->HasAuthority()) {
    ServerFireChargedHook(Velocity);
    return;
  }
  ServerFireChargedHook(Velocity);
}

void URopeSystemComponent::ServerFireChargedHook_Implementation(
    const FVector &Velocity) {
  UE_LOG(LogTemp, Warning,
         TEXT("URopeSystemComponent::ServerFireChargedHook called with "
              "Velocity: %s"),
         *Velocity.ToString());

  if (!HookClass || !GetWorld()) {
    if (GEngine)
      GEngine->AddOnScreenDebugMessage(
          -1, 5.0f, FColor::Red,
          TEXT("[SERVER] ERROR: HookClass or World is null!"));
    UE_LOG(LogTemp, Error,
           TEXT("ServerFireChargedHook: HookClass or World is null"));
    return;
  }

  AActor *Owner = GetOwner();
  if (!Owner)
    return;

  // Reset Logic
  if (CurrentHook) {
    CurrentHook->Destroy();
    CurrentHook = nullptr;
  }
  if (RenderComponent)
    RenderComponent->ResetRope();
  BendPoints.Reset();
  RopeState = ERopeState::Idle;

  // Spawn
  // Use a safer offset to avoid initial overlap (Capsule Radius is usually
  // ~34-40)
  const FVector SpawnLocation =
      Owner->GetActorLocation() + Velocity.GetSafeNormal() * 100.f;
  const FRotator SpawnRotation = Velocity.Rotation();

  FActorSpawnParameters Params;
  Params.Owner = Owner;
  Params.Instigator = Cast<APawn>(Owner);

  CurrentHook = GetWorld()->SpawnActor<ARopeHookActor>(HookClass, SpawnLocation,
                                                       SpawnRotation, Params);
  if (CurrentHook) {
    CurrentHook->FireVelocity(Velocity);
    CurrentHook->OnHookImpact.AddDynamic(this,
                                         &URopeSystemComponent::OnHookImpact);
    RopeState = ERopeState::Flying;

    if (bShowDebug) {
      if (GEngine)
        GEngine->AddOnScreenDebugMessage(
            -1, 3.0f, FColor::Green, TEXT("[SERVER] Hook Spawned & Fired!"));
      UE_LOG(
          LogTemp, Warning,
          TEXT("ServerFireChargedHook: Hook spawned and fired successfully"));
    }
  } else if (bShowDebug) {
    if (GEngine)
      GEngine->AddOnScreenDebugMessage(
          -1, 5.0f, FColor::Red, TEXT("[SERVER] ERROR: Failed to Spawn Hook!"));
    UE_LOG(LogTemp, Error,
           TEXT("ServerFireChargedHook: Failed to spawn Hook Actor"));
  }
}

void URopeSystemComponent::Sever() {
  // Client-side visual reset for instant feedback
  if (RenderComponent) {
    RenderComponent->ResetRope();
  }

  if (!GetOwner()->HasAuthority()) {
    ServerSever();
    return;
  }
  ServerSever();
}

void URopeSystemComponent::SwingJump(float BaseBoostMultiplier) {
  // Only works when attached (swinging)
  if (RopeState != ERopeState::Attached) {
    // Fallback to normal sever if not swinging
    Sever();
    return;
  }

  // Calculate final boost based on apex timing
  float FinalBoost = BaseBoostMultiplier;
  EApexTier Tier = EApexTier::None;
  float BoostPercent = 0.f;

  if (bIsInApexWindow) {
    // Calculate progress through window (0 = just entered, 1 = end of window)
    float Progress = FMath::Clamp(ApexWindowTimer / ApexFrameTime, 0.f, 1.f);

    // Get curve-based boost (if curve exists)
    float CurveValue = 1.f;
    if (ApexBoostCurve) {
      CurveValue = ApexBoostCurve->GetFloatValue(Progress);
    } else {
      // Fallback: linear decay (early = high boost)
      CurveValue = 1.f - Progress;
    }

    // CurveValue is 0-1, represents boost percentage
    BoostPercent = CurveValue;

    // Determine tier from boost percentage
    Tier = DetermineTierFromBoost(BoostPercent);

    // Apply apex boost on top of base
    FinalBoost = BaseBoostMultiplier + (MaxApexBoost - 1.f) * CurveValue;
  }

  // Get current velocity and apply boost
  if (ACharacter *OwnerChar = Cast<ACharacter>(GetOwner())) {
    if (UCharacterMovementComponent *MoveComp =
            OwnerChar->GetCharacterMovement()) {
      FVector CurrentVel = MoveComp->Velocity;
      float CurrentSpeed = CurrentVel.Size();

      // Apply directional boost (keep direction, multiply magnitude)
      FVector BoostedVel =
          CurrentVel.GetSafeNormal() * CurrentSpeed * FinalBoost;
      MoveComp->Velocity = BoostedVel;

      // Debug
      if (bShowDebug) {
        FString TierStr;
        switch (Tier) {
        case EApexTier::Perfect:
          TierStr = TEXT("PERFECT");
          break;
        case EApexTier::Good:
          TierStr = TEXT("Good");
          break;
        case EApexTier::OK:
          TierStr = TEXT("OK");
          break;
        default:
          TierStr = TEXT("None");
          break;
        }
        UE_LOG(LogTemp, Log, TEXT("SWINGJUMP [%s]: Speed %.0f -> %.0f (x%.2f)"),
               *TierStr, CurrentSpeed, BoostedVel.Size(), FinalBoost);
        if (GEngine) {
          FColor TierColor = (Tier == EApexTier::Perfect) ? FColor::Green
                             : (Tier == EApexTier::Good)  ? FColor::Yellow
                                                          : FColor::Cyan;
          GEngine->AddOnScreenDebugMessage(
              -1, 2.f, TierColor,
              FString::Printf(TEXT("SwingJump [%s] %.0f -> %.0f"), *TierStr,
                              CurrentSpeed, BoostedVel.Size()));
        }
      }
    }
  }

  // Trigger events
  OnApexJump.Broadcast(Tier);
  OnApexJump_Client(Tier);

  // Reset apex state
  bIsInApexWindow = false;
  ApexWindowTimer = 0.f;

  // Then sever
  Sever();
}

void URopeSystemComponent::UpdateApexDetection(float DeltaTime) {
  if (RopeState != ERopeState::Attached) {
    bIsInApexWindow = false;
    ApexWindowTimer = 0.f;
    return;
  }

  ACharacter *OwnerChar = Cast<ACharacter>(GetOwner());
  if (!OwnerChar)
    return;

  // Calculate swing arc position (0 = bottom, 0.5 = top/apex, 1 = bottom again)
  // Use normalized height relative to anchor point
  FVector PlayerPos = OwnerChar->GetActorLocation();
  FVector AnchorPos =
      (BendPoints.Num() > 0)
          ? BendPoints[0]
          : (CurrentHook ? CurrentHook->GetActorLocation() : PlayerPos);

  float VerticalDiff = PlayerPos.Z - AnchorPos.Z;
  float RopeLen = FMath::Max(CurrentLength, 1.f);

  // Normalize: -1 (at anchor) to +1 (below anchor by rope length)
  // Then convert to 0-1 arc position: 0 = lowest, 0.5 = apex (at anchor level),
  // 1 = lowest
  float NormalizedHeight = FMath::Clamp(VerticalDiff / RopeLen, -1.f, 1.f);

  // Convert to arc position: when player is at anchor height
  // (NormalizedHeight=0), that's apex (0.5) When below anchor, going up: 0 ->
  // 0.5 When below anchor, going down: 0.5 -> 1
  float ArcPosition = 0.5f - (NormalizedHeight * 0.5f);

  // Check if in apex window based on arc position
  bool bShouldBeInWindow =
      (ArcPosition >= SwingArcApexStart && ArcPosition <= SwingArcApexEnd);

  if (bShouldBeInWindow) {
    if (!bIsInApexWindow) {
      // Enter apex window
      bIsInApexWindow = true;
      ApexWindowTimer = 0.f;

      if (bShowDebug && GEngine) {
        GEngine->AddOnScreenDebugMessage(
            -1, 0.5f, FColor::Magenta,
            FString::Printf(TEXT("APEX WINDOW OPEN (Arc: %.2f)"), ArcPosition));
      }
    } else {
      // Continue in window
      ApexWindowTimer += DeltaTime;
      if (ApexWindowTimer > ApexFrameTime) {
        // Exit window (timed out)
        bIsInApexWindow = false;
        if (bShowDebug && GEngine) {
          GEngine->AddOnScreenDebugMessage(
              -1, 0.5f, FColor::Red, TEXT("Apex window closed (timeout)"));
        }
      }
    }
  } else {
    // Exit window (outside arc range)
    if (bIsInApexWindow) {
      bIsInApexWindow = false;
      if (bShowDebug && GEngine) {
        GEngine->AddOnScreenDebugMessage(
            -1, 0.5f, FColor::Orange,
            FString::Printf(TEXT("Apex window closed (Arc: %.2f)"),
                            ArcPosition));
      }
    }
  }
}

EApexTier
URopeSystemComponent::DetermineTierFromBoost(float BoostPercent) const {
  if (BoostPercent >= PerfectBoostThreshold) {
    return EApexTier::Perfect;
  } else if (BoostPercent >= GoodBoostThreshold) {
    return EApexTier::Good;
  }
  return EApexTier::OK;
}

void URopeSystemComponent::Detach() {
  if (CurrentHook) {
    // 1. Notify Hook that it is now "orphaned" (Visual FX)
    CurrentHook->OnRopeDetached();

    // 2. Unbind events to stop physics updates from this hook
    CurrentHook->OnHookImpact.RemoveDynamic(
        this, &URopeSystemComponent::OnHookImpact);

    // 3. Give it a life span so it cleans itself up eventually
    CurrentHook->SetLifeSpan(3.0f);

    // 4. Forget about it
    CurrentHook = nullptr;
  }

  // 5. Normal cleanup for player side
  if (RenderComponent) {
    RenderComponent->ResetRope();
  }

  BendPoints.Reset();
  BendPointNormals.Reset();
  CurrentLength = 0.f;
  RopeState = ERopeState::Idle;

  // No need to reset movement physics here as we were likely flying (AIR)
  // anyway, but good safety to ensure camera reset if we were somehow attached.
  if (ACharacter *OwnerChar = Cast<ACharacter>(GetOwner())) {
    if (URopeCameraManager *CamMgr =
            OwnerChar->FindComponentByClass<URopeCameraManager>()) {
      CamMgr->SetState(ECameraState::Grounded);
    }
  }
}

void URopeSystemComponent::ServerSever_Implementation() {
  if (CurrentHook) {
    CurrentHook->Destroy();
    CurrentHook = nullptr;
  }

  if (RenderComponent) {
    RenderComponent->ResetRope();
  }

  BendPoints.Reset();
  BendPointNormals.Reset(); // Keep normals array in sync
  CurrentLength = 0.f;
  RopeState = ERopeState::Idle;

  // Restore movement settings
  if (ACharacter *OwnerChar = Cast<ACharacter>(GetOwner())) {
    if (UCharacterMovementComponent *MoveComp =
            OwnerChar->GetCharacterMovement()) {
      MoveComp->BrakingDecelerationFalling = DefaultBrakingDeceleration;
    }

    // Notify camera: back to grounded
    if (URopeCameraManager *CamMgr =
            OwnerChar->FindComponentByClass<URopeCameraManager>()) {
      CamMgr->SetState(ECameraState::Grounded);
    }
  }

  // This event should probably be multicast if visual effects are needed
  OnRopeSevered();
}

void URopeSystemComponent::ReelIn(float DeltaTime) {
  if (!GetOwner()->HasAuthority()) {
    ServerReelIn(DeltaTime);
    return;
  }
  ServerReelIn(DeltaTime);
}

void URopeSystemComponent::ServerReelIn_Implementation(float DeltaTime) {
  CurrentLength = FMath::Max(0.f, CurrentLength - ReelSpeed * DeltaTime);
}

void URopeSystemComponent::ReelOut(float DeltaTime) {
  if (!GetOwner()->HasAuthority()) {
    ServerReelOut(DeltaTime);
    return;
  }
  ServerReelOut(DeltaTime);
}

void URopeSystemComponent::ServerReelOut_Implementation(float DeltaTime) {
  CurrentLength = FMath::Min(MaxLength, CurrentLength + ReelSpeed * DeltaTime);
}

// ===================================================================
// BENDPOINT MANAGEMENT
// ===================================================================

void URopeSystemComponent::AddBendPoint(const FVector &Location) {
  // Delegate to the full version with a default normal
  AddBendPointWithNormal(Location, FVector::UpVector);
}

void URopeSystemComponent::AddBendPointWithNormal(
    const FVector &Location, const FVector &SurfaceNormal) {
  // FLYING STATE: BendPoints may be empty, just append to end
  if (RopeState == ERopeState::Flying) {
    BendPoints.Add(Location);
    BendPointNormals.Add(SurfaceNormal);

    if (bShowDebug) {
      DrawDebugSphere(GetWorld(), Location, 12, 12, FColor::Yellow, false, 2.f);
      DrawDebugLine(GetWorld(), Location, Location + SurfaceNormal * 30.f,
                    FColor::Cyan, false, 2.f);
      UE_LOG(LogTemp, Log, TEXT("FLYING WRAP: Added bendpoint at %s"),
             *Location.ToString());
    }
    return;
  }

  // ATTACHED STATE: Insert before player position (existing logic)
  if (BendPoints.Num() < 2) {
    UE_LOG(LogTemp, Warning,
           TEXT("AddBendPoint: Need at least 2 points (anchor + player)"));
    return;
  }

  // CRITICAL: Ensure normals array is same size as positions array
  // This handles the case where rope was initialized before this function was
  // implemented
  while (BendPointNormals.Num() < BendPoints.Num()) {
    BendPointNormals.Add(FVector::UpVector); // Fill with default normals
  }

  // Insert position before the last element (player position)
  const int32 InsertIndex = BendPoints.Num() - 1;
  BendPoints.Insert(Location, InsertIndex);

  // Keep normals array in sync - now safe because we ensured same size
  BendPointNormals.Insert(SurfaceNormal, InsertIndex);

  if (bShowDebug) {
    DrawDebugSphere(GetWorld(), Location, 12, 12, FColor::Green, false, 2.f);
    DrawDebugLine(GetWorld(), Location, Location + SurfaceNormal * 30.f,
                  FColor::Cyan, false, 2.f);
    UE_LOG(LogTemp, Log, TEXT("WRAP: Added bendpoint at %s with normal %s"),
           *Location.ToString(), *SurfaceNormal.ToString());
  }
}

void URopeSystemComponent::RemoveBendPointAt(int32 Index) {
  if (!BendPoints.IsValidIndex(Index)) {
    UE_LOG(LogTemp, Warning, TEXT("RemoveBendPointAt: Invalid index %d"),
           Index);
    return;
  }

  // Protect anchor (0) and player (last)
  if (Index == 0 || Index == BendPoints.Num() - 1) {
    UE_LOG(LogTemp, Warning,
           TEXT("RemoveBendPointAt: Cannot remove anchor or player point"));
    return;
  }

  BendPoints.RemoveAt(Index);

  // Keep normals array in sync
  if (BendPointNormals.IsValidIndex(Index)) {
    BendPointNormals.RemoveAt(Index);
  }

  if (bShowDebug) {
    UE_LOG(LogTemp, Log, TEXT("UNWRAP: Removed bendpoint at index %d"), Index);
  }
}

FVector URopeSystemComponent::GetLastFixedPoint() const {
  if (BendPoints.Num() < 2)
    return FVector::ZeroVector;
  return BendPoints[BendPoints.Num() - 2];
}

FVector URopeSystemComponent::GetPlayerPosition() const {
  if (BendPoints.Num() < 1) {
    // Fallback to owner location if no bends exist (e.g. Flying start)
    if (GetOwner())
      return GetOwner()->GetActorLocation();
    return FVector::ZeroVector;
  }
  return BendPoints.Last();
}

FVector URopeSystemComponent::GetAnchorPosition() const {
  if (BendPoints.Num() < 1)
    return FVector::ZeroVector;
  return BendPoints[0];
}

void URopeSystemComponent::UpdatePlayerPosition() {
  if (BendPoints.Num() < 1 || !GetOwner())
    return;
  BendPoints.Last() = GetOwner()->GetActorLocation();
}

// ===================================================================
// TRACE UTILITIES
// ===================================================================

bool URopeSystemComponent::CapsuleSweepBetween(const FVector &Start,
                                               const FVector &End,
                                               FHitResult &OutHit, float Radius,
                                               bool bTraceComplex) {
  if (!GetWorld())
    return false;

  FCollisionQueryParams Params(SCENE_QUERY_STAT(RopeTrace), bTraceComplex,
                               GetOwner());
  Params.AddIgnoredActor(GetOwner());
  if (CurrentHook)
    Params.AddIgnoredActor(CurrentHook);

  FCollisionShape Capsule = FCollisionShape::MakeCapsule(Radius, Radius * 2.f);

  bool bHit = GetWorld()->SweepSingleByChannel(
      OutHit, Start, End, FQuat::Identity, RopeTraceChannel, Capsule, Params);

  if (bShowDebug && bHit) {
    DrawDebugCapsule(GetWorld(), OutHit.ImpactPoint, Radius * 2.f, Radius,
                     FQuat::Identity, FColor::Orange, false, 1.f);
  }

  return bHit && OutHit.bBlockingHit && !OutHit.bStartPenetrating;
}

FVector URopeSystemComponent::FindLastClearPoint(const FVector &Start,
                                                 const FVector &End,
                                                 int32 Subdivisions,
                                                 float SphereRadius,
                                                 bool bShowDebugDraw) {
  if (!GetWorld())
    return Start;

  Subdivisions = FMath::Max(1, Subdivisions);
  FVector LastClear = Start;

  for (int32 i = 1; i <= Subdivisions; ++i) {
    const float Alpha =
        static_cast<float>(i) / static_cast<float>(Subdivisions);
    const FVector TestPoint = FMath::Lerp(Start, End, Alpha);

    FHitResult Hit;
    FCollisionQueryParams Params(SCENE_QUERY_STAT(RopeSubstepTrace), false,
                                 GetOwner());
    if (CurrentHook)
      Params.AddIgnoredActor(CurrentHook);

    FCollisionShape Sphere = FCollisionShape::MakeSphere(SphereRadius);

    bool bHit = GetWorld()->SweepSingleByChannel(
        Hit, TestPoint, TestPoint + FVector(0, 0, 1), // Minimal sweep
        FQuat::Identity, RopeTraceChannel, Sphere, Params);

    if (!bHit || !Hit.bBlockingHit) {
      LastClear = TestPoint;
    } else {
      break; // Hit geometry, stop
    }

    if (bShowDebugDraw) {
      DrawDebugSphere(GetWorld(), TestPoint, SphereRadius, 8,
                      bHit ? FColor::Red : FColor::Green, false, 0.5f);
    }
  }

  return LastClear;
}

FVector URopeSystemComponent::ComputeBendPointFromHit(const FHitResult &Hit,
                                                      float Offset) const {
  return Hit.ImpactPoint + Hit.ImpactNormal * Offset;
}

// ===================================================================
// PHYSICS
// ===================================================================

void URopeSystemComponent::ApplyForcesToPlayer() {
  ACharacter *OwnerChar = Cast<ACharacter>(GetOwner());
  if (!OwnerChar || BendPoints.Num() < 2) {
    return;
  }

  // Calculate total physical length
  float TotalPhysicalLength = 0.f;
  for (int32 i = 0; i < BendPoints.Num() - 1; ++i) {
    TotalPhysicalLength += FVector::Distance(BendPoints[i], BendPoints[i + 1]);
  }

  // Force direction towards last fixed point
  const FVector PlayerPos = BendPoints.Last();
  const FVector LastFixedPoint = BendPoints[BendPoints.Num() - 2];
  const FVector DirToAnchor = (LastFixedPoint - PlayerPos).GetSafeNormal();

  // Calculate stretch
  const float Stretch = TotalPhysicalLength - CurrentLength;

  if (Stretch <= 0.f && SwingSettings.CentripetalBias <= 0.f)
    return;

  if (UCharacterMovementComponent *MoveComp =
          OwnerChar->GetCharacterMovement()) {
    // 1. Core Physics: Disable air friction
    MoveComp->BrakingDecelerationFalling = 0.f;

    // 2. Calculate "Magic Forces"
    FVector InputVector = MoveComp->GetLastInputVector();
    FVector SwingForce = CalculateSwingForces(MoveComp->Velocity, InputVector);

    // 3. Apply Forces
    MoveComp->AddForce(SwingForce);

    // 4. Handle Rope Constraint (Tension)
    // 4. Handle Rope Constraint (Tension)
    if (Stretch > 0.f) {
      // HARD CONSTRAINT: Velocity Projection (No Bounce)
      // Kill any velocity component moving away from the anchor
      const float RadialSpeed =
          FVector::DotProduct(MoveComp->Velocity, DirToAnchor);

      // Note: DirToAnchor points TO the anchor.
      // RadialSpeed < 0 means velocity is OPPOSITE to DirToAnchor (moving
      // AWAY).
      if (RadialSpeed < 0.f) {
        // Flatten velocity to be tangent only
        FVector TangentVel =
            FVector::VectorPlaneProject(MoveComp->Velocity, DirToAnchor);
        MoveComp->Velocity = TangentVel;

        // Positional Correction (Optional but helps prevent drift)
        // Ideally handled by Sweep/Slide, but here we can just pull back
        // slighty if needed For now, velocity projection is the key to removing
        // "bounce".
      }

      // Still apply a small spring force for valid tension (Gravity
      // counter-balance) But much stronger to act as a rod
      const FVector TensionForce =
          DirToAnchor * (Stretch * 20000.f); // Increased stiffness
      MoveComp->AddForce(TensionForce);
    }

    // 5. DEBUG VISUALIZATION
    if (bShowDebug) {
      FVector DebugPlayerPos = OwnerChar->GetActorLocation();

      // Velocity (Green)
      DrawDebugLine(GetWorld(), DebugPlayerPos,
                    DebugPlayerPos + MoveComp->Velocity, FColor::Green, false,
                    -1.f, 0, 2.f);

      // Input Control (Yellow) -> Force applied from input/swing logic
      DrawDebugLine(GetWorld(), DebugPlayerPos,
                    DebugPlayerPos + SwingForce * 0.1f, FColor::Yellow, false,
                    -1.f, 0, 2.f);

      // Tangent Direction (Blue) - Where we are actually moving
      // Fixed: checking VelocityDir instead of arbitrary horizontal tangent
      FVector VelocityDir = MoveComp->Velocity.GetSafeNormal();
      if (VelocityDir.IsNearlyZero()) {
        // Fallback if stationary: Perpendicular to rope in a "forward" looking
        // way
        FVector Forward = OwnerChar->GetActorForwardVector();
        VelocityDir =
            FVector::CrossProduct(DirToAnchor,
                                  FVector::CrossProduct(Forward, DirToAnchor))
                .GetSafeNormal();
      }
      DrawDebugLine(GetWorld(), DebugPlayerPos,
                    DebugPlayerPos + VelocityDir * 500.f, FColor::Blue, false,
                    -1.f, 0, 1.f);

      // Speed Display
      if (GEngine) {
        float Speed = MoveComp->Velocity.Size();
        FColor SpeedColor =
            Speed > 2000.f ? FColor::Red
                           : (Speed > 1000.f ? FColor::Yellow : FColor::White);
        GEngine->AddOnScreenDebugMessage(
            2, 0.f, SpeedColor,
            FString::Printf(TEXT("SPEED: %.0f cm/s"), Speed));
      }
    }
  }
}

FVector
URopeSystemComponent::CalculateSwingForces(const FVector &CurrentVelocity,
                                           const FVector &InputVector) const {
  FVector TotalForce = FVector::ZeroVector;

  // 1. Gravity (Reinforced for "Heavy" feel)
  // Note: CMC already applies gravity, but we might want extra gravity during
  // swing Use GravityScale from CMC usually, but here we invoke Swing logic
  // Applying manual gravity on top can be risky, better to rely on CMC
  // GravityScale.

  // 2. Tangential Boost (Pump)
  if (!InputVector.IsNearlyZero()) {
    FVector VelocityDir = CurrentVelocity.GetSafeNormal();

    // Project input onto velocity direction
    float Alignment = FVector::DotProduct(InputVector, VelocityDir);

    // Only boost if input aligns with movement (Pumping)
    if (Alignment > 0.f) {
      TotalForce += VelocityDir * (Alignment * SwingSettings.TangentialBoost);
    }
  }

  // 3. Air Control (Steering)
  // Force perpendicular to velocity but aligned with input
  if (!InputVector.IsNearlyZero()) {
    FVector SteeringDir = InputVector;
    if (!CurrentVelocity.IsNearlyZero()) {
      SteeringDir = FVector::VectorPlaneProject(InputVector, FVector::UpVector)
                        .GetSafeNormal();
    }
    TotalForce += SteeringDir * SwingSettings.AirControlForce;
  }

  // 4. Centripetal Bias (Artificial Tension)
  // Keeps the player moving in a circle even if rope slackens slightly
  if (BendPoints.Num() >= 2) {
    FVector PlayerPos = BendPoints.Last();
    FVector AnchorPos = BendPoints[BendPoints.Num() - 2];
    FVector DirToAnchor = (AnchorPos - PlayerPos).GetSafeNormal();
    TotalForce +=
        DirToAnchor * SwingSettings.CentripetalBias * 1000.f; // Scaled
  }

  // 5. Velocity Damping (Air Resistance)
  // Less friction in forward direction, more in others?
  // For now, simple drag opposed to velocity
  if (!CurrentVelocity.IsNearlyZero()) {
    TotalForce -= CurrentVelocity * SwingSettings.VelocityDamping;
  }

  return TotalForce;
}

void URopeSystemComponent::ApplySwingVelocity(const FVector &NewVelocity) {
  if (ACharacter *Owner = Cast<ACharacter>(GetOwner())) {
    if (UCharacterMovementComponent *CMC = Owner->GetCharacterMovement()) {
      CMC->Velocity = NewVelocity;
    }
  }
}

// ===================================================================
// INTERNAL
// ===================================================================

void URopeSystemComponent::OnHookImpact(const FHitResult &Hit) {
  TransitionToAttached(Hit);
}

void URopeSystemComponent::TransitionToAttached(const FHitResult &Hit) {
  AActor *Owner = GetOwner();
  if (!Owner)
    return;

  // BendPoints.Reset(); // REMOVED: Preserve Flying Bends!

  FVector HookImpactPoint = Hit.ImpactPoint;
  FVector PlayerPosition = Owner->GetActorLocation();

  FVector OffsetDebugY =
      FVector(0, 0, 50); // Décalage visuel pour les traces debug

  // ==========================================================
  // DEBUG: Draw incoming impact line
  // ==========================================================
  if (bShowDebug) {
    DrawDebugSphere(GetWorld(), HookImpactPoint, 12.f, 12, FColor::Red, false,
                    5.f, 0, 1.5f);
    DrawDebugLine(GetWorld(), PlayerPosition + OffsetDebugY,
                  HookImpactPoint + OffsetDebugY, FColor::Red, false, 3.f, 0,
                  2.f);
    DrawDebugPoint(GetWorld(), HookImpactPoint + OffsetDebugY, 12.f,
                   FColor::Red, false, 4.f);

    UE_LOG(LogTemp, Log, TEXT("[TransitionToAttached] HookImpact: %s"),
           *HookImpactPoint.ToString());
  }

  // ==========================================================
  // Correct Anchor using FindLastClearPoint (with debug)
  // ==========================================================

  TArray<FVector> SubstepDebugPoints; // <- Collecte des substeps

  FVector CorrectedAnchor = FindLastClearPoint(PlayerPosition, HookImpactPoint,
                                               25, // Subdivisions
                                               5.f // Sphere radiu
  );

  // ==========================================================
  // DEBUG: Draw Substep Points
  // ==========================================================
  if (bShowDebug) {
    for (int32 i = 0; i < SubstepDebugPoints.Num(); i++) {
      const FVector &P = SubstepDebugPoints[i];
      DrawDebugSphere(GetWorld(), P + OffsetDebugY, 6.f, 8, FColor::Yellow,
                      false, 3.f, 0, 1.f);

      if (i > 0) {
        DrawDebugLine(GetWorld(), SubstepDebugPoints[i - 1] + OffsetDebugY,
                      P + OffsetDebugY, FColor::Yellow, false, 3.f, 0, 0.5f);
      }
    }

    UE_LOG(LogTemp, Log, TEXT("[TransitionToAttached] Substeps: %d points"),
           SubstepDebugPoints.Num());
  }

  // ==========================================================
  // Offset from normal
  // ==========================================================

  if (Hit.bBlockingHit && !Hit.ImpactNormal.IsNearlyZero()) {
    CorrectedAnchor += Hit.ImpactNormal * 15.f;

    if (bShowDebug) {
      DrawDebugLine(GetWorld(), CorrectedAnchor + OffsetDebugY,
                    CorrectedAnchor + OffsetDebugY + Hit.ImpactNormal * 50.f,
                    FColor::Cyan, false, 3.f, 0, 0.5f);
    }
  }

  // ==========================================================
  // Final BendPoint setup (Preserve Flying Wraps!)
  // ==========================================================

  // 1. Capture Flying Bends (Order: Near Player -> Near Hook)
  TArray<FVector> FlyingBends = BendPoints;
  TArray<FVector> FlyingNormals = BendPointNormals;

  // 2. Reset
  BendPoints.Reset();
  BendPointNormals.Reset();

  // 3. Add Anchor (Start)
  BendPoints.Add(CorrectedAnchor);
  BendPointNormals.Add(Hit.ImpactNormal); // Anchor normal

  // 4. Append Flying Bends REVERSED (to match Order: Anchor -> Player)
  for (int32 i = FlyingBends.Num() - 1; i >= 0; --i) {
    BendPoints.Add(FlyingBends[i]);
    if (FlyingNormals.IsValidIndex(i)) {
      BendPointNormals.Add(FlyingNormals[i]);
    } else {
      BendPointNormals.Add(FVector::UpVector);
    }
  }

  // 5. Add Player (End)
  BendPoints.Add(PlayerPosition);
  BendPointNormals.Add(FVector::UpVector); // Player normal (dummy)

  // Calculate total length across all bends
  float TotalDist = 0.f;
  for (int32 i = 0; i < BendPoints.Num() - 1; ++i) {
    TotalDist += FVector::Dist(BendPoints[i], BendPoints[i + 1]);
  }
  CurrentLength = FMath::Min(MaxLength, TotalDist);

  RopeState = ERopeState::Attached;

  // Notify camera: enter swinging state + hook attach effect
  if (ACharacter *OwnerChar = Cast<ACharacter>(GetOwner())) {
    if (URopeCameraManager *CamMgr =
            OwnerChar->FindComponentByClass<URopeCameraManager>()) {
      CamMgr->SetState(ECameraState::Swinging);
      CamMgr->ApplyTransientEffect(FName("HookAttach"), 0.f,
                                   FVector(5.f, 0.f, 0.f), 0.1f);
    }
  }

  // ==========================================================
  // Debug final anchor
  // ==========================================================
  if (bShowDebug) {
    UE_LOG(LogTemp, Log, TEXT("[TransitionToAttached] CorrectedAnchor: %s"),
           *CorrectedAnchor.ToString());

    DrawDebugSphere(GetWorld(), CorrectedAnchor, 14.f, 16, FColor::Green, false,
                    5.f, 0, 2.f);

    DrawDebugLine(GetWorld(), HookImpactPoint + OffsetDebugY,
                  CorrectedAnchor + OffsetDebugY, FColor::Blue, false, 3.f, 0,
                  2.f);
  }

  // Notify BP / Other systems
  OnRopeAttached(Hit);
}

// ===================================================================
// SURFACE NORMAL VALIDATION - Implementation
// ===================================================================

FVector URopeSystemComponent::CalculatePressureDirection(
    const FVector &PointA, const FVector &PointB, const FVector &PointP) {
  // Calculate unit vectors from B towards A and P
  FVector DirToA = (PointA - PointB).GetSafeNormal();
  FVector DirToP = (PointP - PointB).GetSafeNormal();

  // The bisector (pressure direction) is the sum of these unit vectors
  // If the rope is perfectly straight (180°), this will be ZeroVector
  FVector Bisector = DirToA + DirToP;

  // Normalize to get the direction of force
  return Bisector.GetSafeNormal();
}

bool URopeSystemComponent::IsRopePullingAway(const FVector &PressureDir,
                                             const FVector &SurfaceNormal,
                                             float Tolerance) {
  // Edge Case: If rope is perfectly straight, pressure dir is zero
  // In this case, geometry no longer constrains the rope -> Safe to unwrap
  if (PressureDir.IsNearlyZero(0.01f)) {
    return true;
  }

  // Dot Product:
  // < 0 : Pressure and Normal are opposite (rope pushes INTO wall)
  // > 0 : Pressure and Normal are same direction (rope pulls AWAY from wall)
  float WallPressure = FVector::DotProduct(PressureDir, SurfaceNormal);

  // If WallPressure < Tolerance (e.g., -0.05), rope is still pressed against
  // wall We require WallPressure >= Tolerance to unwrap
  return WallPressure >= Tolerance;
}

bool URopeSystemComponent::ShouldUnwrapPhysical(
    const FVector &PrevFixed, const FVector &CurrentBend,
    const FVector &CurrentBendNormal, const FVector &PlayerPos,
    float AngleThreshold, bool bCheckLineTrace) {
  // ============================================================
  // TIER 1: ANGLE CHECK (Hysteresis - Fast Rejection)
  // ============================================================
  FVector DirA = (PrevFixed - CurrentBend).GetSafeNormal();
  FVector DirP = (PlayerPos - CurrentBend).GetSafeNormal();

  float DotAlignment = FVector::DotProduct(DirA, DirP);

  // If the angle is too sharp (e.g., > 2° or Dot > -0.999), reject immediately
  // This prevents unwrapping when we're still wrapping around a corner
  if (DotAlignment > AngleThreshold) {
    return false; // Angle too sharp, keep bend point
  }

  // ============================================================
  // TIER 2: SURFACE NORMAL CHECK (Anti-Tunneling)
  // ============================================================
  FVector PressureDir =
      CalculatePressureDirection(PrevFixed, CurrentBend, PlayerPos);

  if (!IsRopePullingAway(PressureDir, CurrentBendNormal, -0.05f)) {
// Rope is still pushing against the wall
// Even if angle looks flat, the physics says we're still constrained
#if WITH_EDITOR
    if (bShowDebug && GetWorld()) {
      // Debug: Show why we're blocked
      DrawDebugLine(GetWorld(), CurrentBend, CurrentBend + PressureDir * 50.f,
                    FColor::Red, false, 1.f, 0, 2.f);
      DrawDebugLine(GetWorld(), CurrentBend,
                    CurrentBend + CurrentBendNormal * 50.f, FColor::Blue, false,
                    1.f, 0, 2.f);
      DrawDebugString(GetWorld(), CurrentBend + FVector(0, 0, 30),
                      TEXT("BLOCKED: Rope Pushing"), nullptr, FColor::Red, 1.f);
    }
#endif

    return false; // Blocked by surface normal check
  }

  // ============================================================
  // TIER 3: LINE TRACE (Final Safety - Detect Other Obstacles)
  // ============================================================
  if (bCheckLineTrace && GetWorld()) {
    FHitResult Hit;
    FCollisionQueryParams Params(SCENE_QUERY_STAT(RopeUnwrapTrace), false,
                                 GetOwner());

    bool bBlocked = GetWorld()->LineTraceSingleByChannel(
        Hit, PrevFixed, PlayerPos, ECC_Visibility, Params);

    if (bBlocked && Hit.bBlockingHit) {
#if WITH_EDITOR
      if (bShowDebug) {
        DrawDebugLine(GetWorld(), PrevFixed, PlayerPos, FColor::Orange, false,
                      1.f, 0, 2.f);
        DrawDebugSphere(GetWorld(), Hit.ImpactPoint, 10.f, 8, FColor::Orange,
                        false, 1.f);
        DrawDebugString(GetWorld(), Hit.ImpactPoint + FVector(0, 0, 30),
                        TEXT("BLOCKED: Other Obstacle"), nullptr,
                        FColor::Orange, 1.f);
      }
#endif

      return false; // Path blocked by another obstacle
    }
  }

// ============================================================
// ALL CHECKS PASSED - SAFE TO UNWRAP
// ============================================================
#if WITH_EDITOR
  if (bShowDebug && GetWorld()) {
    DrawDebugLine(GetWorld(), PrevFixed, PlayerPos, FColor::Green, false, 1.f,
                  0, 3.f);
    DrawDebugString(GetWorld(), CurrentBend + FVector(0, 0, 30),
                    TEXT("UNWRAP OK"), nullptr, FColor::Green, 1.f);
  }
#endif

  return true;
}

void URopeSystemComponent::UpdateRopeVisual() {
  if (!RenderComponent) {
    RenderComponent =
        GetOwner() ? GetOwner()->FindComponentByClass<URopeRenderComponent>()
                   : nullptr;
    if (!RenderComponent)
      return;
  }

  TArray<FVector> PointsToRender;
  bool bShouldRender = false;
  bool bIsDeploying = false;

  // 1. Determine Points & Behavior based on State
  if (RopeState == ERopeState::Flying) {
    if (CurrentHook && GetOwner()) {
      // Flying Wraps Support
      // Order: Player -> [Intermediate Bends] -> Hook
      PointsToRender.Add(GetOwner()->GetActorLocation());

      // Add any wrapped points
      if (BendPoints.Num() > 0) {
        PointsToRender.Append(BendPoints);
      }

      PointsToRender.Add(CurrentHook->GetActorLocation());

      bShouldRender = true;
      bIsDeploying = true; // Enable Dynamic RestLength
    }
  } else if (RopeState == ERopeState::Attached) {
    if (BendPoints.Num() >= 2) {
      // BendPoints already contains [Anchor, ... , Player]
      PointsToRender = BendPoints;
      bShouldRender = true;
      bIsDeploying = false;
    }
  }

  // 2. Execute Update on Render Component
  if (bShouldRender) {
    bool bStateChanged = (RopeState != LastRopeState);
    bool bTopologyChanged = (PointsToRender.Num() != LastPointCount);
    bool bFirstRender = !RenderComponent->IsRopeActive();

    // Condition for Full Rebuild:
    // - First time rendering
    // - State transition (Mode changed)
    // - Topology changed (Point count changed)
    if (bFirstRender || bStateChanged || bTopologyChanged) {
      RenderComponent->UpdateRope(PointsToRender, bIsDeploying);
    } else {
      // POSITION UPDATE ONLY
      // RenderComponent->UpdatePinPositions(PointsToRender); // Legacy opt
      // But we want to ensure Linkage, so UpdatePinPositions is OK if we fixed
      // it in Render. However, previous attempt used UpdateRope. The signature
      // is UpdateRope(Points, bDeploying).
      RenderComponent->UpdateRope(PointsToRender, bIsDeploying);
    }

    LastPointCount = PointsToRender.Num();
  } else {
    // Go Idle
    if (RenderComponent->IsRopeActive()) {
      RenderComponent->HideRope();
    }
    LastPointCount = 0;
  }

  LastRopeState = RopeState;

  // Debug rendering
  if (bShowDebug && PointsToRender.Num() > 0) {
    for (int32 i = 0; i < PointsToRender.Num() - 1; ++i) {
      DrawDebugLine(GetWorld(), PointsToRender[i], PointsToRender[i + 1],
                    FColor::Green, false, -1.f, 0, 3.f);
    }
  }
}

// End of file
