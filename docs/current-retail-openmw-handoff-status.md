# Current Retail/OpenMW Handoff Status

Date: 2026-07-02

## Target

Retail Fallout New Vegas remains the authoritative game and UI source. OpenMW VR supplies the VR world shell, hands, wrist/Pip-Boy work, and pointer/controller UX. The menu/pause model is: stationary VR scene, hands active, retail UI on one interactable surface, then retail gameplay hands off to a stereo world presentation only after the downstream retail side proves it is no longer in UI and is producing separated left/right world frames.

## Current State

- Retail producer now arms stereo-world runtime by default. Menus, Pip-Boy, pause, and stereo loss stay on the flat/shared fallback path at runtime.
- D3D9 proxy now hooks `DrawPrimitive`, `DrawIndexedPrimitive`, `DrawPrimitiveUP`, and `DrawIndexedPrimitiveUP`.
- D3D9 proxy logs `stereo present state` so the next run distinguishes:
  - `suppressedByUi=1`: retail is still in UI/menu, so stereo world publish is intentionally blocked.
  - `suppressedByUi=0 replayDraws=0`: gameplay reached, but draw replay still is not landing.
  - `shared stereo captured` / `fnvxrD3d9EyeTarget ready=true`: retail stereo world frames are ready.
- D3D9 proxy now reads `Local\FNVXR_Runtime_State` from the NVSE main-loop writer and uses that as the primary UI/gameplay predicate. The old menu-address checks are fallback only.
- D3D9 proxy now treats non-UI runtime plus valid view/projection as enough to mark a captured stereo frame as a world candidate, even if the older shared-camera-active bit stays conservative.
- NVSE runtime state no longer treats a stale loading-menu visibility byte as blocking unless a live loading tile-menu object also exists.
- OpenMW `FNVXRLiveFrameSurface` now has a stereo-frame consumer path for `Local\FNVXR_D3D9_StereoFrame_v1`.
- OpenMW switches the live retail surface to per-eye textures when retail runtime is gameplay and the stereo frame is separated/world/non-UI.
- OpenMW no longer hides the mono portal just because runtime says gameplay; it waits for a real stereo world frame.
- OpenMW no longer requires `cameraActive` before probing the stereo frame. Runtime gameplay opens the gate; the D3D stereo header remains the hard proof.
- OpenMW no longer lets the grip-only menu-panel gate block gameplay stereo presentation. Stereo readiness is polled independently of fresh mono-frame delivery.
- OpenMW's current proof surface binds both left/right retail stereo textures in one multiview draw so a multiview path cannot accidentally show the left eye to both eyes.
- OpenMW world placement now latches an anchor instead of following the headset every frame.
- OpenXR session submission can now hold multiple projection layers without aliasing one local `XrCompositionLayerProjection` / view array.
- OpenMW now has a first FNVXR retail projection-layer presenter path. Once retail stereo is ready, it creates left/right OpenXR color swapchains, uploads the retail stereo pixels on frame end, stamps the layer with current OpenXR views, inserts the projection layer after the OpenMW base projection, and hides the proof surface after the projection layer has uploaded at least once.
- Projection-layer submission now skips malformed projection layers instead of submitting null spaces/swapchains.
- If retail leaves gameplay or stereo readiness drops, OpenMW tears down the retail projection layer before returning to menu/proof-surface behavior.
- Projection-layer upload now guards unavailable XR swapchain images and vertically flips the D3D-sourced stereo pixels during upload so it does not rely on proof-quad UVs for orientation.
- Projection-layer upload now requires a fresh advancing stereo sequence. A valid-but-stale stereo frame can no longer keep the retail world projection alive indefinitely; the freshness timeout defaults to `OPENMW_FNVXR_RETAIL_PROJECTION_STALE_MS=750`.
- OpenMW only hides the proof/menu surface after the retail projection layer is both uploaded and inserted into the XR layer stack. A ready-but-not-inserted projection layer can no longer blank the transition.
- If a retail projection upload fails after the layer has been inserted, OpenMW removes/demotes that projection layer and keeps the proof surface active instead of leaving an old retail image hanging over the scene.

## Architecture Decision

The current `FNVXRLiveFrameSurface` path is a proof/menu/fallback surface. It can prove the retail stereo producer is alive, separated, and synchronized, but it is still a scene-graph surface.

The real "retail becomes the world" target is an OpenXR projection-layer presenter owned by OpenMW/OpenMWVR:

- OpenMW owns OpenXR frame timing, hands, input, and composition.
- Retail FNV produces left/right world images.
- The bridge copies or shares those images into OpenMW-owned XR projection swapchain views.
- Retail menus/Pip-Boy/pause surfaces remain quad/layer UI.

This matches the SWG reference pattern: projection layer for the world, quad layers for UI/HUD/hands/tools. DX9-to-DX11 sharing is an implementation detail under the OpenXR projection-layer path, not a swapchain-ownership reversal.

The current implementation is the first CPU-upload version of that presenter. It is not the final high-performance GPU-share path, but it changes the handoff shape from "keep showing retail gameplay on a scene quad" toward "submit retail gameplay as an OpenXR projection layer."

## Latest Proof

Latest retail run:

- `FNVXR_D3D9_STEREO_REPLAY=1`
- `FNVXR_D3D9_STEREO_READBACK=1`
- `FNVXR_D3D9_SHARED_STEREO=1`
- `FNVXR_D3D9_SHADER_STEREO=1`
- UP draw hooks installed.

Latest D3D9 proof lines:

```text
runtime state observed seq=... phase=2 menuBits=0x23 camera=0 showroom=0
stereo present state frame=... suppressedByUi=1 replayDraws=0 haveView=1 haveProjection=1 sharedStereo=1
```

That means the current live process is still in the retail menu. Stereo world publishing is intentionally suppressed until the runtime protocol reports gameplay or the non-UI/gameplay path proves out after click-through.

## Oracle Findings Applied

- Retail-side stereo replay was starved while `phase=2`; this is expected in the menu. The post-gameplay world-candidate gate was hardened so it cannot deadlock on `cameraActive=0` after `suppressedByUi=0`.
- OpenMW-side stereo presentation was accidentally nested behind the menu-panel/grip allowance and fresh mono frames. That has been split so gameplay stereo can arm and present even when the grip panel is no longer allowed.
- OpenMW multiview cannot rely on `applyLeft/applyRight` state mutation. The proof surface now binds both eye textures and uses `gl_ViewID_OVR` when multiview is enabled.
- Remaining known non-goal blockers: OpenMW raycast focus still needs a direct 3DGUI/retail-surface hit path, OpenMW HUD/pane leakage needs a suppressor, and the true non-quad presentation path needs the projection-layer presenter above after stereo-frame proof.

## Next Proof Step

Start retail with:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\proof\fnvxr-bridge-experiment\scripts\start-retail-surface-producer.ps1 -StopExisting
```

Start OpenMW with:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\proof\fnvxr-bridge-experiment\scripts\start-openmw-fnv-sidecar.ps1 -MonitorSeconds 45 -StopExisting -NoSave
```

In headset, get retail past the menu. Then check `fnvxr_d3d9_proxy.log` in the configured retail FNV game root.

Expected next successful chain:

```text
runtime state observed ... phase=3 ...
stereo present state ... suppressedByUi=0 replayDraws=<nonzero>
created stereo replay targets ...
stereo replay draw count=...
shared stereo captured ... worldCandidate=1 separated=1 uiActive=0
{"event":"fnvxrD3d9EyeTarget",...,"ready":true,...}
```

Expected OpenMW chain after that:

```text
FNVXR retail surface: mapped Local\FNVXR_D3D9_StereoFrame_v1
FNVXR retail surface: stereo world ready ...
FNVXR retail surface: retail stereo projection layer requested
FNVXR retail surface: created retail projection swapchains ...
FNVXR retail surface: uploaded first retail stereo projection frame sequence=...
FNVXR retail surface: inserted retail projection layer
FNVXR retail surface: projection layer is live; hiding proof surface
```

Failure logs that now have explicit meaning:

```text
FNVXR retail surface: stereo world frame is valid but stale; refusing projection flip ...
FNVXR retail surface: retail projection upload failed; keeping proof surface active: ...
```

## Do Not Regress

- Do not replace the retail UI quad with OpenMW UI.
- Do not mutate retail into showroom scenes to fake menu background.
- Do not hide the mono fallback unless a real stereo world frame is ready.
- Do not hand-toggle random environment flags. Use named script switches/profiles.
- Do not let OpenMW VRGUI or movement controls steal focus while the retail surface owns input.
