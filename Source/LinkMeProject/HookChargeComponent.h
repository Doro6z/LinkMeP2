#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "HookChargeComponent.generated.h"

UENUM(BlueprintType)
enum class EChargeState : uint8
{
	Idle            UMETA(DisplayName = "Idle"),
	Charging        UMETA(DisplayName = "Charging"),
	ReadyToFire     UMETA(DisplayName = "Ready To Fire"),  // Focus mode: charge parfaite
	Fired           UMETA(DisplayName = "Fired")
};

// Events
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnChargeStarted);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnChargeUpdated, float, ChargeRatio);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnChargePerfect);       // Charge optimale atteinte
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnTargetUnreachable);   // Cible hors portée
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnChargeFired, FVector, LaunchVelocity, float, FinalSpeed);

UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class LINKMEPROJECT_API UHookChargeComponent : public UActorComponent
{
	GENERATED_BODY()

public:	
	UHookChargeComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	// ===================================================================
	// PUBLIC API
	// ===================================================================

	/**
	 * Démarrer la charge du lancer
	 * @param bInFocusMode - Si true, calcule la force optimale pour toucher TargetLocation
	 * @param InTargetLocation - Position cible (utilisé uniquement si bInFocusMode = true)
	 * @param StartLocation - Position de départ du projectile (main du joueur)
	 */
	UFUNCTION(BlueprintCallable, Category = "Hook Charge")
	void StartCharging(bool bInFocusMode, const FVector& InTargetLocation, const FVector& StartLocation);

	/**
	 * Arrêter la charge et récupérer la vélocité finale
	 * @param OutVelocity - Vecteur vélocité à appliquer au projectile
	 * @return True si la charge était valide (>= MinChargeThreshold)
	 */
	UFUNCTION(BlueprintCallable, Category = "Hook Charge")
	bool StopChargingAndGetVelocity(FVector& OutVelocity);

	/**
	 * Annuler la charge en cours (reset à Idle)
	 */
	UFUNCTION(BlueprintCallable, Category = "Hook Charge")
	void CancelCharging();

	/** Obtenir le ratio de charge actuel (0-1) pour UI/animations */
	UFUNCTION(BlueprintPure, Category = "Hook Charge")
	float GetChargeRatio() const;

	/** Returns true if actively charging */
	UFUNCTION(BlueprintPure, Category = "Hook Charge")
	bool IsCharging() const { return ChargeState == EChargeState::Charging; }

	/** Returns true if ready to fire (charged enough or perfect) */
	UFUNCTION(BlueprintPure, Category = "Hook Charge")
	bool IsReadyToFire() const { return ChargeState == EChargeState::ReadyToFire || (ChargeState == EChargeState::Charging && CurrentCharge >= MinChargeThreshold); }

	/** Obtenir la vitesse de lancer actuelle */
	UFUNCTION(BlueprintPure, Category = "Hook Charge")
	float GetCurrentLaunchSpeed() const { return CurrentLaunchSpeed; }

	/** Charge actuelle >= Charge requise - epsilon ? (Focus mode) */
	UFUNCTION(BlueprintPure, Category = "Hook Charge")
	bool IsChargePerfect() const { return bChargePerfect; }

	/** Cible atteignable ? */
	UFUNCTION(BlueprintPure, Category = "Hook Charge")
	bool IsTargetReachable() const { return bTargetReachable; }

	/** Obtenir la charge requise (0-1, Focus mode) */
	UFUNCTION(BlueprintPure, Category = "Hook Charge")
	float GetRequiredCharge() const { return RequiredCharge; }

	// ===================================================================
	// CONFIGURATION
	// ===================================================================

	/** Vitesse minimale de lancer (charge = 0) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hook Charge|Speed")
	float MinLaunchSpeed = 800.0f;

	/** Vitesse maximale de lancer (charge = 1) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hook Charge|Speed")
	float MaxLaunchSpeed = 3500.0f;

	/** Vitesse d'accumulation de la charge (0->1 par seconde) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hook Charge|Speed")
	float ChargeRate = 0.6f;

	/** Charge minimale pour considérer le lancer valide */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hook Charge|Thresholds")
	float MinChargeThreshold = 0.05f;

	/** Charge maximale (normalisée 0-1) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hook Charge|Thresholds")
	float MaxCharge = 1.0f;

	/** Courbe d'easing pour la charge (optionnel) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hook Charge|Feel")
	UCurveFloat* ChargeCurve = nullptr;

	/** Tolérance pour considérer la cible touchée (en cm) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hook Charge|Focus")
	float HitTolerance = 50.0f;

	/** Nombre d'itérations de recherche binaire */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hook Charge|Focus")
	int32 BinarySearchIterations = 10;

	/** Intervalle de recalcul de la vitesse requise (secondes) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hook Charge|Focus")
	float RecalcInterval = 0.1f;

	/** Epsilon pour considérer la charge "parfaite" (% de RequiredCharge) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hook Charge|Focus")
	float PerfectChargeEpsilon = 0.03f;

	/** Canal de collision pour la prédiction de trajectoire */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hook Charge|Focus")
	TEnumAsByte<ECollisionChannel> ProjectileTraceChannel = ECC_WorldStatic;

	/** If true, enables Verbose Logs and Visual Arc Debugging */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug")
	bool bShowDebug = false;

	// ===================================================================
	// EVENTS
	// ===================================================================

	UPROPERTY(BlueprintAssignable, Category = "Hook Charge|Events")
	FOnChargeStarted OnChargeStarted;

	UPROPERTY(BlueprintAssignable, Category = "Hook Charge|Events")
	FOnChargeUpdated OnChargeUpdated;

	UPROPERTY(BlueprintAssignable, Category = "Hook Charge|Events")
	FOnChargePerfect OnChargePerfect;

	UPROPERTY(BlueprintAssignable, Category = "Hook Charge|Events")
	FOnTargetUnreachable OnTargetUnreachable;

	UPROPERTY(BlueprintAssignable, Category = "Hook Charge|Events")
	FOnChargeFired OnChargeFired;

protected:
	// Helpers
	float CalculateRequiredSpeed(const FVector& Start, const FVector& Target);
	
	bool TestProjectileHit(
		const FVector& Start, 
		const FVector& InitialDirection,
		float Speed,
		const FVector& Target,
		float Tolerance);
		
	bool SimulateAndCheckHit(
		const FVector& Start,
		const FVector& LaunchVelocity,
		const FVector& Target,
		float Tolerance);

	float ChargeToSpeed(float InCharge) const;
	float SpeedToCharge(float InSpeed) const;

protected:
	// Runtime State
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hook Charge|State")
	EChargeState ChargeState = EChargeState::Idle;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hook Charge|State")
	float CurrentCharge = 0.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hook Charge|State")
	float CurrentLaunchSpeed = 0.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hook Charge|State")
	bool bIsFocusMode = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hook Charge|State")
	FVector TargetLocation = FVector::ZeroVector;
	
	FVector StartPosition = FVector::ZeroVector;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hook Charge|State")
	float RequiredSpeed = 0.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hook Charge|State")
	float RequiredCharge = 0.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hook Charge|State")
	bool bTargetReachable = true;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hook Charge|State")
	bool bChargePerfect = false;

	// Cache
	float TimeSinceLastRecalc = 0.0f;
	FVector CachedTargetLocation = FVector::ZeroVector;
	bool bRequiresRecalc = true;
};
