# Blueprint: Character Rope - Input Bindings

## Setup Components (Construction Script ou BeginPlay)

```
BeginPlay
    │
    ├─ Get Component By Class (RopeSystemComponent)
    │    └─ Save to Variable: RopeSystem
    │
    └─ Get Component By Class (AimingComponent)
         └─ Save to Variable: AimingComp
```

---

## Input Action: IA_FireHook (Triggered)

```
IA_FireHook [Triggered]
    │
    ├─ [Branch: Is Valid? RopeSystem]
    │       │
    │       └─ False ──> Return
    │
    ├─ [Branch: RopeSystem->IsRopeAttached?]
    │       │
    │       ├─ True ──> RopeSystem->Sever() ──> Return
    │       │
    │       └─ False ──> Continue...
    │
    ├─ Get Control Rotation
    │    └─ Get Forward Vector ──> Direction
    │
    ├─ [Optional: Get Aim Trace Direction from AimingComp]
    │    └─ If AimingComp exists and has target ──> Use that Direction
    │
    └─ RopeSystem->FireHook(Direction)
```

### Alternative avec Hand Socket

```
IA_FireHook [Triggered]
    │
    ├─ Get Mesh Component
    │    └─ Get Socket Location (hand_r) ──> HandPos
    │
    ├─ Get Control Rotation
    │    └─ Get Forward Vector
    │    └─ Scale by 2000 ──> Launch Velocity
    │
    ├─ Calculate Direction from Hand to Crosshair
    │    └─ (CrosshairWorldPos - HandPos).Normalized ──> Direction
    │
    └─ RopeSystem->FireHook(Direction)
```

---

## Input Action: IA_ReelIn (Started / Ongoing)

```
IA_ReelIn [Started]
    │
    └─ Set Variable: bIsReelingIn = True

Event Tick
    │
    ├─ [Branch: bIsReelingIn?]
    │       │
    │       └─ True ──> RopeSystem->ReelIn(Delta Time)
    │
    └─ Continue...

IA_ReelIn [Completed / Cancelled]
    │
    └─ Set Variable: bIsReelingIn = False
```

---

## Input Action: IA_ReelOut (Started / Ongoing)

```
IA_ReelOut [Started]
    │
    └─ Set Variable: bIsReelingOut = True

Event Tick
    │
    ├─ [Branch: bIsReelingOut?]
    │       │
    │       └─ True ──> RopeSystem->ReelOut(Delta Time)
    │
    └─ Continue...

IA_ReelOut [Completed / Cancelled]
    │
    └─ Set Variable: bIsReelingOut = False
```

---

## Input Action: IA_CutRope (Triggered)

```
IA_CutRope [Triggered]
    │
    ├─ [Branch: Is Valid? RopeSystem]
    │       │
    │       └─ False ──> Return
    │
    └─ RopeSystem->Sever()
```

---

## Input Action: IA_StartAiming (Triggered)

```
IA_StartAiming [Triggered]
    │
    ├─ [Branch: RopeSystem->IsRopeAttached?]
    │       │
    │       └─ True ──> Return (don't aim while swinging)
    │
    ├─ Call: StartAiming() [from CharacterRope.h]
    │    ├─ bIsPreparingHook = True
    │    ├─ bUseControllerRotationYaw = True
    │    └─ CharacterMovement->bOrientRotationToMovement = False
    │
    └─ [Optional] AimingComp->StartAiming()
```

---

## Input Action: IA_StopAiming (Triggered)

```
IA_StopAiming [Triggered]
    │
    ├─ Call: StopAiming() [from CharacterRope.h]
    │    ├─ bIsPreparingHook = False
    │    ├─ bUseControllerRotationYaw = False
    │    └─ CharacterMovement->bOrientRotationToMovement = True
    │
    └─ [Optional] AimingComp->StopAiming()
```

---

## Event: OnRopeAttached (Custom Event from RopeSystem)

```
RopeSystem->OnRopeAttached (Hit Result)
    │
    ├─ Play Sound 2D (Hook Impact Sound)
    │
    ├─ Play Animation Montage (Rope Attach Montage)
    │
    ├─ Spawn Emitter at Location (Hit.ImpactPoint, Sparks VFX)
    │
    ├─ Camera Shake (Light Shake)
    │
    └─ [Optional] HUD Update (Show Rope UI)
```

---

## Event: OnRopeSevered (Custom Event from RopeSystem)

```
RopeSystem->OnRopeSevered
    │
    ├─ Play Sound 2D (Rope Cut Sound)
    │
    ├─ Stop Animation Montage (Rope Attach Montage)
    │
    └─ [Optional] HUD Update (Hide Rope UI)
```

---

## Animation Blueprint Integration

### Variables

```
├─ bIsPreparingHook (bool) - From Character
├─ bIsSwinging (bool) - Derived
└─ RopeLength (float) - From RopeSystem
```

### AnimGraph

```
Idle/Walk/Run State Machine
    │
    ├─ [Transition: bIsPreparingHook?]
    │    └─ Aiming Pose
    │
    └─ [Transition: bIsSwinging?]
         └─ Swing Animation
              ├─ Blend by RopeLength
              │    ├─ Short Rope ──> Tight Swing
              │    └─ Long Rope ──> Wide Swing
              │
              └─ Layered Blend
                   └─ Upper Body ──> Holding Rope Pose
```

### Event Graph

```
Event Blueprint Update Animation
    │
    ├─ Get Owning Pawn ──> Cast to CharacterRope
    │    │
    │    ├─ Get bIsPreparingHook ──> Set AnimBP Variable
    │    │
    │    └─ Get RopeSystemComponent
    │         ├─ IsRopeAttached? ──> Set bIsSwinging
    │         └─ GetCurrentLength ──> Set RopeLength
    │
    └─ Continue...
```

---

## HUD/UI Updates (Optional)

### Widget Blueprint: RopeHUD

```
Event Construct
    │
    └─ Bind to Character's RopeSystem Events

On RopeAttached
    │
    ├─ Show Rope Length Bar
    ├─ Show Reticle (different style while attached)
    └─ Animate In

On RopeSevered
    │
    ├─ Hide Rope Length Bar
    └─ Animate Out

Event Tick
    │
    ├─ Get RopeSystem->GetCurrentLength
    ├─ Get RopeSystem->GetMaxLength
    └─ Update Progress Bar (CurrentLength / MaxLength)
```

---

## Configuration Recommandée (Input Mapping)

```
Enhanced Input Actions:

IA_FireHook
    ├─ Mouse Left Button (Press)
    └─ Gamepad Right Trigger (Press)

IA_ReelIn
    ├─ Mouse Wheel Up (Axis)
    └─ Gamepad Right Shoulder (Hold)

IA_ReelOut
    ├─ Mouse Wheel Down (Axis)
    └─ Gamepad Left Shoulder (Hold)

IA_CutRope
    ├─ E Key (Press)
    └─ Gamepad B Button (Press)

IA_StartAiming
    ├─ Mouse Right Button (Press)
    └─ Gamepad Left Trigger (Press)

IA_StopAiming
    ├─ Mouse Right Button (Release)
    └─ Gamepad Left Trigger (Release)
```

---

## Testing Checklist

### Phase 1: Hook Firing
- [ ] Hook spawns correctly
- [ ] Hook flies in aimed direction
- [ ] Hook attaches on impact
- [ ] OnRopeAttached event fires

### Phase 2: Basic Swing
- [ ] Player swings around anchor
- [ ] ReelIn shortens rope
- [ ] ReelOut lengthens rope
- [ ] Momentum feels good

### Phase 3: Bendpoints (Blueprint Logic)
- [ ] Wrapping creates bendpoints
- [ ] Bendpoints stick to geometry
- [ ] Unwrapping removes bendpoints
- [ ] No infinite loops

### Phase 4: Polish
- [ ] Animations play correctly
- [ ] Sounds trigger
- [ ] VFX spawn
- [ ] HUD updates

---

## Performance Tips

1. **Throttle Trace Checks**
   - Don't check wrap/unwrap every frame if rope is stable
   - Use distance thresholds

2. **Optimize Debug Drawing**
   - Disable in shipping builds
   - Use console variable to toggle

3. **Cache Component References**
   - Get components once in BeginPlay
   - Don't call GetComponentByClass every frame

4. **Use Object Pooling for VFX**
   - Pool particle systems
   - Reuse instead of spawn/destroy

