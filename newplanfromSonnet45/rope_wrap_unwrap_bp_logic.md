# Blueprint: Logique Wrap/Unwrap - RopeSystemComponent

## Configuration Exposée (Variables de Classe)

```
├─ WrapCooldown (float) = 0.1
├─ UnwrapCooldown (float) = 0.1
├─ MinBendDistance (float) = 50.0
├─ SubstepDivisions (int) = 5
├─ SphereRadius (float) = 15.0
├─ MaxBendPoints (int) = 30
└─ bEnableWrapUnwrap (bool) = true
```

---

## Event: OnRopeTickAttached

```
┌─────────────────────────────────────────────────────────────────┐
│ Event OnRopeTickAttached (Delta Time)                           │
└───────────────┬─────────────────────────────────────────────────┘
                │
                ├─ [Branch: bEnableWrapUnwrap?]
                │       │
                │       ├─ True ──┬──> Check For Wrap Logic
                │       │         └──> Check For Unwrap Logic
                │       │
                │       └─ False ──> Return
                │
                └─ Update Cooldown Timers (Subtract Delta Time)
```

---

## Function: CheckForWrap

### Inputs
- None (uses component state)

### Logic

```
┌─────────────────────────────────────────────────────────────────┐
│ 1. SAFETY CHECKS                                                │
└─────────────────────────────────────────────────────────────────┘
├─ Get Bend Point Count
│    └─ [Branch: Count < 2?] ──> Return (need anchor + player)
│
├─ [Branch: Count >= MaxBendPoints?] ──> Return (safety cap)
│
├─ [Branch: WrapCooldownTimer > 0?] ──> Return (still in cooldown)
│
└─ Continue...

┌─────────────────────────────────────────────────────────────────┐
│ 2. GET TRACE SEGMENT                                            │
└─────────────────────────────────────────────────────────────────┘
├─ Get Last Fixed Point ──> LastFixed
├─ Get Player Position ──> PlayerPos
│
└─ Continue...

┌─────────────────────────────────────────────────────────────────┐
│ 3. CAPSULE SWEEP                                                │
└─────────────────────────────────────────────────────────────────┘
├─ Capsule Sweep Between (LastFixed, PlayerPos, Hit Result)
│    │
│    ├─ [Branch: No Hit?] ──> Return (path is clear)
│    │
│    └─ Hit detected ──> Continue...
│
┌─────────────────────────────────────────────────────────────────┐
│ 4. COMPUTE BENDPOINT                                            │
└─────────────────────────────────────────────────────────────────┘
├─ Find Last Clear Point (Hit.ImpactPoint, PlayerPos, SubstepDivisions, SphereRadius)
│    └──> ClearPoint
│
├─ Compute Bend Point From Hit (Hit, 15.0)
│    └──> RefinedPoint
│
├─ [Select Better Point]
│    └─ Distance(ClearPoint, LastFixed) > Distance(RefinedPoint, LastFixed)?
│           ├─ True ──> Use ClearPoint
│           └─ False ──> Use RefinedPoint
│
└─ NewBendPoint

┌─────────────────────────────────────────────────────────────────┐
│ 5. VALIDATION                                                   │
└─────────────────────────────────────────────────────────────────┘
├─ Distance(NewBendPoint, LastFixed)
│    └─ [Branch: < MinBendDistance?] ──> Return (too close)
│
└─ Continue...

┌─────────────────────────────────────────────────────────────────┐
│ 6. ADD BENDPOINT                                                │
└─────────────────────────────────────────────────────────────────┘
├─ Add Bend Point (NewBendPoint)
│
├─ Set WrapCooldownTimer = WrapCooldown
└─ Set UnwrapCooldownTimer = UnwrapCooldown / 2.0
```

---

## Function: CheckForUnwrap

### Inputs
- None (uses component state)

### Logic

```
┌─────────────────────────────────────────────────────────────────┐
│ 1. SAFETY CHECKS                                                │
└─────────────────────────────────────────────────────────────────┘
├─ Get Bend Point Count
│    └─ [Branch: Count < 3?] ──> Return (need anchor + bend + player)
│
├─ [Branch: UnwrapCooldownTimer > 0?] ──> Return (still in cooldown)
│
└─ Continue...

┌─────────────────────────────────────────────────────────────────┐
│ 2. GET UNWRAP SEGMENT                                           │
└─────────────────────────────────────────────────────────────────┘
├─ Get Player Position ──> P
│
├─ Get Bend Points ──> Array
│    ├─ Get [Array.Length - 3] ──> A (Previous Fixed)
│    └─ Get [Array.Length - 2] ──> B (Last Fixed, to be removed)
│
└─ Continue...

┌─────────────────────────────────────────────────────────────────┐
│ 3. LINE OF SIGHT CHECK                                          │
└─────────────────────────────────────────────────────────────────┘
├─ Capsule Sweep Between (P, A, Hit Result, SphereRadius * 0.5)
│    │
│    ├─ [Branch: Hit?] ──> Return (path blocked, can't unwrap)
│    │
│    └─ No Hit ──> Path is clear!
│
┌─────────────────────────────────────────────────────────────────┐
│ 4. REMOVE BENDPOINT                                             │
└─────────────────────────────────────────────────────────────────┘
├─ Remove Bend Point At (Array.Length - 2)
│
├─ Set UnwrapCooldownTimer = UnwrapCooldown
└─ Set WrapCooldownTimer = WrapCooldown / 2.0
```

---

## Event Graph Structure

```
Event OnRopeTickAttached
    │
    ├─ Subtract Delta Time from WrapCooldownTimer
    ├─ Subtract Delta Time from UnwrapCooldownTimer
    │
    ├─ Call CheckForWrap
    └─ Call CheckForUnwrap
```

---

## Variables Privées (Internal State)

```
├─ WrapCooldownTimer (float) = 0.0
└─ UnwrapCooldownTimer (float) = 0.0
```

---

## Avantages de cette Approche

### ✅ Itération Rapide
- Tweaker les valeurs en temps réel
- Voir immédiatement les changements
- Pas besoin de recompiler

### ✅ Debugging Facile
- Breakpoints sur chaque étape
- Watch les variables en direct
- Print String pour tracer l'exécution

### ✅ Logique Visible
- Designers peuvent comprendre le flow
- Facile à modifier sans toucher au C++
- Documentation visuelle gratuite

### ✅ Flexibilité
- Ajouter des conditions custom facilement
- Tester différentes stratégies de wrap
- Brancher des events pour effets visuels

---

## Optimisations Futures (Optionnel)

### Si Performance Devient un Problème

**Option 1 : Throttle les Checks**
```
Every Nth Frame:
    └─ Check For Wrap/Unwrap
```

**Option 2 : Distance Threshold**
```
If Distance(Player, LastWrapCheck) > 100:
    └─ Check For Wrap
```

**Option 3 : Blueprint Nativization**
- Compiler le Blueprint en C++ natif
- Garde la flexibilité du design
- Performance proche du C++ pur

---

## Notes de Mise en Œuvre

1. **Créer un Blueprint enfant** de `RopeSystemComponent` (ex: `BP_RopeSystem`)
2. **Implémenter les events** ci-dessus dans l'Event Graph
3. **Exposer les variables** en EditAnywhere pour tweaking
4. **Tester progressivement** : d'abord Wrap seul, puis Unwrap
5. **Affiner les valeurs** de cooldown et distances

