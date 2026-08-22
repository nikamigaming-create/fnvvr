# FNVVR full 6DoF integration and release plan

Audit date: 2026-08-22

This is the implementation plan for turning the current exact-retail visual
trial into a playable and releasable Fallout: New Vegas VR product: physical
6DoF head tracking, same-tick binocular world rendering, complete controller
input, working retail menus, tracked hands and weapons, authoritative firing,
and GPU-native presentation.

## Audited baseline

- FNVVR implementation baseline: `3121598799a6890968d2c345631523c47d2eee7a`
  (`codex/live-pipboy-hands`). The next product attestation supersedes this
  planning identifier and binds the complete working source/document set.
- Donor research checkout: `kote2345/fnv_vr` at
  `589b2d3fd98218ad233255e9649f903514613643`.
- Current catalogs contain 119 x64 and 121 Win32 registered tests. The previous
  August 22 build recorded 238/238 passes; the final-pixel gate raises the next
  clean catalog to 240 tests, and only a newly generated exact
  attestation can authorize later source or artifact changes.
- Current shared protocols include XInput v4, DirectInput v10, pose v9, origin
  v6, D3D9 stereo CPU v8, GPU frame v4, and GPU color v5. These versions must
  not be mixed or silently upgraded.
- The retained headless run
  `20260729-120130-129-4a961da02e23` proved deterministic six-axis simulator
  motion through engine stereo and OpenXR. It did not prove physical head/body
  behavior, controller mutation, tracked weapons, or authoritative firing.
- Three later `retail-vr-play-v1` runs did not reach physical acceptance:
  `20260729-173527-361-f05e9af73460` timed out waiting for advancing retail and
  pose state; `20260729-174503-337-6c2363c21efc` created the OpenXR session and
  retail CPU-v8 eye bridge but Fallout exited before acceptance; and
  `20260729-175041-419-cc7c18f76135` never obtained an OpenXR HMD system. All
  three retained `controllerMutationAuthorized=false` and
  `trackedWeaponAuthorized=false`.

This baseline is the truth source for planning. A simulator pass, a non-black
stereo image, or a visually moving weapon is not a completed product gate.

## Definition of done

FNVVR is complete only when one attested `retail-vr-play` run satisfies every
item below without a manual code/configuration change between tests.

1. `WorldStereo` renders both eyes from one simulation tick, one accepted
   predicted-pose lineage, distinct eye cameras, and a complete retail scene.
2. Physical yaw, pitch, roll, left/right, up/down, and forward/back motion are
   correctly signed. Leaning never orbits the world, moves the actor, or
   changes locomotion heading.
3. The player can move, turn, activate, crouch, jump, fire, aim/block, reload,
   holster, use VATS, use favorites, open the Pip-Boy, pause, and back out using
   tracked controllers with no duplicate input path.
4. The retail Pip-Boy remains a live binocular wrist device with tracked
   screen/dial interaction. Every other blocking retail UI is usable on a
   stable mono `UiQuad`, including pointer, hover, click, drag, scroll/zoom,
   accept, back, and controller navigation where that menu requires it.
5. Both hands and the visible first-person weapon are body-anchored and stable.
   Grip poses drive wrists; the independent aim pose drives the weapon/muzzle.
6. The visible barrel, retail muzzle, engine projectile or hit ray, spread,
   recoil, impact, ammunition, damage, and reload state agree through one
   logged transform lineage. Retail FNV remains authoritative.
7. Menus, loading, save changes, cell changes, death, fast travel, recenter,
   tracking loss, focus loss, device reset, and OpenXR session changes restore
   state and never leak a stale camera, hand, weapon, input, or world frame.
8. Eye and UI pixels use the selected production GPU transport. CPU readback
   is absent from the released gameplay route.
9. The supported runtime/mod inventory and the target headset profile pass the
   retained compatibility, performance, soak, and recovery matrices.
10. The final manifest alone may report `fullProductAccepted=true`. A
    `trialReady` or visual-trial result can never be relabeled as product
    acceptance.

## Non-negotiable product rules

- Retail `FalloutNV.exe` owns simulation, cells, collision, quests, saves,
  menus, animation, equipped items, ammunition, spread, recoil, projectiles,
  hit tests, damage, and rendered scene content.
- The OpenXR host owns predicted HMD/controller poses, action sampling, frame
  timing, composition, haptics requests, and non-authoritative diagnostics.
- Gameplay-critical camera, rig, weapon, muzzle, and input decisions are
  validated and consumed inside the retail process.
- Mono gameplay is never a successful fallback. Unknown or stale gameplay
  state produces a visible safety blank.
- The retail Pip-Boy is a persistent live wrist device in `WorldStereo`;
  opening or focusing it changes retail input ownership, not presentation
  mode. Every other blocking UI is deliberately flat retail UI. Do not rebuild
  Fallout menus in the host.
- Head look never continuously writes the player actor rotation. Body yaw
  changes only through deliberate snap turn, smooth turn, locomotion policy,
  or explicit recenter.
- Grip and aim poses are different contracts. Never substitute the grip
  orientation for a current aim pose when the weapon is authoritative.
- Do not call the complete retail world renderer twice. Do not reuse one
  accumulator for both eyes. Do not ship alternating-frame eyes.
- Do not enable the retained per-D3D-draw replay path as product stereo.
- Every mutation site must synchronously revalidate the current loaded retail
  image, required function bytes, object ancestry, compatibility inventory,
  producer epoch, pose/runtime lineage, and current product mode.
- Every temporary camera/rig/render write has one scope guard and restores on
  normal return, rejection, exception, load, UI transition, device loss, and
  shutdown.

## Shipping stereo decision - locked

There is exactly one accepted gameplay stereo schedule:

```text
one retail simulation tick
  -> one accepted predicted OpenXR pose/runtime sample
  -> one conservative union visibility traversal
  -> one fresh isolated left-eye render
  -> one fresh isolated right-eye render
  -> restore all retail state
  -> validate and atomically publish the pair
```

The two eyes must have the same source-frame identity, runtime-state sample,
simulation/cell epoch, predicted display time lineage, pose sequence, visible
array identity, and transaction ID. Both must be completed before either is
published. If either eye fails, both are discarded.

No task in this plan ports or enables AFR, alternating-eye rendering,
frame-parity eye selection, a cached opposite eye, or a one-game-frame
inter-eye offset. OpenXR compositor reprojection is allowed only as the
runtime's normal final presentation behavior; it is never evidence that a
stale or mismatched retail eye pair is valid.

## Reference material: adopt, adapt, or reject

The references are research inputs, not production authority.

| Source | Adopt or adapt | Explicitly reject as product proof |
|---|---|---|
| `kote2345/fnv_vr` camera research | The two-camera distinction, verified retail seams, direct quaternion-to-`NiCamera` evidence, culler/frustum observations, and address candidates to revalidate against FNVVR's exact-runtime manifest | Continuous HMD-to-actor rotation, broad mouse-look patching, fixed addresses without FNVVR revalidation, and the claim that apparent stability proves physical 6DoF |
| Donor `RenderFirstPerson`/weapon research | Unique `Weapon` discovery, save/apply/render/restore transaction, action-state gates, root-change invalidation, and the post-animation seam as a candidate for persistent IK | The current F11 relative-delta calibration as product calibration, grip-as-aim behavior, visual motion as muzzle proof, and any unrestored or gameplay-visible render-only transform |
| Donor native stereo report | One conservative visible array, fresh eye accumulators, bind-before-populate ordering, and the two stock accumulation branches | Calling the stock world renderer twice or accepting only one stock branch |
| Donor DXVK transport | A bounded D3D11-first/DXVK import feasibility spike and the 16:9 UI aspect-fit observations | Making DXVK mandatory without a supported-runtime decision, accepting CPU fallback, or treating a color bridge as proof of the complete FNVVR transaction |
| `Making-6DOF-Mods-3D-A-General-Method-Rev4-AFR.pdf` | Physical-unit stereo math, explicit eye ownership, stale-eye diagnostics, main-camera discrimination, and independent layer testing | AFR/alternating-eye stereo, one-frame inter-eye age, screen-space convergence shifts, or Present-parity eye inference in the shipping OpenXR path |
| `6DOF-HeadTracking-Master-Reference-Revs6.pdf` | Clean-plus-head idempotence, in-thread/in-frame writes, final-render-camera validation, drive-model tests, per-camera isolation, wobble/cardinal tests, and failure-catalog diagnostics | Treating generic OpenTrack/ASI/AOB guidance as FNV evidence, replacing exact retail hashes with signatures, or applying its generic hook prescriptions without engine-specific proof |

The donor checkout contains no `LICENSE`, `COPYING`, or `NOTICE` file at the
audited commit. Until permission is established, do not copy donor source.
Reimplement verified behavior independently, cite research provenance, and do
not commit decoded memory images or generated donor build artifacts.

## Product architecture

```text
OpenXR host
  - samples predicted HMD, eye, grip, aim, actions and reference-space state
  - publishes versioned intent with producer/process/epoch lineage
  - consumes only complete retail GPU/UI transactions
  - submits WorldStereo or UiQuad
             |
             v
xNVSE plugin inside retail FalloutNV.exe
  - exact loaded-image and compatibility authority
  - runtime/menu classifier and controller-mode state machine
  - body-local head/controller transforms
  - arm/hand/weapon application and authoritative combat hooks
  - retail telemetry and restoration
             |
             v
Retail engine stereo backend and D3D9 bridge
  - one conservative visibility traversal
  - isolated left/right cameras, accumulators, color and depth targets
  - first-person/world/auxiliary pass coverage
  - GPU publication with completion and ownership handoff
```

### Canonical coordinate chain

The codebase gets one transform implementation and one set of names. Hooks may
consume it; they may not recreate axis swaps locally.

- OpenXR local axes: right, up, back (`-Z` is forward).
- Retail `NiCamera` local axes: forward, up, right. The render-camera basis
  conversion is `(x, y, z) -> (-z, y, x)` for both local vectors and the
  quaternion vector part; it is a proper rotation, not an actor transform.
- Actor/Gamebryo vector mapping: `(x, y, z) -> (x, -z, y)`.
- `metersToGameUnits` is a measured calibration value. The current `70` units
  per meter is an initial approximation, not permanent truth.

For an OpenXR pose `T_pose`, recentered origin `T_origin`, retail body/camera
anchor `T_anchor`, and scale `S`:

```text
T_local = inverse(T_origin) * T_pose
T_camera = T_anchor * ni_camera_basis(T_local, S)
```

The center head displacement is mapped once. Left and right eyes come from the
same frame's `XrView` poses; fixed IPD is diagnostic fallback only. Do not apply
head translation once to the center and again to each eye, and do not rotate
room translation by the current head orientation.

Controller transforms use the same origin/body anchor:

```text
T_wrist_game  = T_body * inverse(T_origin) * T_grip_xr * C_grip_to_wrist
T_weapon_game = T_body * inverse(T_origin) * T_aim_xr  * C_aim_to_weapon
T_muzzle_game = T_weapon_game * T_weapon_to_muzzle
```

`C_grip_to_wrist` and `C_aim_to_weapon` are explicit calibration records, not
captured accidental poses. Each record is versioned by controller interaction
profile, handedness, weapon/form or weapon class, model/node identity, scale,
and calibration revision.

### Product state machine

| State | Presentation | Input | Camera/rig/weapon behavior |
|---|---|---|---|
| Startup or loading | Latest current `UiQuad`, otherwise safety blank | Neutral except an explicitly valid retail UI action | No camera, rig, weapon, or firing write |
| Interactive retail UI | Current `UiQuad` | Exactly one UI route: ray/pointer or menu navigation | Suspend world props; restore all temporary world state |
| Live Pip-Boy focus | Fresh `WorldStereo` with the wrist device | Tracked fingertip/screen/dial actions through retail | Keep authenticated hands, weapon, Pip-Boy root, and both eyes current |
| UI-to-world handoff | Hold last valid quad for the bounded window, then blank | Neutral until the new mode is authoritative | Wait for a strictly newer complete stereo transaction |
| Gameplay ready | `WorldStereo` | Gameplay action map | Head-local cameras, hands, weapon, and retail combat authority active |
| Tracking stale/lost | Safety blank or current blocking UI quad | Release every held action within 250 ms | Restore/suspend camera, rig, weapon and firing writes |
| Runtime/device/reference-space change | Safety blank | Neutral | Invalidate resources, origins, calibrations and pose lineage; reacquire cleanly |

## Capability and fuse strategy

Do not use one broad source fuse as a development switch. Build narrow,
auditable capability leases and aggregate them only for release.

1. `CameraStereoVisual`: may apply accepted eye transforms and publish an
   isolated pair; no input or weapon authority.
2. `ControllerInput`: may consume semantic actions on the game thread; no rig
   or combat transform authority.
3. `UiInteraction`: may route pointer/navigation only while the classifier is
   authoritatively `Ui`.
4. `VisualRig`: may write and restore first-person hand/weapon transforms; no
   projectile/hit mutation.
5. `CombatAim`: may feed the smallest proven retail firing seam while retail
   retains spread/recoil/projectile/damage authority.
6. `GpuPresentation`: may publish/consume only complete GPU transactions.
7. `FullProduct`: requires all of the above plus compatibility, performance,
   recovery, and retained-run evidence.

`RetailMutationProofComplete`, `CompiledProductionRendererProof`,
`StereoWorldProductionProofComplete`, and
`ProductWorldStereoIntegrationComplete` remain false until their final gates
pass. The DirectInput/XInput proxy production fuses should remain false if the
direct, exact-retail xNVSE game-thread consumer is selected as the product
input route; do not activate two controller consumers.

## Phase 0 - Reconcile contracts and lock the baseline

### Work

- Update the status/architecture records to distinguish the new
  `retail-vr-play-v1` physical route from the bounded visual-trial route.
- Create one checked status matrix for every capability: implemented,
  unit-tested, simulator-tested, physically tested, production-authorized.
- Resolve the current GPU contract conflict before implementation:
  `runtime/fnvxr_retail_safety.h` and `docs/architecture-v2.md` require GPU
  color and depth, while GPU color ABI v5 deliberately keeps depth render-local.
  Record one ADR choosing either:
  - color v5 for product v1 with a formal decision that OpenXR depth submission
    is not a release gate; or
  - a new complete color/depth ABI and OpenXR depth-layer gate.
- Select one production input owner. The recommended owner is the xNVSE
  main-game-loop consumer already used by physical play; keep both input proxy
  mutators transparent.
- Inventory every camera, culler, accumulator, first-person, rig, projectile,
  hit-ray, menu, and input mutation site plus its restore path and exact hash.
- Record donor facts as candidates in FNVVR-owned manifest/tests. Do not import
  donor code or binary data.
- Freeze one known retail and one TTW fixture, representative saves, installed
  mod hashes, physical runtime manifest, headset firmware/runtime version, and
  controller interaction profile.
- Run the complete x64 and Win32 build/test/attestation baseline and retain its
  hashes before changing behavior.

### Exit gate

One reviewed architecture decision, one capability matrix, one mutation-site
inventory, one reproducible fixture set, and a clean full test/attestation
baseline. No new product mutation is enabled in this phase.

## Phase 1 - Stabilize the physical 6DoF vertical slice

### Work

- Reproduce the most advanced physical run with Link/Air Link active and keep
  the HMD awake until `XR_SESSION_STATE_READY`; separate runtime-unavailable,
  game-exit, and bridge-readiness failures in the manifest.
- Trace one physical pose end-to-end: host sample, producer epoch, recentered
  origin, body/camera anchor, center eye transform, left/right cameras, render
  transaction, CPU-v8 publication, and OpenXR submission.
- Make camera application clean-plus-head and idempotent. Re-read the engine's
  clean camera basis each accepted frame; never accumulate the previous VR
  result.
- Prove the actual renderer camera and culler camera receive the same accepted
  orientation before traversal. Keep auxiliary/shadow/reflection cameras out
  of the head transform.
- Apply room translation in the recentered, gravity-aligned body frame. Add
  explicit signed-axis telemetry and double-application detection.
- Define standing/seated origin, player height, world scale, recenter, snap
  turn, smooth turn, and physical crouch behavior. Body yaw changes only for a
  deliberate turn/recenter event.
- On pose age, reference-space generation, producer epoch, tracking validity,
  load, menu, cell, third-person, death, or camera identity changes, restore
  and reacquire instead of reusing state.
- Add a desktop wobble/cardinal fixture plus a physical cardinal script:
  +/-100 mm on X/Y/Z and +/-15 degrees yaw/pitch/roll, one axis at a time.

### Exit gate

A retained physical run proves all six signed axes, clean recenter, no orbital
pivot, no player/body drift, stable movement heading, no stale transform after
UI/load/cell transitions, and exact pose-to-eye lineage. Simulator evidence is
required but cannot satisfy this gate.

## Phase 2 - Complete same-tick stereo and first-person rendering

### Work

- Keep the current schedule: snapshot, one conservative union visibility
  traversal, fresh left accumulator, fresh right accumulator, restoration,
  atomic publication.
- Cover and prove both retail accumulation branches. Do not force an unknown
  selector merely to make one branch pass.
- Bind each eye camera, color, depth, viewport, auxiliary targets, and
  accumulator before `AddVisibleArray`, because population may render
  immediately.
- Prove center/center parity first, then distinct cameras with identical
  simulation tick and conservative visible-set identity.
- Audit the separate `RenderFirstPerson` transaction. Determine exactly where
  arms, weapon, muzzle effects, shell casings, crosshair, particles, and HUD
  are drawn relative to the world hook.
- Render first-person geometry once per eye with the same eye camera, target,
  pose, and transaction identity as its world eye. A mono weapon pasted over
  stereo world is a hard failure.
- Keep gameplay HUD out of `WorldStereo`; only explicitly approved diegetic
  world geometry may remain.
- Cover exterior, interior, exterior/interior without an observed loading bit,
  water, sky, transparency, particles, image-space effects, NPCs, decals,
  scope effects, and first/third-person transitions.
- Validate restoration of all cameras, culler fields, visible arrays,
  accumulators, target groups, depth, viewports, shaders, globals, first-person
  transforms, and reference counts on every exit path.

### Exit gate

At least 10 minutes of retained same-tick binocular play crosses the complete
scene matrix with no one-eye target, structural divergence, stale pose, mono
first-person layer, HUD contamination, resource aliasing, state leak, or
crash. Both stock branches have independent retained evidence.

## Phase 3 - Finish semantic controller input and locomotion

### Work

- Replace environment-variable combinations as the product mapping contract
  with a versioned semantic action map. Environment flags may remain for labs,
  not for required release controls.
- Bind OpenXR actions for move, turn, primary fire, aim/block, activate/grab,
  jump, crouch, reload, holster, VATS, Pip-Boy, pause, accept, back, favorites,
  stick clicks, and menu scroll/zoom. Add a haptic output action.
- Support Touch/Touch Plus first, then validate other interaction profiles
  without assuming identical button availability.
- Keep the xNVSE main-game-loop consumer as the single retail input owner.
  Remove or hard-disable overlapping OS `SendInput`, XInput proxy, DirectInput
  proxy, keyboard fallback, and mouse fallback lanes once the semantic route
  is accepted.
- Retain the current mode classifier, consumer acknowledgement, producer epoch,
  release-before-press transition, and 250 ms stale neutralization rules.
- Make movement body-relative, not head-relative. Provide snap and smooth turn,
  adjustable deadzones/curves, left-handed bindings, seated mode, and a comfort
  vignette option without changing retail simulation.
- Ensure firing and activation are impossible while the same trigger is owned
  by a menu click. A mode transition must release first and require a fresh
  press.
- Route favorites and weapon selection through retail controls/state. Do not
  maintain a second host-side inventory or weapon wheel.

### Required gameplay action test

Walk/run, strafe, turn, jump, crouch, activate doors/containers/NPCs, draw and
holster, fire, aim/block, reload, use VATS, change favorites, open/close the
Pip-Boy, pause/resume, and recover from focus/tracking loss. Each action must
produce one retail event and one release, never zero or two.

### Exit gate

Thirty minutes of physical play completes the action test with one input
owner, no stuck/duplicated action, no head-driven locomotion yaw, correct
UI/gameplay ownership, and neutralization within 250 ms after the host, HMD,
or tracking producer stops.

## Phase 4 - Make every retail menu usable

### Work

- Preserve runtime classification as authority. Pixel heuristics may reject a
  bad image but may not classify gameplay as UI.
- Expand the classifier/acceptance matrix to cover:
  - startup, main, pause, settings, save/load, message boxes and DLC notices;
  - live-wrist Pip-Boy Items/Stats/Data/Map, including screen, tab-dial,
    scroll/zoom, focus, and weapon-orbit interaction;
  - inventory, container, barter, repair, crafting, companion and
    quantity/slider submenus on `UiQuad`;
  - dialogue, VATS, terminals/hacking, lockpicking, wait/sleep, level-up,
    perks, race/character creation, death, credits and loading;
  - console and mod-added TileMenus, which default to `UiQuad` until validated.
- For non-Pip-Boy UI, use one current 16:9 retail texture, aspect-fit without
  cropping, and bind its runtime-state sample, source frame, pose epoch,
  capture ordinal, and pixel-completeness evidence.
- Make the controller ray intersect either the exact displayed quad or the
  authenticated wrist screen/control plane selected by the classifier. Keep
  render and input dimensions explicit so DPI, window size, backbuffer size,
  client coordinates, and wrist-local coordinates cannot drift.
- Complete pointer hover, press/release, drag, sliders, scroll wheel, map zoom,
  accept, cancel/back, key repeat, D-pad/stick navigation, and pointer capture
  loss. Never feed ray and gamepad navigation simultaneously unless the menu
  has a tested split contract.
- Treat unknown or stale runtime/UI evidence as a safety blank with neutral
  input. Never manufacture a quad or expose stale world stereo from an
  unclassified interactive state.
- On non-Pip-Boy UI entry, suspend/restore hands, weapon and combat input before
  presenting the quad. Pip-Boy focus instead retains authenticated world props
  and changes only retail input focus. On quad exit, retain the quad until a
  strictly newer complete stereo transaction is ready, then switch atomically.
- Add an automated menu traversal fixture where possible and a retained manual
  checklist for menus that require quest/game context.

### Exit gate

The live wrist Pip-Boy and complete non-Pip-Boy menu matrix pass open,
interact, nested interact, back, close, and world-handoff tests with no black
frame, cropped menu, lost pointer, double click, stuck key, accidental stereo
menu, gameplay fire, stale world frame, or unrecoverable focus loss.

## Phase 5 - Body-anchored hands and visual weapon chain

### Work

- Start from the existing first-person rig discovery, ancestry checks, FABRIK
  solver, independent grip/aim publication, and post-animation hook.
- Prove unique left/right clavicle, upper arm, forearm, hand, `Weapon`,
  projectile/muzzle endpoint, root, `AnimData`, equipped form, and weapon-class
  identities in one frame before applying a write.
- Latch the body/first-person anchor from the exact accepted camera origin.
  Controller positions remain body-origin-relative and never inherit head
  translation twice.
- Drive wrists from grip poses. Solve shoulders/elbows within anatomical limits
  and preserve engine animation, scale, handedness, and world bounds.
- Drive weapon orientation and position from the right aim pose plus an
  explicit calibration. Add left-hand support points for two-handed weapons
  without letting the support hand redefine the muzzle.
- Use a donor-style `RenderFirstPerson` save/apply/render/restore wrapper only
  as a narrow visual diagnostic until its complete helper behavior and
  coexistence with FNVVR's per-eye first-person transaction are proven.
- Decide the production application seam from evidence:
  - post-animation for persistent two-hand IK and animation composition;
  - the narrow per-eye first-person render seam for eye-specific parallax;
  - never one unscoped transform that leaks into gameplay or another eye.
- Reset node caches and calibration on equip/unequip, weapon form/model change,
  root/`AnimData` change, reload/equip actions, first/third person, load, menu,
  cell, recenter, scale, tracking, and reference-space changes.
- Preserve recoil, reload, pump/bolt, magazine, melee, idle and equip animation.
  Controller pose supplies the base aim transform; retail animation supplies
  authored relative motion.
- Add per-class calibration records and a visible calibration workflow. Never
  make the pose at the instant a user presses a key the undocumented permanent
  baseline.

### Visual weapon coverage

| Class | Required visual proof |
|---|---|
| Pistol/revolver | One-hand aim, recoil, reload, holster, iron sight |
| Rifle/shotgun/automatic | Two-hand support, recoil, magazine/pump/bolt, movement |
| Scoped firearm | Stable scope/weapon geometry with correct per-eye policy |
| Energy/beam | Weapon and muzzle-effect alignment |
| Launcher/projectile | Weapon root and projectile endpoint alignment |
| Thrown explosive/mine | Hand pose, release point and restored animation |
| One/two-handed melee | Grip, swing arc, impact timing and no firearm assumptions |
| Unarmed | Both hands, guard/attack and no required `Weapon` node |
| Mod-added/unknown | Validated generic node chain and calibration; otherwise an explicit unsupported-state reject, never silently wrong aim |

### Exit gate

Both hands and all required visual classes remain stable through movement,
turn, lean, recenter, recoil, reload, equip, menus, loading and tracking loss.
The visible first-person layer is correctly binocular and every rejected write
restores the exact retail transform.

## Phase 6 - Make weapon aim authoritative

### Work

- Extend the existing read-only `BaseProcess::GetProjectileNode` observation
  into a proof tool first. Log controller aim, weapon transform, endpoint,
  muzzle forward, engine pre-spread direction, final projectile/hit direction,
  impact, pose sequence, runtime sample and source frame in one coordinate
  space.
- Identify separate retail seams for hitscan, physical projectile, continuous
  beam, multi-pellet, thrown explosive, melee/unarmed and VATS behavior. Do not
  assume one projectile-node hook covers every weapon.
- Feed controller/muzzle intent at the smallest in-process point before retail
  applies the relevant shot calculation. Preserve retail ammunition, condition,
  animation, recoil, spread, pellet randomization, projectile creation, hit
  tests, damage and scripting.
- Distinguish intended aim from post-spread result. The visible barrel must
  follow intended aim; the final logged ray may differ only by the retail
  spread/recoil value belonging to the same shot.
- Make fire require current aim tracking, current gameplay mode, current
  weapon/endpoint ancestry and an accepted pose/runtime lineage. Never fall
  back to head aim or a stale controller pose.
- Keep reload/equip/holster and weapon-condition behavior authoritative. Do not
  teleport the muzzle while authored sequences temporarily own the weapon.
- Add haptics only after a confirmed retail shot/reload/melee event. The host
  does not generate gameplay from a haptic request.
- Test scopes, silencers, alternate ammo, shotguns, automatic fire, launchers,
  grenades/mines, energy beams, melee, unarmed, VATS, companion/dialogue
  interruption, and mod-added weapons.

### Quantitative acceptance

- With intentional spread disabled or separately accounted for, median muzzle
  angular residual is at most 0.5 degrees and p95 is at most 1.0 degree.
- Muzzle origin residual is at most 2 cm in calibrated game scale.
- Near, medium and long test targets agree between controller intent, visible
  sight/barrel, engine ray/projectile and impact within the tolerance implied
  by those angular/origin limits.
- Every shot identity maps to one trigger edge/hold policy, one ammunition
  event and the retail-authored number of projectiles/pellets.
- Recoil and reload return to the calibrated chain without a snap, drift or
  permanently modified node.

### Exit gate

Every weapon class passes visual and authoritative tests in retained physical
runs. The manifest can set `trackedWeaponAuthorized=true`; visual-only hand or
weapon motion cannot set it.

## Phase 7 - Replace CPU-v8 with the production GPU route

### Work

- Implement the Phase 0 transport ADR, not both competing contracts in the
  same route.
- Build a live producer backend behind the existing transport abstractions:
  same-adapter identity, non-aliased left/right resources, GPU-only copies,
  shared completion fence, consumer-release ownership, producer process/epoch,
  resource-set generation, device-reset handling, and no CPU pixel transfer.
- If the standard retail route can use D3D9Ex sharing, prove D3D9Ex creation,
  D3D11 open/copy, NT/legacy handle compatibility, and completion semantics on
  the actual device and runtime.
- If native sharing cannot satisfy the contract, run a separately bounded
  D3D11-first DXVK import spike based on the donor observation. Make DXVK a
  supported backend only after compatibility, performance, UI and device-loss
  gates pass; never silently switch backends or fall back to CPU.
- Use a resource ring sized from measured frames in flight. The producer must
  never overwrite a resource until the consumer's release fence is observed.
- Publish `BinocularWorld` and `MonoUiQuad` through the same selected ownership
  model while retaining distinct mode/source lineage. A UI texture cannot be
  relabeled as world stereo or vice versa.
- Copy accepted eye resources into the acquired OpenXR images entirely on the
  GPU. If depth is selected by the ADR, encode, transport, validate and submit
  the matching per-eye depth with correct near/far and pose identity.
- Recover cleanly from resize, alt-tab, fullscreen/window changes, D3D device
  loss/reset, OpenXR swapchain recreation, adapter mismatch, producer exit and
  consumer restart.
- Remove CPU-v8 from the product profile and keep it only in the explicitly
  bounded visual-trial profile.

### Performance gate

Select and record an initial supported headset mode, recommended 72 Hz for the
first Quest Link/Air Link release. Measure retail update, visibility, both eye
passes, GPU copy, host wait/copy, compositor submit, missed frames, pose age and
memory. The release profile must sustain its selected refresh target in the
scene matrix with no unbounded CPU wait, no resource overwrite, no CPU
readback, stable memory over a two-hour soak, and a documented p95/p99 budget.
If the target is missed, optimize or lower the declared supported render scale;
do not call asynchronous reprojection an unmeasured success.

### Exit gate

GPU-only world and UI transactions pass adapter/fence/ownership tests, device
recovery, the complete scene/menu matrices, physical latency review and the
two-hour soak. CPU-v8 is absent from `retail-vr-play`.

## Phase 8 - Compatibility, comfort and release qualification

### Work

- Complete the live module census and clean loaded-page reference for every
  supported code-mutating module. Keep arbitrary/unrecognized patches a hard
  reject.
- Retain exact compatibility rules for xNVSE, JIP LN, JohnnyGuitar, ShowOff and
  the owned TTW profile. Add other mods only from exact hashes plus scoped
  normalization evidence.
- Test new game, saved game, exterior/interior, fast travel, doors without a
  loading bit, death/reload, sleep/wait, VATS, dialogue, terminals, cutscenes,
  scripted camera changes, forced third person, alt-tab, focus loss and clean
  shutdown.
- Finalize standing/seated height, world scale, floor offset, snap/smooth turn,
  dominant hand, turn speed, deadzones, render scale, UI distance/size,
  calibration reset and comfort defaults.
- Provide first-run validation with actionable errors for runtime, headset,
  adapter, game version, xNVSE, mods, source resolution, controller profile and
  save/fixture readiness.
- Make staging/restoration transactional and recoverable. Never leave stale
  proxy/plugin/config files in the game root after a failed run.
- Generate one retained release bundle: source/build hashes, tests, exact
  installed artifacts, runtime/mod census, physical run manifest, frame timing,
  camera/cardinal proof, menu matrix, weapon matrix, recovery/soak result and
  headset mirror evidence.

### Exit gate

The guarded release run passes every phase gate and independently validates its
installed artifacts without copying or launching. Only then may the aggregate
full-product fuse and `fullProductAccepted` result change.

## Implementation ownership by area

| Area | Primary files | Required outcome |
|---|---|---|
| Shared lineage and capability records | `protocol/fnvxr_shared_state.h`, `protocol/fnvxr_product_contract.h`, GPU transport headers | One producer, stable snapshots, explicit epochs/samples/modes, no ABI ambiguity |
| Exact runtime/engine authority | `runtime/fnvxr_retail_*`, engine manifest/ABI/calls/compatibility files | Current-process proof at every mutation decision and both world branches |
| Camera and rig | `runtime/fnvxr_retail_eye_camera_transaction.h`, `plugin/fnvxr_nvse_plugin.cpp` | Canonical transforms, body decoupling, hands, calibration, restore guards |
| Stereo and GPU producer | `renderhook/fnvxr_d3d9_proxy.cpp`, retail bridge/eye target/GPU publisher files | Complete world and first-person eyes plus GPU-only publication |
| OpenXR/input/composition | `host/fnvxr_openxr_pose_host.cpp`, GPU consumer/route and UI gate files | Semantic actions, one input owner, UiQuad/WorldStereo composition, haptics |
| Launch/evidence | `scripts/fnvxr-product-common.ps1`, `scripts/start-fnvxr-product.ps1`, verification/attestation scripts | Narrow profiles, transactional staging, retained phase-specific evidence |
| Tests | `tests/` plus simulator/fixture tools | Unit, adversarial, headless, physical, menu, weapon, performance and recovery gates |

Large implementations should split the current monolithic plugin and host by
domain before adding more hooks: camera/origin, controller input, menu
authority, rig/IK, weapon/combat proof, OpenXR actions, and presentation. Keep
shared policy in small testable headers and Win32/engine calls behind adapters.

## Required retained evidence matrix

| Requirement | Unit/adversarial | Headless simulator | Physical retail | Release blocker |
|---|---:|---:|---:|---:|
| Exact runtime and compatibility authority | Yes | Yes | Yes | Yes |
| Six signed camera axes and recenter | Yes | Yes | Yes | Yes |
| Body/head decoupling and locomotion heading | Yes | Helpful | Yes | Yes |
| Same-tick stereo and both stock branches | Yes | Yes | Yes | Yes |
| First-person binocular arms/weapon | Yes | Yes | Yes | Yes |
| Complete controller action map | Yes | Yes | Yes | Yes |
| Complete retail menu matrix | Yes | Partial | Yes | Yes |
| Visual weapon class matrix | Yes | Partial | Yes | Yes |
| Authoritative muzzle/projectile/hit chain | Yes | Partial | Yes | Yes |
| GPU transport/fence/device recovery | Yes | Yes | Yes | Yes |
| Performance and two-hour soak | No | Helpful | Yes | Yes |
| Install/restore and independent validation | Yes | Yes | Yes | Yes |

Every retained dynamic result must bind run ID, source/build hashes, installed
artifact hashes, runtime/mod identities, host/game process IDs, producer epochs,
runtime samples, pose sequences, transaction IDs, resource-set IDs, source
frames, predicted/rendered display time and final OpenXR eye identities.

## Risk register

| Risk | Detection | Required response |
|---|---|---|
| World appears to orbit during lean | Signed cardinal trace shows translation rotated by head pose or applied twice | Reject transaction; fix the canonical origin/body mapping before any other feature work |
| First-person weapon is mono or at wrong depth | Eye hashes/geometry differ only in world pass; weapon lacks eye transaction identity | Integrate the separate first-person pass per eye; do not hide it with a host overlay |
| First traversal consumes scene state | Eye structural counts diverge or second eye misses targets | One union visible set plus isolated accumulators; expand the state snapshot/restore ledger |
| Input fires while clicking UI | Trigger edge appears in both UI and gameplay consumers | Single controller-mode owner, release-before-press, and fresh-edge requirement |
| Visual barrel disagrees with damage | Muzzle/ray/impact residual exceeds tolerance | Keep combat authority closed; locate the actual weapon-class firing seam |
| Tracking loss leaves a stuck control or weapon | Producer heartbeat stops but retail state remains held/mutated | Neutralize within 250 ms and restore camera/rig/weapon state on the game thread |
| Compatibility mod rewrites a protected seam | Current bytes/module inventory differ from normalized rule | Refuse mutation and record the exact mismatch; never guess around it |
| CPU or GPU transport stalls | Pose age, fence wait, frame time or resource ownership regresses | Reject frame, expand ring/repair synchronization, and keep CPU fallback out of product |
| UI is cropped or pointer is offset | Captured aspect/client coordinates differ from displayed quad | Bind source/input/display dimensions and test corners, drag and DPI/focus changes |
| Device/session change revives stale resources | Old resource set or pose epoch is accepted after reset | Invalidate all resources/origins/calibrations and require a new complete lineage |

## Immediate implementation queue

1. Land the Phase 0 status matrix, transport ADR, input-owner decision, and
   capability leases.
2. Re-run `retail-vr-play-v1` with an active physical runtime and instrument
   the exact reason for any game exit before controller acknowledgement.
3. Retain the physical six-axis/head-body/recenter proof on the existing
   CPU-v8 route.
4. Prove both world branches and the separate first-person render transaction.
5. Freeze and test the semantic gameplay input map and complete UI matrix.
6. Enable a narrow visual-rig diagnostic: hands plus one pistol, with complete
   save/apply/restore and binocular first-person evidence.
7. Add read-only muzzle/hit telemetry, then authorize one pistol firing seam
   only after its transform lineage passes.
8. Expand authoritative combat class by class; do not use one successful
   pistol as proof for projectiles, beams, melee, thrown weapons or VATS.
9. Integrate the selected GPU producer and remove CPU-v8 from physical play.
10. Run compatibility, recovery, performance and soak qualification; aggregate
    the full-product fuse only after all retained evidence passes.

This order creates a truthful playable vertical slice early while preserving
the hard distinction between visual progress and product authority. The donor
and reference PDFs help shorten investigation, but FNVVR's exact-runtime,
same-tick, retail-authoritative and fail-closed contracts remain the release
standard.
