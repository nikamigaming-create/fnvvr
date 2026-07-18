# Next Steps

The active target is a crisp retail-only headset path: independent 6DoF head
tracking, flat retail UI, right-hand weapon aim, and then complete native stereo
world presentation.

## Non-Negotiable Runtime Contract

- Retail Fallout: New Vegas remains authoritative for gameplay, UI, weapons,
  projectiles, animation, collision, saves, quests, and mod behavior.
- The standalone OpenXR host supplies predicted HMD/controller poses, input
  intent, composition, and diagnostics.
- Engine-sensitive work stays in xNVSE. Every shared mapping must have a
  fixed/versioned layout, a stable-snapshot rule, and one declared producer per
  mutable field before it is trusted.
- Startup, menus, pause, Pip-Boy, inventory, dialogue, VATS, loading, and
  unknown/non-gameplay states use the flat mono retail surface.
- All non-blocking gameplay requires true binocular stereo, independent 6DoF,
  a tracked authoritative retail weapon, and no persistent gameplay HUD.
  Invalid gameplay stereo is a visible rejection, not a mono fallback.
- UI exit holds the last valid quad until one fresh complete pose-matched eye
  transaction is ready, then switches to world stereo atomically.

## Phase 0: Lock the Baseline

- Run the launch-safety audit, x64 tests, and Win32 build.
- Capture one known-save baseline with no camera/weapon transform injection.
- Record HMD, eye, controller, retail camera, player, and rig-node state in one
  time-correlated log.
- Walk exterior to interior and back while keeping an NPC in view; record cell
  changes, hook continuity, resolved-target parity, and per-eye draw work.
- Keep a one-switch fail-closed path that disables retail mutation and blocks
  headset launch. Flat presentation remains an allowed UI mode only.
- During input telemetry, require advancing XInput packet and DirectInput frame
  heartbeats; a stopped producer must visibly neutralize controls within
  250 ms without falling through to an unrelated physical-controller path.

## Phase 1: Decouple Head and Body

- Define the retail player/body transform as the locomotion frame.
- Apply headset yaw/pitch/roll to the eye camera locally; do not rotate the
  actor every time the player looks around.
- Convert local HMD translation through the recentered body frame before it
  reaches the camera.
- Make snap/smooth turn or explicit recenter the only operations that
  intentionally change body heading.
- Add telemetry for desired head transform, pre-apply camera transform,
  post-apply camera transform, player transform, and the reason any write was
  rejected.

Head acceptance gate: look around and lean in place while the player actor and
movement heading stay stable, then turn/recenter deliberately and verify no
jump, drift, or stale offset.

## Phase 2: Calibrate Right-Hand Weapon Aim

- Publish the current right aim pose with tracking-valid/current flags.
- Log the retail first-person weapon root, hand chain, muzzle/projectile nodes,
  camera, and player/body transforms in the same frame.
- Solve one explicit OpenXR-to-retail coordinate conversion and configurable
  grip-to-weapon offset; do not scatter sign flips through hooks.
- First drive only the visual weapon/arm rig and verify handedness, scale,
  roll, and two-handed/FABRIK constraints.
- Then route the desired muzzle forward direction to the engine-side firing or
  hit-ray hook, keeping retail responsible for ammunition, recoil, spread,
  projectile creation, and damage.
- Compare controller ray, muzzle ray, projectile/hit ray, and impact point at
  near, medium, and long range.

Weapon acceptance gate: where the physical controller points, the visible
barrel, retail projectile/hit result, and impact marker agree within a logged
tolerance, including after recoil and reload.

## Phase 3: Keep UI Deliberately Flat

- Treat runtime UI classification as authoritative, not pixel heuristics.
- Suppress world stereo for every blocking/interactive UI state and present the
  retail mono frame on the stable headset surface.
- Keep pointer ray, click, scroll, back, and controller navigation mapped to
  retail input only.
- Require a short stable-gameplay debounce plus a fresh complete eye pair
  before removing the flat surface.
- On any UI transition, restore the flat surface before discarding the last
  world pair. Stale or invalid gameplay stereo publishes no successful frame.

UI acceptance gate: open and close pause, Pip-Boy/inventory, dialogue, VATS,
loading transitions, and nested menus without blank frames, stereo menus,
double input, or a lost pointer.

## Phase 4: Native Retail Stereo

- Capture two complete retail world eyes from the same simulation tick.
- Suspend the second engine traversal while the player cell/camera origin is
  unknown or settling, and reset all eye-pair continuity on a cell epoch.
- Use the OpenXR per-eye poses and asymmetric FOV values that correspond to the
  submitted frame.
- Exclude retail UI from the world eye pair; UI remains the mono surface.
- Validate depth clears, state restoration, culling, transparencies, first-
  person geometry, particles, and performance in several cells.
- Reject incomplete, stale, identical, UI-contaminated, pose-mismatched,
  one-target-only, underfilled, or structurally divergent pairs; publish no
  gameplay frame for a rejected transaction.
- First isolate culling: during `AccumulateScene`, use the center/union camera
  only while visibility is built, restore the selected eye camera before
  `RenderScene`, and compare registered geometry IDs, resolved targets, draw
  counts, shader-call counts, and hashes.
- Scope the world accumulator/culler and run `ProcessAlt` once in list mode to
  produce one conservative `NiVisibleArray`. Create fresh non-aliased left and
  right accumulators. For each eye, bind its exact camera, color, depth, and
  auxiliary targets before `AddVisibleArray` because population can draw
  immediately; then render and finalize that accumulator once.
- Begin center/center and require identical registered geometry and draw
  structure. Only then enable distinct eye cameras/projections and require the
  expected pixel separation without structural divergence.
- Install no address- or vtable-based hook until the loaded `1.4.0.525` module,
  function prologues, call signatures, singleton pointers, and object
  ownership validate. Failed validation leaves retail mutation inert and live
  VR blocked.

## Extending xNVSE Safely

Yes, the plugin can be expanded to extract additional retail details, but each
new field needs a concrete consumer and proof:

1. identify the authoritative engine object or hook;
2. validate executable version, pointer chain, and scene/runtime phase;
3. log the value before mutating anything;
4. add a fixed protocol field or a versioned mapping;
5. consume it in the host only if magic/version/sequence checks pass;
6. keep gameplay-critical application inside the retail process;
7. fail closed when the structure or timing is not proven.

The goal is not to reproduce FNV outside FNV. The goal is to give the retail
engine precise VR intent, read back authoritative results, and present those
results correctly in OpenXR.
