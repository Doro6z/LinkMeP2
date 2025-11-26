# Système de Corde LinkMe

## Vue d'ensemble

Le système de corde est implémenté dans le module `LinkRope` et fournit une simulation physique complète de corde avec rendu, utilisable via Blueprint ou C++.

## Architecture

### Composants principaux

1. **AChainActor** : Actor principal qui orchestre la corde
   - Gère les endpoints (start/end)
   - Expose les paramètres de simulation
   - Délègue la simulation et le rendu aux composants

2. **UChainSimulationComponent** : Simulation physique Verlet
   - Intégration Verlet avec sous-pas
   - Contraintes de distance (rigidité/élasticité)
   - Collisions optionnelles
   - **Important** : Start et End sont toujours ancrés (pas de LooseEnd)

3. **UChainRenderComponent** : Rendu visuel
   - Utilise InstancedStaticMeshComponent
   - Un mesh par segment de corde
   - Épaisseur configurable

4. **UChainOwnerComponent** : Composant gameplay
   - Gère la création/destruction de la corde
   - Machine à états (Idle, Aiming, HookInFlight, Anchored, ReelingIn/Out)
   - API Blueprint pour throw/reel/cut
5. **UHookChainComponent** : config du hook (vitesse, tag hookable, poids)
6. **AHookProjectileActor** : projectile lancé, diffuse un événement `OnHookImpact`

### Types de base (RopeTypes.h)

- **FChainPoint** : Point simulé avec position, dernière position, masse inverse
- **FChainParams** : Paramètres de simulation (segments, gravité, damping, élasticité, collisions)
- **FChainEndPoint** : Point d'ancrage avec support d'Actor et Socket

## Utilisation dans Blueprint

### Intégration dans le personnage

1. Dans `BP_ThirdPersonCharacter`, ajouter un composant `ChainOwnerComponent`
2. Configurer les paramètres :
   - `StartSocketName` : Socket où attacher le début (ex: "pelvis")
   - `MaxRopeLength` : Longueur maximale
   - `ReelSpeed` : Vitesse de rembobinage
   - `ThrowDistance` : Distance de lancer pour le raycast

3. Binder les Input Actions (Enhanced Input) :
   - `IA_RopeThrow` (Pressed) → `StartAim`
   - `IA_RopeThrow` (Released) → `ThrowChain`
   - `IA_RopeReel` (Hold) → `ReelIn(DeltaSeconds)`
   - `IA_RopeReelOut` (Hold) → `ReelOut(DeltaSeconds)`
   - `IA_RopeCut` → `CutChain`

### Exemple de binding dans Blueprint

```
Event BeginPlay
  -> Get ChainOwnerComponent
  -> Bind Enhanced Input Actions
    -> IA_RopeThrow (Pressed) -> StartAim
    -> IA_RopeThrow (Released) -> ThrowChain
    -> IA_RopeReel (Hold) -> Set State to Retracting
    -> IA_RopeCut -> CutChain
```

## Paramètres ajustables

### FChainParams (dans ChainOwnerComponent ou ChainActor)

**Simulation :**
- `SegmentCount` (1-200) : Nombre de segments (plus = plus fluide mais plus coûteux)
- `Substeps` (1-30) : Sous-pas Verlet (plus = plus stable)
- `ConstraintIterations` (1-20) : Itérations du solveur (plus = plus rigide)
- `GravityScale` (0-2) : Échelle de gravité
- `Damping` (0-1) : Amortissement (0 = pas d'amortissement)
- `Elasticity` (0-1) : Élasticité (0 = rigide, 1 = très extensible)
- `MaxStretchRatio` (0-3) : Ratio d'étirement max (ex: 0.5 = +50%)

**Collision :**
- `bEnableCollision` : Activer les collisions
- `CollisionRadius` : Rayon de collision
- `CollisionChannel` : Canal de collision

**Render :**
- `RopeThickness` : Épaisseur visuelle de la corde

### Paramètres gameplay (ChainOwnerComponent)

- `MaxRopeLength` : Longueur maximale
- `ReelSpeed` / `ReelOutSpeed` : vitesses de rembobinage/déroulement
- `StartSocketName` : Socket sur le personnage
- `HookProjectileClass` : classe à instancier pour le hook (par défaut `AHookProjectileActor`)
- `AimPreviewLength` + `bShowAimDebug` : preview visuelle pendant la visée

### Paramètres hook (UHookChainComponent)

- `LaunchSpeed`
- `HookableTag`
- `bAttachToAnySurface`
- `bSimulatePhysicsOnImpact`
- `MaxFlightTime` : timeout auto reset

## Hook Prototype

1. `StartAim()` affiche une flèche (`UArrowComponent`) et un ghost de trajectoire (spline + `DrawDebugLine`).
2. `ThrowChain()` spawn `AHookProjectileActor`, lui passe la direction et attend l’événement `OnHookImpact`.
3. À l’impact :
   - Si l’acteur possède le tag `Hookable`, le hook s’y attache (AttachToComponent).
   - Sinon, il reste planté à la `HitLocation` (optionnellement en physique).
4. `UChainOwnerComponent` instancie un `AChainActor` qui relie le pelvis au hook (l’endpoint est attaché au projectile).
5. `ReelIn/ReelOut` déplacent le hook le long de la corde (simple translation pour le prototype). `ReleaseChain` et `CutChain` détruisent hook + corde.

### Test rapide

1. Créer `BP_HookProjectile` (hérite de `AHookProjectileActor`) : ajuster mesh, vitesse dans `HookComponent`.
2. Ajouter quelques cubes avec le tag `Hookable`.
3. Inputs : `IA_RopeThrow` (press/release), `IA_RopeReel`, `IA_RopeReelOut`, `IA_RopeCut`.
4. Play In Editor : viser → la flèche montre la direction, tirer → le projectile part, s’ancre, la corde se rend automatiquement, `ReelIn` / `ReelOut` déplacent le hook.

## Points d'extension futurs

### 1. Enroulement autour d'objets
- Ajouter un système de détection de collision avec obstacles
- Créer des points d'ancrage dynamiques autour des obstacles
- Modifier `SolveConstraints` pour gérer les ancres multiples

### 2. Items (ciseaux, +/- longueur)
- Créer des DataAssets pour les items
- Modifier `RopeParams` runtime via `SetParams` dans `ChainOwnerComponent`
- Pour couper : `CutChain()` existe déjà
- Pour modifier longueur : ajuster `RestLength` dans la simulation

### 3. Multiplayer sync
- Ajouter réplication aux propriétés critiques dans `AChainActor`
- Repliquer `Start` et `End` endpoints
- Optionnel : repliquer les positions intermédiaires (coûteux)

### 4. Tension pour tractions/grappin
- Calculer la tension dans `GetStretchRatio()` ou nouvelle fonction
- Appliquer des forces au personnage via `AddForce` dans le Character
- Détecter quand la tension dépasse un seuil pour déclencher des événements

## Debug et test

### Blueprint de test

Créer `BP_TestChainActor` :
1. Hérite de `AChainActor`
2. Dans l'éditeur, définir `Start.Location` et `End.Location`
3. Configurer `Params` pour tester différents réglages
4. Placer dans une map de test

### Visualisation debug

Dans `ChainSimulationComponent.cpp`, décommenter les `DrawDebugHelpers` si besoin :
```cpp
DrawDebugSphere(World, P, CollisionRadius, 8, FColor::Red, false, 0.1f);
```

## Modifications courantes

### Changer la longueur de la corde
- Modifier `MaxRopeLength` dans `ChainOwnerComponent`
- Ou créer une fonction qui ajuste `RestLength` dans la simulation

### Ajuster la rigidité
- Modifier `Elasticity` (0 = rigide) et `ConstraintIterations` (plus = plus rigide)

### Changer la vitesse de rembobinage
- Modifier `ReelSpeed` dans `ChainOwnerComponent`

### Utiliser un mesh personnalisé
- Assigner `SegmentMesh` dans `ChainRenderComponent` (ou via `ChainActor`)

## Notes techniques

- Le système utilise Verlet integration pour la stabilité
- Les collisions sont optionnelles et peuvent être coûteuses avec beaucoup de segments
- Le rendu utilise ISMC pour la performance (un draw call par corde)
- Start et End sont toujours ancrés (pas de point libre) pour éviter les instabilités

