# FNVVR Retail Status

Last updated: 2026-07-12

## Direction Locked

The product path is one retail `FalloutNV.exe`, one standalone OpenXR host, the
FNVVR xNVSE plugin, and the retail proxy DLLs. There is no alternate game
runtime or world handoff in the shipping architecture.

Retail UI is intentionally flat for this phase. Startup/menu mode, inventory,
Pip-Boy, dialogue, VATS, loading, pause, and unknown/non-gameplay states must
present the normal retail mono frame on the OpenXR UI surface. Stereo world
presentation may resume only after runtime state and a valid retail eye pair
both prove unobstructed gameplay.

## Proven Foundation

- xNVSE `6.4.8` and OpenXR SDK `release-1.1.60` are pinned and fetched into
  ignored dependency directories.
- The 32-bit xNVSE plugin and D3D9/DirectInput/XInput proxies build for retail
  FNV `1.4.0.525`.
- Retail FNV loads the plugin and proxy DLLs through the normal xNVSE/game
  process.
- The standalone host creates a real OpenXR session and samples HMD, grip, aim,
  buttons, triggers, squeeze, and thumbsticks.
- Shared pose ABI v5 carries independent left/right grip and aim poses with
  active/current tracking flags. The retail right-hand rig uses grip position
  and current aim orientation, with a guarded grip-orientation fallback.
- Pose/input shared state reaches the live retail process; retail publishes
  runtime, camera, player, menu, and weapon-class telemetry back to the host.
- XInput v2 and DirectInput v9 use versioned mapping names and stable
  odd/even snapshots. DirectInput look uses cumulative angle counters consumed
  once across state/buffered polling; 250 ms producer staleness neutralizes
  held buttons, triggers, sticks, grip, pointer, and look state.
- The flat retail frame can be presented and the shared pointer/click path has
  worked in tested menus. Generic close/back semantics and exhaustive nested or
  mod-added menu coverage are not signed off.
- The plugin distinguishes gameplay from menu, Pip-Boy, dialogue, VATS,
  loading, other visible TileMenu states, and an active non-HUD interface menu
  even when its ID is mod-added or its TileMenu pointer lags. The raw engine
  MenuMode bit is retained as a diagnostic because live gameplay can report it
  continuously.
- The retail D3D9 proxy captures mono frames and contains replay/readback and
  complete-eye-pair diagnostics.
- Retail player rig discovery searches for and logs arm-chain, hand, weapon,
  projectile, and muzzle-flash nodes; retained runs have not proved a complete
  non-null weapon/projectile/muzzle chain. FABRIK solver tests cover the arm
  math foundation.

## Current Headset Boundary

### Head tracking

HMD orientation is available on both sides of the bridge and the retail camera
apply path exists. The last observed behavior was not an acceptable 6DoF
camera: looking around pivoted too much of the player/body frame with the head.
Until a live test proves otherwise, head/body decoupling is the first blocker.

Acceptance requires:

- ordinary head yaw/pitch/roll changes the eye cameras without continuously
  rewriting the player actor transform;
- local headset translation is applied in the correct camera/body frame and
  returns cleanly after recenter;
- movement/turn controls retain a deliberate body heading independent of
  moment-to-moment head look;
- entering or leaving any UI mode cannot leave a stale camera transform behind.

### Weapon aim

The OpenXR right aim pose is consumed only as the orientation source for the
right-hand IK target; grip position still anchors the wrist. The weapon node is
discovered and logged but is not directly solved, and no muzzle,
projectile-launch, or hit-ray hook currently consumes the aim pose.
Controller-to-weapon calibration and authoritative firing alignment are not
yet proven; visual hand motion alone is insufficient.

Acceptance requires one transform chain to agree on:

- right-controller aim pose;
- retail weapon and arm pose;
- muzzle position and forward direction;
- projectile launch or engine hit ray;
- crosshair/impact diagnostics;
- recoil and animation recovery.

The first proof should use a known ranged weapon in a controlled save, log the
controller ray and retail muzzle/raycast in the same coordinate space, and
compare both direction and impact point over several distances.

### Rendering

The retained July 11 run identifies a concrete scene-consistency failure. At
the exterior-to-interior transition, retail cell `896697` changed to `1073541`
without a loading gate. A published pair then had only one resolved target
path (`0/1`), and a later interior pair submitted `1107/1629` draws and
`3978/5714` vertex-shader constant calls. Pixel separation alone incorrectly
allowed this work to look like successful stereo.

The legacy native hook invoked the full Gamebryo render boundary once per eye.
It could not restore every culling, portal, occlusion, visibility, effect, and
render-list side effect consumed by the first traversal. That path remains
disabled in the production stereo environment.

The working tree now implements a coherent single-traversal producer:

- the verified `DoRenderFrame` boundary at `0x008706B0` receives one
  body-local HMD camera and the union of both OpenXR eye frusta;
- Gamebryo culls and submits one scene exactly once;
- each exact submitted D3D9 draw is replayed to left and right eye targets,
  with fixed-function view/projection updates and validated shader WVP deltas;
- shared stereo provenance is `StereoProducerSingleTraversal`, with the
  rendered pose sequence and display time attached to the pixels;
- runtime telemetry rejects camera drift, invalid rotation matrices, missing
  eye replay, non-separated images, legacy double traversal, head/body
  coupling, and gameplay flat-plane transitions.

The union frustum prevents ordinary left/right eye culling loss, although D3D9
replay cannot recover geometry that Gamebryo omitted before submission. A live
headset run is still required to prove the new boundary visually.

Loaded-process inspection verified `AccumulateScene` at `0x00B6BEE0`,
`RenderScene` at `0x00B6C0D0`, the SceneGraph camera at `+0xAC`, and the world
culler at `+0xB4`. It did not verify the previously proposed TESMain `+0x88`
accumulator pointer; that claim is withdrawn, and no guessed accumulator
virtual methods are called by the production path.

## xNVSE Extraction Boundary

Extending `nvse_fnvxr.dll` is the right way to expose more retail-side detail.
The plugin runs in-process and can inspect known engine objects, scene-graph
nodes, menu/runtime state, and guarded hook points, then publish compact
versioned telemetry to the host.

That is not a blanket promise that every engine behavior can be copied out and
replayed externally. Gameplay-critical changes should remain in-process:
weapon transforms, muzzle/projectile direction, hit tests, activation, and
animation state must be applied or confirmed by retail FNV. The standalone
host should publish desired poses/input and compose the returned retail frames.
Unknown layouts need executable-version checks, pointer validation, logging,
and a fail-closed fallback.

## Immediate Blockers

1. Run one guarded exterior/interior/head-turn headset test and require the live
   analyzer to pass single traversal, camera matrix, WVP, provenance,
   separation, head/body decoupling, and no-gameplay-2D-transition checks.
2. Keep retail IK, synthetic hands, and weapon writes disabled during that
   camera/stereo acceptance run.
3. Equip the deterministic ranged-weapon loadout, prove right-arm/weapon/muzzle
   discovery, and calibrate controller-to-weapon visual alignment.
4. Align the retail muzzle/projectile/hit ray to the same controller transform
   and verify impacts while retail retains combat authority.

## Safe Local Artifacts

- staged plugin layout under `local/fnv-plugin-stage/`;
- dependency manifest at `deps/manifest.json`;
- retail probe at `local/fnv-probe.json`;
- combined preflight at `local/preflight-fnvxr.json`;
- run manifests and telemetry under ignored `local/` directories or the local
  retail game root.
