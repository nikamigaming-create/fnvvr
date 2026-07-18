# FNVVR Retail Status

Last updated: 2026-07-18

## Direction Locked

The product path is one retail `FalloutNV.exe`, one standalone OpenXR host, the
FNVVR xNVSE plugin, and the retail proxy DLLs. There is no alternate game
runtime or world handoff in the shipping architecture.

All non-blocking gameplay, exploration, combat, and world interaction require
true binocular 3D, independent 6DoF head motion, a tracked retail weapon, and
no persistent gameplay HUD. Mono gameplay cannot satisfy acceptance.

Startup, pause, inventory, barter, terminals, dialogue, VATS, loading,
Pip-Boy, and other blocking retail UI use the stable mono quad. The controller
ray drives retail mouse pointer/click input. On UI exit, the last valid quad is
held until one fresh complete pose-matched stereo transaction is ready, then
the compositor changes to world stereo atomically.

## Proven Foundation

- xNVSE `6.4.8` and OpenXR SDK `release-1.1.60` are pinned and fetched into
  ignored dependency directories.
- The 32-bit xNVSE plugin and D3D9/DirectInput/XInput proxies build for retail
  FNV `1.4.0.525`.
- Retail FNV loads the plugin and proxy DLLs through the normal xNVSE/game
  process. Exact standard-retail runtime identity is required; the no-gore and
  editor runtimes are rejected.
- Retail mutation remains hard source-fused. The current plugin returns inert
  before creating mappings/listeners or applying camera, input, rig, or code
  hooks. Config and environment values cannot bypass that fuse.
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
- The retail D3D9 proxy's mono UI capture remains useful. Its per-draw replay
  and CPU readback/ring are historical diagnostics and a production NO-GO.
- Retail player rig discovery searches for and logs arm-chain, hand, weapon,
  projectile, and muzzle-flash nodes; retained runs have not proved a complete
  non-null weapon/projectile/muzzle chain. FABRIK solver tests cover the arm
  math foundation.

## Current Headset Boundary

All live OpenXR presentation is source-blocked in both the host executable and
the guarded launcher. This includes the former flat-compositor baseline. The
retained live observations below explain the boundary; they are not a current
authorization to launch the game or headset.

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

The per-D3D-draw producer is rejected as the production path. Read-only loaded-
process inspection now establishes a bounded engine-level replacement:

- `RenderWorldSceneGraph` at `0x00873200` is a complete function boundary;
- `AccumulateScene` at `0x00B6BEE0` invokes the world culler, whose
  `ProcessAlt` path can temporarily consume an explicit `NiVisibleArray`;
- the accumulator `SetCamera`, `AddVisibleArray`, render, and finalize methods
  have complete instruction-aligned loaded-memory hashes;
- `AddVisibleArray` may draw some geometry immediately, so an eye's exact
  camera, color, depth, and auxiliary targets must be bound before population;
- `RenderAccumulatorWithoutFinalize` at `0x00B6BA20` and the finalize wrapper
  at `0x00B6B930` expose the split needed for separate eye accumulators; and
- the SceneGraph camera at `+0xAC` and world culler at `+0xB4` were observed
  live and non-null after initialization.

The required schedule is snapshot, one conservative union visible set, fresh
left accumulator with all left-eye state bound before populate/render/finalize,
fresh right accumulator with the exact same visible set and right-eye state,
then complete restoration and atomic publication. Failure discards isolated
outputs. The backend must handle both stock world-accumulation branches and
must not globally hook helpers shared by auxiliary passes.

All selected functions were byte-identical across two independent loaded-memory
dumps and a fresh default flat launch. The exact PE identity and function hashes
are now a tested manifest and read-only runtime probe. This is a GO for the
bounded engine renderer implementation, not proof that headset VR is finished.

## xNVSE Extraction Boundary

Extending `nvse_fnvxr.dll` is the right way to expose more retail-side detail.
The plugin runs in-process and can inspect known engine objects, scene-graph
nodes, menu/runtime state, and guarded hook points, then publish compact
versioned telemetry to the host.

That is not a blanket promise that every engine behavior can be copied out and
replayed externally. Gameplay-critical changes should remain in-process:
weapon transforms, muzzle/projectile direction, hit tests, activation, and
animation state must be applied or confirmed by retail FNV. The standalone
host should publish desired poses/input, open accepted GPU resources, and
compose only complete color/depth transactions. Unknown layouts need exact
executable checks, pointer validation, logging, and fail-closed rejection.

## Immediate Blockers

1. Implement the exact-version/hash-validated world-boundary backend for both
   stock accumulation branches with complete authoritative state restoration.
2. Prove center/center, then distinct-eye transactions using one conservative
   visible set and fresh non-aliased per-eye accumulators.
3. Implement GPU-native D3D9Ex-to-D3D11/OpenXR color and encoded-depth sharing
   with handles, adapter identity, completion sequencing, and transaction IDs;
   no CPU eye-pixel ring.
4. Prove the authoritative retail weapon, muzzle, projectile/hit, recoil, and
   reload chain against the right-controller aim pose.
5. Pass retail/headset acceptance across UI transitions, exterior/interior cell
   changes, first-person geometry, particles/transparency, twelve-direction
   pose motion, and performance. Invalid gameplay stereo is a visible reject,
   never a successful mono fallback.

## Safe Local Artifacts

- staged plugin layout under `local/fnv-plugin-stage/`;
- dependency manifest at `deps/manifest.json`;
- retail probe at `local/fnv-probe.json`;
- combined preflight at `local/preflight-fnvxr.json`;
- run manifests and telemetry under ignored `local/` directories or the local
  retail game root;
- recoverable backups of the stale installed mutators under
  `local/installed-artifact-backups/20260718-050105-432/`. The live game-root
  copies were replaced by current source-fused/inert builds.
