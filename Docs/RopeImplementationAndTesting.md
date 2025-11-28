# Rope Implementation & Testing Guide

This guide summarizes the current rope system architecture, how the recent changes should be used, and how to validate the behavior in editor builds.

## Gameplay rope (URopeSystemComponent)
- **State machine**: `Idle → Flying → Attached`. The component owns bend points (`BendPoints`) where index `0` is the anchor/hit point and the last entry is always the player location.
- **Collision and wrapping**: The active segment (last fixed point → player) uses capsule sweeps (`SweepForHit`) and a short binary refinement (`RefineImpactPoint`) to place bend points outside of blocking geometry. Cooldowns (`WrapCooldown` / `UnwrapCooldown`) prevent oscillations when moving near corners.
- **Length authority**: `CurrentLength` is recomputed every tick from the geometry via `ComputeTotalPhysicalLength()` and clamped to `MaxLength`. This value governs stretching forces.
- **Forces**: `ApplyForcesToPlayer()` projects the player’s velocity, clamps outward motion when the rope is taut, and applies spring/tangent forces (`SpringStiffness`, `SwingTorque`, `AirControlForce`).
- **Rendering bridge**: `UpdateRopeVisual()` forwards bend points to the cosmetic renderer each tick after gameplay updates.

## Cosmetic rope (URopeRenderComponent)
- **Anchored simulation**: Every gameplay bend point is converted to a pinned verlet particle (anchors stored in `AnchorIndices`). Only interior particles move during constraint solving.
- **Segment-aware rest lengths**: Each consecutive pair of verlet particles stores its rest length so the visual rope stays consistent even when segments differ in size.
- **Rendering**: An `UInstancedStaticMeshComponent` draws cylinder instances between verlet particles. Set `RopeMesh` to a 100-unit-tall cylinder mesh for correct scaling.
- **Tuning knobs**: Adjust `SubdivisionsPerSegment`, `Damping`, `GravityScale`, and `SolverIterations` to balance stability and visual smoothness.

## How to extend
- To tighten reel-in behavior, call `ReelIn()` each frame when input is held; the reduced `CurrentLength` will increase stretch and pull the player toward the last fixed bend point.
- To alter collision feel, tweak `RopeRadius`, `BendOffset`, and `CornerThresholdDegrees`. Larger radii improve robustness at the cost of earlier wrapping.
- For networked play, replicate only bend points and `CurrentLength`; each client runs its own cosmetic simulation.

## Testing checklist
1. **Single-corner wrap**: Swing toward a wall edge; verify a single bend point is inserted and removed after clearing the corner (cooldowns prevent flicker).
2. **Multi-corner path**: Move in a zig-zag around multiple obstacles; the rope should stack bend points up to `MaxBendPoints` without tunneling.
3. **Reel in/out feel**: Hold the reel-in input while hanging; the player should accelerate toward the last fixed point. Reel-out should relax tension up to `MaxLength`.
4. **Penetration guard**: Rapidly cross thin obstacles; capsule sweeps plus refinement should keep bend points outside geometry.
5. **Visual stability**: With `RopeMesh` assigned, confirm cylinders stay attached at all bend points and segments remain smooth when swinging.

Record any anomalies (e.g., missed wraps, jitter, excessive stretch) with the map name and reproduction steps to iterate quickly.
