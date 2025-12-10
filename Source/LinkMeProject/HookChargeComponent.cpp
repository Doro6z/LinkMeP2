#include "HookChargeComponent.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/Actor.h"
#include "DrawDebugHelpers.h"

UHookChargeComponent::UHookChargeComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UHookChargeComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (ChargeState != EChargeState::Charging) return;

	// Incrémenter la charge
	float PreviousCharge = CurrentCharge;
	CurrentCharge = FMath::Clamp(CurrentCharge + (ChargeRate * DeltaTime), 0.0f, MaxCharge);

	// Calculer vitesse actuelle basique
	CurrentLaunchSpeed = ChargeToSpeed(CurrentCharge);

	// MODE FOCUS : Recalcul de la vitesse requise
	if (bIsFocusMode)
	{
		TimeSinceLastRecalc += DeltaTime;

		// Recalculer si nécessaire (intervalle écoulé ou cible bougée)
		bool bTargetMoved = FVector::Dist(TargetLocation, CachedTargetLocation) > 10.0f;

		if (bRequiresRecalc || TimeSinceLastRecalc >= RecalcInterval || bTargetMoved)
		{
			RequiredSpeed = CalculateRequiredSpeed(StartPosition, TargetLocation);
			RequiredCharge = SpeedToCharge(RequiredSpeed);

			CachedTargetLocation = TargetLocation;
			TimeSinceLastRecalc = 0.0f;
			bRequiresRecalc = false;
			
			if (bShowDebug)
			{
				UE_LOG(LogTemp, Warning, TEXT("[HookCharge] Recalculated NeedSpeed: %f (Charge: %f)"), RequiredSpeed, RequiredCharge);
			}
		}

		// Vérifier si charge parfaite
		bool bWasPerfect = bChargePerfect;
		float ChargeEpsilon = FMath::Max(0.02f, RequiredCharge * PerfectChargeEpsilon);
		
		// Perfect si reachable ET dans la range
		bChargePerfect = bTargetReachable &&
			(CurrentCharge >= (RequiredCharge - ChargeEpsilon)) &&
			(CurrentCharge <= (RequiredCharge + ChargeEpsilon));

		// Event si passage à "parfait"
		if (bChargePerfect && !bWasPerfect)
		{
			OnChargePerfect.Broadcast();
		}
	}
	
	// Visual Debug of Current Charge State
	if (bShowDebug)
	{
		FString DebugMsg = FString::Printf(TEXT("Charge: %.2f | Speed: %.0f | Focus: %s | Reachable: %s"), 
			CurrentCharge, CurrentLaunchSpeed, bIsFocusMode ? TEXT("ON") : TEXT("OFF"), bTargetReachable ? TEXT("YES") : TEXT("NO"));
		GEngine->AddOnScreenDebugMessage(110, 0.0f, FColor::Yellow, DebugMsg);
		
		if (bIsFocusMode)
		{
			DrawDebugSphere(GetWorld(), TargetLocation, 30.f, 8, bTargetReachable ? FColor::Green : FColor::Red, false, -1.0f);
		}
	}

	// Broadcast update pour UI/animations
	if (FMath::Abs(CurrentCharge - PreviousCharge) > SMALL_NUMBER)
	{
		OnChargeUpdated.Broadcast(GetChargeRatio());
	}
}

void UHookChargeComponent::StartCharging(bool bInFocusMode, const FVector& InTargetLocation, const FVector& InStartLocation)
{
	if (ChargeState == EChargeState::Charging)
	{
		// Already charging: Update context but DO NOT reset charge
		// This supports "Triggered" input events that fire every frame
		bIsFocusMode = bInFocusMode;
		TargetLocation = InTargetLocation;
		StartPosition = InStartLocation;
		bRequiresRecalc = true;
		
		if (bShowDebug)
		{
			// Less spammy log for continuous update?
			// UE_LOG(LogTemp, Verbose, TEXT("[HookCharge] Updating Charge Target..."));
		}
		return;
	}

	ChargeState = EChargeState::Charging;
	CurrentCharge = 0.0f;
	CurrentLaunchSpeed = MinLaunchSpeed;
	
	bIsFocusMode = bInFocusMode;
	TargetLocation = InTargetLocation;
	StartPosition = InStartLocation;
	
	if (bShowDebug)
	{
		UE_LOG(LogTemp, Warning, TEXT("[HookCharge] StartCharging (FRESH). Focus: %d, Target: %s, Start: %s"), 
			bInFocusMode, *InTargetLocation.ToString(), *InStartLocation.ToString());
		DrawDebugSphere(GetWorld(), StartPosition, 20.f, 8, FColor::Cyan, false, 2.0f);
	}
	
	// Force recalc immediately if focus mode
	bRequiresRecalc = true;
	bChargePerfect = false;
	TimeSinceLastRecalc = 0.0f;

	OnChargeStarted.Broadcast();
}

bool UHookChargeComponent::StopChargingAndGetVelocity(FVector& OutVelocity)
{
	UE_LOG(LogTemp, Warning, TEXT("UHookChargeComponent::StopChargingAndGetVelocity called. State: %d, Charge: %f"), (int32)ChargeState, CurrentCharge);

	if (ChargeState == EChargeState::Idle)
	{
		OutVelocity = FVector::ZeroVector;
		UE_LOG(LogTemp, Warning, TEXT("StopCharging: State was Idle, returning false"));
		return false;
	}
	
	bool bValidLaunch = (CurrentCharge >= MinChargeThreshold);
	if (!bValidLaunch)
	{
		UE_LOG(LogTemp, Warning, TEXT("StopCharging: Charge %f below threshold %f"), CurrentCharge, MinChargeThreshold);
	}
	
	// Calcul du vecteur final
	float SpeedToUse = CurrentLaunchSpeed;
	UE_LOG(LogTemp, Warning, TEXT("StopCharging: SpeedToUse: %f"), SpeedToUse);
	
	// En mode Focus, si on est "Enough Charged" (ou perfect), on utilise la RequiredSpeed pour être précis
	// Cependant, pour le gameplay, si le joueur charge TROP, il doit rater (overshoot).
	// Donc on utilise toujours le CurrentLaunchSpeed basé sur la charge.
	// C'est au joueur de relâcher au bon moment (quand bChargePerfect est true).
	
	FVector Dir = (TargetLocation - StartPosition).GetSafeNormal();
	
	// Si Focus Mode et Reachable, SuggestProjectileVelocity nous donne le vecteur EXACT
	if (bIsFocusMode && bTargetReachable)
	{
		UE_LOG(LogTemp, Warning, TEXT("StopCharging: Focus Mode Active. TargetReachable. Recalculating Arc."));
		// On recalculer l'arc pour la vitesse actuelle
		PRAGMA_DISABLE_DEPRECATION_WARNINGS
		UGameplayStatics::SuggestProjectileVelocity(
			GetWorld(),
			OutVelocity,
			StartPosition,
			TargetLocation,
			CurrentLaunchSpeed,
			false,
			0.f,
			0.f,
			ESuggestProjVelocityTraceOption::DoNotTrace
		);
		PRAGMA_ENABLE_DEPRECATION_WARNINGS
	}
	else
	{
		// Mode normal : l'orientation vient du controller/camera (passé par StartPosition? Non, StartPosition est latérale)
		// ATTENTION: StartCharging reçoit InStartLocation (Hand Pos).
		// Mais on a besoin de la direction de visée ici.
		// En fait, OutVelocity doit être calculé par l'appelant (CharacterRope) qui connait la AimDirection
		// Ce composant ne gère que le Speed scalaire, sauf en focus mode où il connait la target.
	}
	
	ChargeState = EChargeState::Fired;
	OnChargeFired.Broadcast(OutVelocity, CurrentLaunchSpeed);
	
	// Reset après délai ou immédiat
	// ChargeState = EChargeState::Idle; // Laisser l'appelant gérer le reset ou appel CancelCharging
	
	return bValidLaunch;
}

void UHookChargeComponent::CancelCharging()
{
	ChargeState = EChargeState::Idle;
	CurrentCharge = 0.0f;
}

float UHookChargeComponent::GetChargeRatio() const
{
	return CurrentCharge / MaxCharge;
}

// ===================================================================
// ALGORITHMS
// ===================================================================

float UHookChargeComponent::CalculateRequiredSpeed(const FVector& Start, const FVector& Target)
{
	if (!GetWorld()) return MinLaunchSpeed;

	float Low = MinLaunchSpeed;
	float High = MaxLaunchSpeed;
	float BestSpeed = MaxLaunchSpeed;
	bool bFoundSolution = false;

	// Direction initiale approximative
	FVector Direction = (Target - Start).GetSafeNormal();

	for (int32 Iteration = 0; Iteration < BinarySearchIterations; ++Iteration)
	{
		float MidSpeed = (Low + High) * 0.5f;

		// Tester si cette vitesse permet d'atteindre la cible
		bool bCanHit = TestProjectileHit(Start, Direction, MidSpeed, Target, HitTolerance);

		if (bCanHit)
		{
			// Succès : cette vitesse fonctionne, essayer plus bas (pour trouver la vitesse minimale requise)
			BestSpeed = MidSpeed;
			High = MidSpeed;
			bFoundSolution = true;
		}
		else
		{
			// Échec : vitesse trop faible, essayer plus haut
			Low = MidSpeed;
		}
	}

	bTargetReachable = bFoundSolution;

	if (!bFoundSolution)
	{
		OnTargetUnreachable.Broadcast();
		return MaxLaunchSpeed; // Retourne Max si inaccessible
	}

	return BestSpeed;
}

bool UHookChargeComponent::TestProjectileHit(const FVector& Start, const FVector& InitialDirection, float Speed, const FVector& Target, float Tolerance)
{
	// 1. Tenter SuggestProjectileVelocity (Analytique, rapide)
	FVector OutVelocity;
	
	
	// Utilisation de la signature classique avec suppression du warning deprecated
	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	bool bSuccess = UGameplayStatics::SuggestProjectileVelocity(
		GetWorld(),
		OutVelocity,
		Start,
		Target,
		Speed,
		false,
		0.0f,
		0.0f,
		ESuggestProjVelocityTraceOption::DoNotTrace
	);
	PRAGMA_ENABLE_DEPRECATION_WARNINGS

	if (bSuccess)
	{
		return true;
	}

	// 2. Fallback: Simulation physique (plus lent mais gère collisions)
	return false;
}

bool UHookChargeComponent::SimulateAndCheckHit(const FVector& Start, const FVector& LaunchVelocity, const FVector& Target, float Tolerance)
{
	FPredictProjectilePathParams PredictParams;
	PredictParams.StartLocation = Start;
	PredictParams.LaunchVelocity = LaunchVelocity;
	PredictParams.bTraceWithCollision = true;
	PredictParams.bTraceComplex = false;
	PredictParams.ProjectileRadius = 5.0f;
	PredictParams.MaxSimTime = 2.0f;
	PredictParams.SimFrequency = 15.0f;
	PredictParams.TraceChannel = ProjectileTraceChannel;
	PredictParams.ActorsToIgnore.Add(GetOwner());

	FPredictProjectilePathResult PredictResult;
	bool bHit = UGameplayStatics::PredictProjectilePath(GetWorld(), PredictParams, PredictResult);

	if (bHit && PredictResult.HitResult.bBlockingHit)
	{
		float DistanceToTarget = FVector::Dist(PredictResult.HitResult.ImpactPoint, Target);
		return DistanceToTarget <= Tolerance;
	}

	// Pas de hit direct, vérifier la trajectoire
	for (const FPredictProjectilePathPointData& Point : PredictResult.PathData)
	{
		if (FVector::Dist(Point.Location, Target) <= Tolerance)
		{
			return true;
		}
	}

	return false;
}

float UHookChargeComponent::ChargeToSpeed(float InCharge) const
{
	float Alpha = InCharge; // Pourrait passer par ChargeCurve->GetFloatValue(InCharge)
	if (ChargeCurve)
	{
		Alpha = ChargeCurve->GetFloatValue(InCharge);
	}
	return FMath::Lerp(MinLaunchSpeed, MaxLaunchSpeed, Alpha);
}

float UHookChargeComponent::SpeedToCharge(float InSpeed) const
{
	float Alpha = FMath::GetMappedRangeValueClamped(FVector2D(MinLaunchSpeed, MaxLaunchSpeed), FVector2D(0.f, 1.f), InSpeed);
	// Inverser la courbe si nécessaire, mais approximation linéaire suffit pour RequiredCharge
	return Alpha;
}
