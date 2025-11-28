# LinkMe – Système de corde hybride (Document technique)

Ce document décrit une architecture de corde/grappin adaptée au GDD « LinkMe ». L'objectif est d'obtenir un gameplay lisible et maîtrisé en multijoueur, tout en conservant un rendu physique crédible. La logique est séparée du rendu : une corde algorithmique (squelette) pilotant un visuel physique cosmétique (peau).

## 1. Architecture proposée

| Élément | Rôle |
| --- | --- |
| **`AC_RopeSystem` (UActorComponent)** | Cerveau gameplay : états, liste de points d'inflexion (bend points), gestion de longueur, application des forces au joueur. |
| **`ARopeHookActor` (Actor projectile)** | Grappin physique lançable (collision via `USphereComponent`, `SimulatePhysics`). Expose `Fire(const FVector& Dir)` et notifie `OnHookImpact`. |
| **`URopeRenderComponent` (UActorComponent)** | Rendu procédural client : points simulés en Verlet -> segments mesh instanciés. Aucune spline UE, aucun `UCableComponent`. |

### 1.1 États de corde

Enum `ERopeState` dans `AC_RopeSystem` :
- **Idle** : aucune corde active.
- **Flying** : hook en vol, en attente d'impact.
- **Attached** : hook accroché, corde tendue/enroulée.

### 1.2 Données clés (`AC_RopeSystem`)
- `TArray<FVector> BendPoints` : du point d'ancrage (index 0) jusqu'au joueur (dernier index).
- `float CurrentLength` / `float MaxLength` : longueur disponible / maximale (bobine).
- `ARopeHookActor* CurrentHook` : hook actif.
- `ERopeState RopeState` : état courant.

## 2. Boucle principale (pseudo-code)

```cpp
void AC_RopeSystem::TickComponent(float DeltaTime, ...)
{
    if (RopeState == ERopeState::Idle) return;

    if (RopeState == ERopeState::Flying)
    {
        if (CurrentHook && CurrentHook->HasImpacted())
        {
            TransitionToAttached();
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
```

## 3. Algorithmes clés

### 3.1 Gestion des bend points (enroulement/déroulement)

```cpp
void AC_RopeSystem::ManageBendPoints()
{
    const FVector PlayerPos = GetOwner()->GetActorLocation();
    const FVector LastPoint = BendPoints.Last();

    // A. Ajouter un point si un obstacle bloque la ligne joueur ↔ dernier point
    FHitResult Hit;
    if (LineTrace(LastPoint, PlayerPos, Hit))
    {
        const FVector NewPoint = Hit.ImpactPoint + Hit.Normal * BendOffset; // léger décalage
        BendPoints.Add(NewPoint);
    }

    // B. Retirer la dernière pliure si la ligne vers l'avant-dernier point est libre
    if (BendPoints.Num() > 1)
    {
        const FVector PreLast = BendPoints[BendPoints.Num() - 2];
        if (!LineTrace(PreLast, PlayerPos, Hit))
        {
            BendPoints.RemoveAt(BendPoints.Num() - 1);
        }
    }
}
```

### 3.2 Bobine et longueur exploitable

```cpp
void AC_RopeSystem::ManageRopeLength(float DeltaTime)
{
    float RopeLength = 0.f;
    for (int i = 0; i < BendPoints.Num() - 1; ++i)
    {
        RopeLength += (BendPoints[i + 1] - BendPoints[i]).Size();
    }

    if (RopeLength > CurrentLength)
    {
        const float Excess = RopeLength - CurrentLength;
        const FVector Dir = (BendPoints[0] - GetOwner()->GetActorLocation()).GetSafeNormal();
        const FVector Force = Dir * (Excess * SpringStiffness);
        ApplyForceToPlayer(Force);
    }
}
```

`CurrentLength` est modifié par le rembobinage/déraillage. La comparaison avec la longueur géométrique contrôle la tension.

### 3.3 Application des forces (traction et swing)

```cpp
void AC_RopeSystem::ApplyForcesToPlayer()
{
    ACharacter* OwnerChar = Cast<ACharacter>(GetOwner());
    if (!OwnerChar) return;

    const FVector Anchor = BendPoints[0];
    const FVector PlayerPos = OwnerChar->GetActorLocation();
    const FVector Dir = (Anchor - PlayerPos).GetSafeNormal();
    const float Distance = (Anchor - PlayerPos).Size();

    const float Stretch = Distance - CurrentLength;
    if (Stretch > 0.f)
    {
        FVector Velocity = OwnerChar->GetVelocity();
        const FVector TangentVel = Velocity - FVector::DotProduct(Velocity, Dir) * Dir;
        OwnerChar->GetCharacterMovement()->Velocity = TangentVel; // contrainte de corde

        const FVector Pull = Dir * (Stretch * SpringStiffness);
        OwnerChar->GetCharacterMovement()->AddForce(Pull);

        const FVector Right = FVector::CrossProduct(Dir, FVector::UpVector);
        OwnerChar->GetCharacterMovement()->AddForce(Right * SwingTorque); // swing cartoonesque
    }
}
```

### 3.4 Interactions : trébuchement et coupe

- **Trip/Trap** : pour chaque segment, tester la distance d'un autre joueur au segment. Si < rayon de capsule et corde tendue → passer le joueur en ragdoll et lui appliquer une impulsion proportionnelle à la tension.
- **Coupe** : un `LineTrace`/`SphereTrace` qui touche la maille de corde déclenche `Sever()` dans `AC_RopeSystem` (destruction du hook + reset des bend points). Un effet VFX signale la rupture.

## 4. Rendu cosmétique (procédural, sans spline/cable)

- `URopeRenderComponent` lit `BendPoints` chaque tick côté client et génère des **points Verlet** intermédiaires (4–12 itérations selon Slack/Damping/GravityScale).
- Chaque paire de points Verlet est convertie en **segment de mesh instancié** (cylindre low poly). Il n'y a **aucun recours** aux splines UE ni à `UCableComponent`.
- Le rendu colore la corde selon la tension (bleu → rouge) et peut déclencher un VFX de rupture lorsque le serveur signale une coupe.

## 5. Réseau

- Réplication serveur → clients : `RopeState`, `BendPoints`, `CurrentLength`, identifiant du hook.
- Le serveur reste autorité pour `LineTrace` et forces. Les clients recalculent uniquement le visuel.
- Les événements de coupe/nœud défait sont envoyés via RPC pour resynchroniser le squelette.

## 6. Data Assets et paramétrage

Créer un `URopeDataAsset` exposant :
- `MaxLength`, `MinLength`, `ReelSpeed`, `ReelOutSpeed`.
- `SpringStiffness`, `BendOffset`.
- `CosmeticSegments`, `SolverIterations`, `GravityScale`, `Damping` (pour le solver client).
- `UnknotTime` pour les mécaniques de nœuds.

## 7. Advanced Mechanics Analysis (User Request 2025-11-27)

### 7.1. The "Fun & Constraining" Challenge
The goal is a rope that feels physical (trails, wraps) but works in MP (18 players).
**Problem**: Full physics engines (Chaos/PhysX) are non-deterministic and expensive to replicate. Syncing 18 physics ropes over network is a nightmare (bandwidth + jitter).

### 7.2. The Solution: "Taut-Wrap / Slack-Sag" Hybrid
We separate the rope into two states managed by our custom system:

#### A. The Logical Rope (Server - Authority)
*   **Representation**: `TArray<FVector> BendPoints`.
*   **Behavior**:
    *   **Slack**: The server considers the rope a straight line (or simple curve) but applies **NO forces**. It does **NOT** generate bend points on geometry (too complex to wrap loose rope).
    *   **Taut**: When `CurrentLength <= Distance`, the rope becomes a "Guitar String". It applies forces and **generates Bend Points** (wrapping) via Line Traces.
*   **Grabbing**: To grab another player's rope, we don't use collision. We use **Math**.
    *   When Player B tries to grab, we iterate Player A's `BendPoints`.
    *   We calculate `FMath::PointDistToSegment` for each segment.
    *   If distance < Threshold, we register a "Grab".

#### B. The Visual Rope (Client - Cosmetic)
*   **Representation**: `URopeRenderComponent` (Verlet Integration).
*   **Behavior**:
    *   **Slack**: Gravity pulls the particles down. It looks like it's trailing/sagging.
    *   **Taut**: Tension pulls particles straight.
    *   **Result**: The player *sees* a trailing rope, but the server only cares about the tension points. This is the "Smoke & Mirrors" used in games like *Uncharted* or *The Last of Us*.

### 7.3. Modular Attachment (Back vs Hand)
*   **Sockets**: The `RopeSystem` should not assume `ActorLocation`.
*   **Implementation**:
    *   Add `FName AttachSocketName` to `URopeSystemComponent`.
    *   Default to "SpineSocket" (Back pulley).
    *   When player presses "Grab Rope" input -> Interp `AttachSocketName` to "HandSocket".
    *   The `GetRopeOrigin()` function uses this socket.

### 7.4. Multiplayer Scalability
*   **Cost**:
    *   Server: Raycasts only when moving fast/wrapping. Very cheap.
    *   Client: Verlet sim is O(N) points. 18 ropes * 50 points = 900 points. Trivial for modern CPU/GPU.
    *   Bandwidth: We only replicate `BendPoints` (Array of vectors). Efficient.

### 7.5. Implementation Roadmap
1.  **Refine Wrapping**: Ensure `ManageBendPoints` is robust (Done/In Progress).
2.  **Dynamic Origin**: Replace `GetActorLocation()` with `GetSocketLocation(SocketName)`.
3.  **Interaction**: Create `TryGrabRope()` function that checks all `URopeSystemComponent` in world.

## 8. Extensions/Gameplay (alignés avec le GDD)

- **Super nœud** : plusieurs cordes qui se croisent fusionnent en « nœud gordien » nécessitant plusieurs joueurs pour être défait. Augmente tension et devient un point de contrôle du round.
- **Bobine visible** : mesh de roue sur le dos qui grossit quand la corde se rembobine (feedback lisible en stream). Peut servir de jauge de longueur.
- **Momentum Attack** : charge de rotation qui réduit temporairement `CurrentLength` puis libère une impulsion (catapulte d'allié ou hook boosté).
- **Power-ups** : items ciseaux (coupe globale), « +/- longueur » (ajuste `MaxLength`/`CurrentLength`), buff d'élasticité pour des swings plus longs.

## 9. Pourquoi cette approche ?

- Évite les chaînes 100 % physiques (coûteuses et difficiles à répliquer) tout en conservant un rendu crédible.
- Sépare nettement gameplay déterministe (squelette) et jus visuel (peau), idéal pour le multijoueur et pour créer des moments « TikTokables » sans instabilités réseau.
