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
held until one fresh complete pose-matched stereo transaction with a strictly
newer retail source-frame identity is ready, then the compositor changes to
world stereo atomically. That freshness watermark remains enforced after the
bounded quad hold expires into a safety blank.

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
  hooks. Config and environment values cannot bypass that fuse. A future fuse
  change is still insufficient without a synchronous live PE/function/module
  integrity check at the exact mutation decision; the current validator
  deliberately rejects.
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
  The legacy flat retail producer is now runtime-fused, and direct plugin-only
  game installation is fused; the guarded stage/validate path is the only
  game-tree deployment route.
- GPU stereo metadata uses a fixed 200-byte cross-architecture v4 ABI, one
  fully ordered odd/even producer helper, non-aliased color/depth handles, an
  exact runtime-state sample identity, and a frame-bound consumer-observed
  shared-fence requirement. No GPU resource producer or consumer is implemented
  yet, so this is a tested contract rather than a completed transport.
- Retail player rig discovery searches for and logs arm-chain, hand, weapon,
  projectile, and muzzle-flash nodes; retained runs have not proved a complete
  non-null weapon/projectile/muzzle chain. FABRIK solver tests cover the arm
  math foundation.
- The four shipped Win32 DLLs compile and link reproducibly with `/Brepro` and
  non-incremental linking. Two independent clean build roots produced identical
  bytes. The pinned plugin is now SHA-256
  `E394CB96DC1F881E66DF5E11877BCCBA55033FE50A4D407FD70CCB359BB3650D`.
- The guarded deployment transaction now invalidates prior authority, assigns
  a fresh nonce, clean-builds x64 and Win32, requires nonempty complete test
  catalogs, hashes 110 first-party/vendored source inputs before and after the
  build, and binds ten exact artifact/configuration identities. The July 18
  Release transaction passed 77/77 combined tests and its independent
  ValidateOnly pass matched every installed mandatory destination without
  copying or launching anything.
- The exact 124-module retail census contract validates owned primary/post
  snapshots, process creation identity, generation and transaction ordering,
  non-overlapping PE32 mappings, mapped executable evidence, proxy policy, and
  JIP/ShowOff dispositions. Diagnostic scanning is bounded at 512 and reports
  Steam/NVIDIA overlays explicitly even when both are present. No live census
  adapter supplies these proofs yet, so production authorization remains false.
- A pure mapped-image verifier now reconstructs PE32 executable coverage,
  relocation normalization, exact patch allowances, and full stable protection
  partitions while rejecting RWX, guards, gaps, overlap, and unexpected
  executable pages. Steam-encrypted `FalloutNV.exe` still needs an independent
  clean loaded-page reference manifest; encrypted disk bytes are not treated as
  a valid loaded-image reference.
- The center/center resource owner has move-only context and cleanup
  capabilities, exact camera/culler/accumulator layouts, non-aliased left/right
  ownership, reverse-order teardown, and adversarial lifecycle tests. It has no
  production authorization issuer and invokes no retail constructor today.

## Current Headset Boundary

All live OpenXR presentation is source-blocked in both the host executable and
the guarded launcher. This includes the former flat-compositor baseline. The
retained live observations below explain the boundary; they are not a current
authorization to launch the game or headset.

The legacy stereo environment function now throws before setting activation
variables. Independent host and D3D product-integration fuses also remain
false, so changing one historical proof constant cannot revive the old mono
gameplay fallback/HUD path. The standalone OpenXR diagnostic is separately
source-fused before loading the OpenXR loader.

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

Read-only samples established stable candidate addresses and loaded instruction
blocks for the world/culling/accumulator path. That is not yet sufficient to
authorize construction or rendering: constructor arguments, ownership,
destruction, both stock branches, synchronous loaded-page identity, and the
complete compatibility patch sets remain hard gates. The strict probe exposed
a compatibility conflict: JIP LN 57.30 rewrites the
call displacement at `0x008751C7..0x008751CA` inside `RenderFirstPerson`; no
other bytes in that function differed in that sample. The target was inside the
loaded `jip_nvse.dll` (installed SHA-256
`9D2779647ED0CE63043390F47FC978E3234AF8E558DC6CB6BCB231478A2D74D4`).
Later initialization samples also showed JIP-era rewrites in accumulator and
culling helpers, proving that hash-before-initialization was a TOCTOU bug. The
probe now waits for live SceneGraph initialization, hashes afterward, and
rejects the loaded JIP module itself until its complete patch set is normalized
and bound to an exact module hash. This supports continuing the bounded engine
renderer implementation; it is not proof that headset VR is finished.

Installed `ShowOffNVSE.dll` is also treated as uncontrolled module inventory;
it has not been normalized or authorized by the engine proof. JIP and ShowOff
remain installed for the user's mod stack, but no production stereo/mutation
gate can pass while their complete loaded patch sets are unverified.

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

1. Implement the read-only live census/evidence adapter and the independently
   sampled loaded-page reference manifest for Steam-encrypted `FalloutNV.exe`.
   It must leave all proxy/JIP/ShowOff proof dispositions failed until their
   complete patch and mapped-image evidence exists.
2. Implement the exact-version/hash-validated world-boundary backend for both
   stock accumulation branches with complete authoritative state restoration,
   plus explicit hash-bound compatibility contracts for the complete loaded
   patch sets of JIP, ShowOff, and any other code-mutating module; arbitrary or
   unenumerated patched code remains a hard reject.
3. Prove center/center, then distinct-eye transactions using one conservative
   visible set and fresh non-aliased per-eye accumulators.
4. Implement GPU-native D3D9Ex-to-D3D11/OpenXR color and encoded-depth sharing
   with handles, adapter identity, completion sequencing, and transaction IDs;
   no CPU eye-pixel ring.
5. Prove the authoritative retail weapon, muzzle, projectile/hit, recoil, and
   reload chain against the right-controller aim pose.
6. Pass retail/headset acceptance across UI transitions, exterior/interior cell
   changes, first-person geometry, particles/transparency, twelve-direction
   pose motion, and performance. Invalid gameplay stereo is a visible reject,
   never a successful mono fallback.

## Safe Local Artifacts

- nonce-bound, tested no-launch StageOnly manifest under
  `local/openxr-retail-sidecar-runs/20260718-085015-525/`;
- independent no-copy/no-launch ValidateOnly manifest under
  `local/openxr-retail-sidecar-runs/20260718-085104-081/`;
- current build attestation at
  `build/fnvxr-retail-build-attestation-Release.json` (110 source inputs,
  ten artifact/config identities, 77 passed registered tests);
- exact installed/quarantined hash inventory at
  `local/installed-artifact-manifest.json`;
- dependency manifest at `deps/manifest.json`;
- retail probe at `local/fnv-probe.json`;
- combined preflight at `local/preflight-fnvxr.json`;
- run manifests and telemetry under ignored `local/` directories or the local
  retail game root;
- recoverable backups of the stale installed mutators under
  `local/installed-artifact-backups/20260718-050105-432/`. The live game-root
  FNVXR copies were replaced through a clean 77/77 combined Release
  build/test/stage.
  The installed compound-gated inert plugin is SHA-256
  `E394CB96DC1F881E66DF5E11877BCCBA55033FE50A4D407FD70CCB359BB3650D`.
  The default-on retail oracle and stale pre-fuse game-root OpenXR host were
  quarantined into that backup directory; neither remains in an auto-load or
  executable game-root location. All preceding plugin/proxy versions remain
  recoverable there.
