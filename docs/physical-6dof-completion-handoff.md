# Physical 6DOF Completion Handoff

## Objective

Finish the working physical Fallout New Vegas VR path without replacing its architecture.

The current implementation already produces binocular engine-rendered stereo with independent HMD 6DOF. Preserve that result. Do not replace it with a test harness, projected game plane, mono fallback, generated imagery, or a separate demonstration application.

Work directly in the real game and physical headset path.

## Repository and evidence

- Repository: `D:\code\fnvvr`
- Latest merged baseline: `efed09c` on `origin/main`
- Reference video: `C:\Users\nbrys\Downloads\Recording 2026-08-02 052358.mp4`
- Evidence run: `D:\code\fnvvr\local\product-runs\20260802-052023-844-75e538c6d72f`
- Important run files:
  - `fnvxr_input_telemetry.log`
  - `fnvxr_retail_vr.log`
  - `host.stdout.log`
  - `manifest.json`

Start from the merged baseline:

```powershell
Set-Location D:\code\fnvvr
git fetch origin
git switch main
git pull --ff-only origin main
git switch -c codex/finish-physical-6dof
git status -sb
```

## What already works

The reference recording proves:

- Both eyes render the real engine world.
- The old duplicate left-eye first-person weapon pass is gone.
- The world remains spatially stationary during HMD translation and rotation.
- The gun is visible binocularly and is not simply attached to the face.
- Rendered world geometry has real stereo depth and normal occlusion.
- Center-integrated first-person publication is active with `privateEyeCalls:0`.

Do not regress these properties while fixing the remaining problems.

## Required order of work

Complete these phases in order:

1. Restore player locomotion.
2. Drive the weapon continuously from the right controller.
3. Complete 360-degree world visibility.
4. Tune close-range weapon comfort.

Build and validate after every phase. Do not combine all three primary fixes before testing their individual authority paths.

## Phase 1: Restore locomotion

### Evidence

OpenXR and the plugin receive real left-stick input. One recorded shared-state sample was:

```text
ls=23573,22758
```

The runtime simultaneously reports:

```text
runtimeGameplay=1
controllerMode=gameplay
gameplayControls=0
```

The player remains stationary. This is an input-authority or final-consumption problem, not a controller pairing problem.

### Intended input path

```text
OpenXR thumbstick action
→ PoseFrame leftThumbstickX/Y
→ shared virtual Xbox state
→ NVSE external input consumer
→ sustained DirectInput movement keys or native analog state
→ Fallout player movement
```

### Code to inspect

Start with these symbols:

- `consumeExternalXInputGameplayControls`
- `updateControllerAxes`
- `pluginKeyboardMovementEnabled`
- `pluginGameplayKeyboardFallbackEnabled`
- `gameplayControlsActive`
- `holdDirectInputKey`
- shared `leftThumbX` and `leftThumbY` publication

Relevant files:

- `host/fnvxr_openxr_pose_host.cpp`
- `plugin/fnvxr_nvse_plugin.cpp`
- `renderhook/fnvxr_dinput8_proxy.cpp`
- `renderhook/fnvxr_xinput_proxy.cpp`
- `renderhook/fnvxr_input_proxy_safety.h`
- `scripts/fnvxr-product-common.ps1`

### Implementation method

1. Add transition-based telemetry at every input authority boundary. Avoid per-frame log spam.
2. Put the raw OpenXR axis, shared axis, gameplay classification, generated movement state, and final proxy consumption under a common sample lineage.
3. Determine whether `gameplayControls=0` is merely incorrect host reporting or the actual suppressing gate.
4. Authorize physical gameplay input when all of the following are true:
   - physical headset play is requested;
   - runtime state is gameplay;
   - no menu owns input;
   - the controller consumer is acknowledged.
5. Keep movement suppressed while a menu owns input.
6. If native analog XInput remains unreliable in Fallout, use the existing sustained DirectInput W/A/S/D path. Do not generate repeated key taps.
7. Release every held movement key on focus loss, menu transition, runtime loss, process shutdown, or authority loss.

### Acceptance criteria

- Holding the left stick forward continuously moves the real player.
- Releasing the stick stops movement cleanly.
- Backward and lateral movement work.
- Opening a menu immediately releases movement.
- Closing the menu restores gameplay authority without a stuck key.
- Telemetry proves nonzero input at the final game-consumption boundary.

## Phase 2: Drive the weapon from the right controller

### Evidence

The right controller is tracked independently of the HMD, but weapon application is gated by an intermittent full-hand solve.

Run totals:

```text
rightSolved=true: 198
rightSolved=false: 435
weaponWriteApplied=true: 198
weaponWriteApplied=false: 435
```

Failure frames commonly report:

```text
rightSolved=false
weaponAligned=false
weaponWriteApplied=false
handTargetErrorUnits≈27
```

This explains the recording: the controller moves, but the gun remains at the last accepted transform.

### Required authority split

The weapon must not depend on successful arm IK.

```text
Right OpenXR aim pose
→ body/stage-local transform
→ calibrated weapon-root transform
→ apply before stereo eye rendering

Optional arm IK
→ follows the weapon target when solvable
→ never owns or gates weapon authority
```

### Code to inspect

- `plugin/fnvxr_nvse_plugin.cpp`
- `runtime/fnvxr_retail_center_renderer_operations.h`
- `runtime/fnvxr_retail_center_runtime.h`
- `renderhook/fnvxr_retail_vr_bridge_win32.h`
- `renderhook/fnvxr_d3d9_proxy.cpp`

### Implementation method

1. Preserve the body/stage anchor already used by the working head-independent path.
2. During a valid calibration frame, calculate:

```text
controllerToWeapon = inverse(rightAimWorld) × weaponWorld
```

3. For every current tracked frame, calculate:

```text
desiredWeaponWorld = currentRightAimWorld × controllerToWeapon
```

4. Apply `desiredWeaponWorld` directly to the first-person weapon root before the center-integrated stereo pair is rendered and published.
5. Update the required NiNode transforms and bounds immediately.
6. Use the same pose sequence and exact weapon transform for both eyes.
7. Keep arm and hand IK downstream and optional.
8. If controller tracking is briefly invalid, retain the last valid weapon pose for a short bounded interval. Never substitute an HMD-relative transform.
9. Keep `headTermInRigTransform=0`.
10. Preserve the center-integrated first-person path and `privateEyeCalls:0`.

### Required telemetry

For each accepted weapon transaction, record:

- pose sequence;
- reference-space generation;
- right-aim validity;
- calibration validity;
- weapon write attempted/applied;
- weapon position residual;
- weapon angular residual;
- head contribution to the rig transform.

### Acceptance criteria

- Moving only the right controller moves the gun immediately.
- Rotating only the controller rotates the gun.
- Moving only the head does not move the gun in world space.
- Both eyes see the same controller-driven pose.
- `weaponWriteApplied=true` remains continuous while right-aim tracking is valid.
- Arm IK failure does not freeze the gun.

## Phase 3: Complete 360-degree world visibility

### Evidence

The reference video shows geometry disappearing into a white void with hard directional boundaries during a rear/right turn. Returning toward the original direction restores the scene.

This is not primarily a stale HMD center. The run proves:

- center orientation changes with the HMD;
- the left and right renderer cameras match their culler cameras;
- the eye baseline is valid;
- 130 visibility items are captured;
- only 28 items are considered replay-safe;
- 102 immediate-render items are rejected;
- `snapshotFailure=1`;
- `snapshotRenderingRetained=false`.

The white void is missing scene submission data.

### Code to inspect

- `runtime/fnvxr_retail_world_accumulation_hook_win32.cpp`
- `runtime/fnvxr_retail_world_accumulation_hook_win32.h`
- `runtime/fnvxr_retail_world_accumulation_hook_lease.h`
- `runtime/fnvxr_retail_center_renderer_operations.h`
- `runtime/fnvxr_center_renderer_backend.h`
- `renderhook/fnvxr_retail_vr_bridge_win32.h`
- `renderhook/fnvxr_d3d9_proxy.cpp`

### Implementation method

1. Preserve the current center transaction and eye-camera construction.
2. Audit the world accumulation hook installation, lease, item classification, snapshot lifetime, per-eye ordering, and desktop-camera restoration.
3. Populate and render each eye immediately while that eye's render target, renderer camera, and culler camera are simultaneously active.
4. Perform genuine scene traversal for an eye when an item cannot be replayed safely.
5. Do not silently discard immediate renderers.
6. If using a union visibility pass, ensure its culling volume covers both eye frusta and the current HMD direction—not the original desktop-forward sector.
7. Restore the stock desktop camera and renderer state only after both eyes finish.
8. Never reuse a visible set across incompatible pose sequences or reference-space generations.
9. During development, treat `snapshotFailure` or rejected required geometry as a failed stereo transaction. Do not present a partial world as success.

### Required telemetry

Record per accepted stereo transaction:

- traversed items;
- retained/replayed items;
- immediate items;
- rejected items;
- left-eye submitted items;
- right-eye submitted items;
- pose generation;
- visibility generation;
- renderer/culler camera identity.

### Acceptance criteria

- A slow physical 360-degree head turn contains continuous terrain and world geometry.
- No white void, hard black scene sector, or direction-dependent disappearance occurs.
- Terrain, buildings, statics, sky, and immediate renderers remain present.
- Normal binocular occlusion remains correct.
- Required geometry is not rejected from stereo submission.
- `snapshotFailure` no longer accompanies accepted physical stereo frames.

## Phase 4: Tune close-range comfort

Do this only after the first three phases pass.

The recorded eye baseline is stable at approximately `0.0634 m`, so do not assume runtime IPD is broken.

Review:

- controller-to-weapon calibration offset;
- weapon scale;
- near-plane clipping;
- minimum comfortable convergence distance;
- whether the model intersects either eye's near region.

Do not solve close-range discomfort by rendering the weapon monoscopically.

## Build validation

After every phase, run:

```powershell
Set-Location D:\code\fnvvr
.\scripts\build-fnvxr-product.ps1 -Configuration Release
```

The existing attested baseline passes 232 Win32/x64 tests. All tests must continue to pass before physical launch.

Add focused regression tests for every corrected authority boundary and failure condition.

## Physical launch

Use the real physical game path:

```powershell
.\scripts\start-fnvxr-product.ps1 `
  -UseAttestedBuild `
  -PhysicalHeadsetPlay `
  -PhysicalRuntimeManifest 'C:\Program Files\Oculus\Support\oculus-runtime\oculus_openxr_64.json' `
  -RetailFixtureAction Load `
  -RetailFixtureWeapon Pistol `
  -MaximumRunSeconds 600 `
  -RetailReadyTimeoutSeconds 120 `
  -HostReadyTimeoutSeconds 90 `
  -HostFrames 60000
```

Before asking the user to put on the headset, verify the new run is alive and logs prove:

- binocular projection submission;
- physical gameplay controller authority active;
- nonzero left-stick consumption at the final game boundary;
- continuous weapon writes from current right-aim poses;
- center-integrated first-person publication with `privateEyeCalls:0`;
- complete visibility submission without a rejected scene sector.

## Final physical acceptance recording

Record one continuous, easily shareable video—not side-by-side footage—showing:

1. HMD translation and rotation while the world remains stable.
2. Right-controller translation and rotation moving the gun in both eyes.
3. Left-stick forward, backward, and lateral player movement.
4. A complete physical 360-degree turn with no missing geometry.
5. Close approach to the weapon to assess convergence and clipping.
6. A menu open/close cycle proving movement keys are released and restored safely.

Telemetry is necessary diagnostic evidence but is not final acceptance. Success requires the user's physical headset report and the continuous video.

## Non-regression rules

- Do not bring back private per-eye first-person calls.
- Do not make the gun HMD-relative.
- Do not gate weapon movement on full-arm IK.
- Do not render a partial visible set as a successful stereo frame.
- Do not permit gameplay movement while menus own input.
- Do not use a short automated-test frame limit for interactive play.
- Do not claim headset success based only on monitor output or logs.
