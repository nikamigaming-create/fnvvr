# FNVVR Production Architecture v2

Decision date: 2026-07-18

## Technical Decision

The retail Fallout: New Vegas VR target is technically feasible, with one
hard architectural correction:

- **GO:** an exact-version, engine-level stereo renderer using one conservative
  visible set and two fresh eye accumulators, followed by GPU-native OpenXR
  color/depth transport;
- **NO-GO:** replaying individual D3D9 draws for both eyes, running the full
  Gamebryo frame twice, or moving eye pixels through a CPU readback/ring.

The GO is for the bounded implementation path, not a claim that production VR
is finished. All live presentation and retail mutation remain source-fused
until the implementation and acceptance gates below pass.

## Product Contract

There are exactly two user-visible content modes:

1. `WorldStereo`: all non-blocking gameplay, exploration, combat, and world
   interaction use true binocular 3D, independent 6DoF head motion, a tracked
   retail weapon, and no persistent gameplay HUD.
2. `UiQuad`: startup, pause, inventory, barter, terminals, dialogue, VATS,
   loading, Pip-Boy, and other blocking retail UI use a stable mono quad. The
   controller ray drives the ordinary retail mouse pointer and click path.

Mono gameplay is never a successful fallback. When UI closes, the last valid
quad remains visible until one fresh, complete, pose-matched stereo transaction
is ready; the compositor then changes to `WorldStereo` atomically. If that
transaction does not arrive inside the fixed hold window, presentation fails
visibly instead of relabeling a mono game frame as VR.

## Authority Boundary

Retail `FalloutNV.exe` remains authoritative for simulation, cells, collision,
quests, saves, UI, animation, equipped weapons, ammunition, recoil, spread,
projectiles, hit tests, damage, and the rendered world. The OpenXR host owns
predicted headset/controller poses, actions, frame timing, accepted resource
composition, and non-authoritative diagnostics.

Gameplay-critical pose application stays inside the retail process. The host
publishes intent and opens accepted GPU resources; it never simulates a second
copy of the game.

## Verified Retail Engine Boundary

Read-only inspection of the loaded standard retail `1.4.0.525` executable has
established these primitives:

- `RenderWorldSceneGraph` at `0x00873200` is a complete instruction-aligned
  world-render boundary.
- `AccumulateScene` at `0x00B6BEE0` invokes the world culling process.
- `BSCullingProcess::ProcessAlt` at `0x00C4F070` can temporarily consume an
  explicit `NiVisibleArray`, restoring the previous array afterward.
- `NiAccumulator::AddVisibleArray` at `0x00A9B790` can draw some geometry
  immediately. Eye camera, color, depth, and auxiliary targets must therefore
  be bound before population.
- `RenderAccumulatorWithoutFinalize` at `0x00B6BA20` and the finalize wrapper
  at `0x00B6B930` provide the required per-accumulator split.
- `RenderAndFinalizeAccumulator` at `0x00B6C0D0` consumes one accumulator; it
  must not be reused as both eyes.

The exact loaded PE identity and complete function-body SHA-256 values live in
`runtime/fnvxr_retail_engine_manifest.h`. They matched two independent memory
dumps and a fresh hidden flat launch. Production code must validate the mapped
executable, not the Steam-encrypted on-disk code bytes.

The stock world renderer contains two accumulation branches selected by retail
state. The production backend must support both branches, or deliberately
control and restore the selector as part of the same transaction. A global hook
on the shared accumulation helpers is forbidden because auxiliary render paths
also use them.

## Stereo World Transaction

One accepted gameplay frame is a fail-closed transaction:

1. Validate the loaded PE identity and every required function hash.
2. Snapshot authoritative camera, culler, accumulator, render-target, depth,
   viewport, shader, and auxiliary state.
3. Derive the predicted body-local head pose, both exact eye cameras, and one
   conservative union visibility camera/frustum for the same simulation tick.
4. Build one explicit conservative `NiVisibleArray` without advancing the game
   a second time.
5. Create fresh, non-aliased left and right accumulators.
6. For the left eye, bind its exact camera plus isolated color, encoded-depth,
   and auxiliary targets before adding the shared visible array; render and
   finalize that accumulator exactly once.
7. Repeat step 6 for the right eye with a separate accumulator and the exact
   same visible-array identity.
8. Validate complete color/depth/resource-graph coverage, pose identity,
   transaction identity, and GPU completion.
9. Restore every authoritative retail state item before publication.
10. Publish both eyes atomically. Any failure restores state, discards isolated
    outputs, and publishes no gameplay frame.

Center/center is the first implementation proof: both eye passes must have the
same registered geometry and draw structure. Distinct eye transforms are
enabled only after that passes.

## GPU Transport

Eye pixels remain GPU-native. The retail producer publishes only versioned
metadata: adapter LUID, shared color/depth handles, dimensions/formats, a GPU
completion sequence or fence, transaction ID, source frame, pose sequence, and
rendered display time. The host opens the resources on the matching adapter and
submits color plus matching depth to OpenXR.

The transport contract is defined in `protocol/fnvxr_gpu_frame_transport.h`.
CPU eye-image arrays, missing depth, aliased eye resources, unstable metadata,
or incomplete GPU work are explicit architecture failures.

## Weapon and UI Commit Rules

The right-controller aim pose, visible retail weapon/barrel, muzzle direction,
projectile or hit ray, and impact result must share one calibrated transform
chain. Retail retains ammunition, recoil, spread, animation, projectile, and
damage authority. A host-only debug hand or ray does not satisfy this gate.

Retail HUD pixels are excluded from `WorldStereo`. Blocking retail UI switches
the whole presentation to `UiQuad`; no synthetic gameplay HUD is composited on
top of the world.

## Release Gates

The source fuses may change only after all of these pass:

- exact loaded-runtime identity/hash validation and both stock world branches;
- deterministic center/center, then distinct-eye engine transactions with full
  state restoration and no resource aliasing;
- GPU-native per-eye color and depth through D3D9Ex/D3D11/OpenXR;
- authoritative weapon, muzzle, projectile/hit, recoil, and reload alignment;
- UI entry/exit coverage with stable pointer input and no gameplay HUD;
- signed translation X/Y/Z and yaw/pitch/roll evidence tied to the exact
  rendered/submitted transaction;
- exterior, interior, cell-transition, NPC, particle, transparency, first-
  person, and performance acceptance in the complete retail game.

Until then the safe default is inert retail code and a blocked headset launch.
