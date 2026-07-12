# FNVXR Bridge Status

Last updated: 2026-07-04

## Pinned Baseline

The first passable in-headset retail FNV baseline is pinned in `docs/playable-big-screen-vr-baseline.md`.

Scope: retail Fallout New Vegas is playable in VR on the OpenXR sidecar's large curved screen. Menu/gameplay entry, basic controller routing, runtime state gating, L3 run/stamina mode, R3 auto-forward, B jump, Y favorites, and Pip-Boy/inventory UI lane handling have live proof from `local\openxr-retail-sidecar-runs\20260704-122203`.

This is the first-cut big-screen VR baseline, not the final native stereo world presentation.

## Proven

- Dependencies are cached locally under `deps/`.
- Latest fetched xNVSE release: `6.4.8`.
- Latest fetched Khronos OpenXR SDK and SDK-Source release: `release-1.1.60`.
- Retail FNV is detected through the local probe script; the exact install path is intentionally local-only.
- `FalloutNV.exe` is present and is x86 / 32-bit.
- Installed retail FNV version is `1.4.0.525`.
- Installed xNVSE runtime files are present in the game root:
  - `nvse_loader.exe`
  - `nvse_1_4.dll`
  - `nvse_steam_loader.dll`
- Existing live xNVSE plugins include JIP LN NVSE and ShowOffNVSE.
- `nvse_fnvxr.dll` builds as both x64 test DLL and Win32 FNV-compatible DLL.
- The Win32 DLL exports `NVSEPlugin_Query` and `NVSEPlugin_Load`.
- The Win32 DLL creates the shared-memory bridge mappings when `NVSEPlugin_Load` is called.
- The loaded DLL test validates the exported xNVSE entry points and shared-state ABI.
- The local OpenXR loader is visible when using the vcpkg loader path.
- A local OpenXR runtime is active and visible to the probe.
- OpenXR instance creation succeeds.
- With the headset awake, `xrGetSystem(XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY)` succeeds.
- `nvse_fnvxr.dll` was installed into the live FNV xNVSE plugin folder.
- Retail `FalloutNV.exe` launched through `nvse_loader.exe` and loaded the bridge DLL.
- Live bridge smoke passed against the actual game process through shared memory:
  - OpenXR/OpenMW-side state is consumed by `FalloutNV.exe`.
  - The xNVSE-loaded DLL publishes runtime, camera, player, and input telemetry.
- Real OpenXR pose host passed against the live game process:
  - OpenXR D3D11 session creation succeeds.
  - HMD and controller action spaces are created.
  - Host sent real headset/controller pose state into FNV through the shared mappings.
  - Live FNV bridge published runtime/game state during the prop-layer smoke.
- OpenXR diagnostic prop runs proved that external layers can render while the live FNV bridge responds.
- The diagnostic prop runs are not the playable hand/Pip-Boy path.
- The exact OpenMWVR reuse points for hands, finger curl, Pip-Boy wrist attachment, pointer ray, and GUI/mouse click injection are recorded in `docs/openmwvr-reuse-map.md`.
- The OpenXR pose host now creates controller actions for trigger, squeeze, A/B/X/Y, menu, and thumbstick clicks.
- The pose host drains lifecycle events until the session reaches focused state; controller action bindings become active (`activeMask=0x7ff`).
- The pose protocol now carries an optional normalized menu pointer hit from the right aim ray to the game quad.
- The OpenXR host can draw a visible yellow pointer marker on the game quad when `FNVXR_SHOW_GAME_PLANE=1`.
- The live xNVSE DLL can map that menu pointer hit into the FNV client window cursor, so Button A clicks the aimed point instead of relying on desktop automation.
- The live xNVSE DLL consumes rising-edge input intent:
  - A -> left mouse click
  - X -> activate key (`E`)
  - left menu -> Pip-Boy/menu key (`Tab`)
- OpenXR prop calibration now separates grip pose from aim pose:
  - grip pose drives left/right hand markers and the temporary wrist/Pip-Boy rig
  - right aim pose drives the temporary pointer ray
- The temporary OpenXR renderer now uses a D3D11 depth buffer for the prop pass.
- Local-only retail FNV source meshes were inspected for calibration. Extracted game assets stay ignored and must not be published.
- A Win32 `d3d9.dll` proxy now builds for the retail game render hook path.
- Live FNV loads the staged D3D9 proxy from the game root.
- The proxy forwards the full public D3D9 export set and wraps `IDirect3D9`.
- The proxy intercepts the live `CreateDevice` call:
  - windowed: `1`
  - backbuffer: `2048x1280`
  - format: `22` (`D3DFMT_X8R8G8B8`)
- The proxy hooks live `IDirect3DDevice9::Reset`, `Present`, `EndScene`, `Clear`, `SetTransform`, `SetVertexShaderConstantF`, `DrawPrimitive`, and `DrawIndexedPrimitive`.
- Live FNV submits fixed-function `D3DTS_VIEW` / `D3DTS_PROJECTION` matrices at roughly 120 transform calls/sec in the menu path.
- The hook derives left/right eye view matrices from the live FNV view matrix with a default 0.064m IPD:
  - left eye: +0.032m on FNV view-space X
  - right eye: -0.032m on FNV view-space X
- `fnvxr_stereo_math_test` locks the eye-split sign and verifies projection pass-through until asymmetric OpenXR frusta are wired.
- Offscreen stereo replay is implemented behind `FNVXR_D3D9_STEREO_REPLAY=1`:
  - creates left/right 2048x1280 render target textures plus depth surfaces
  - replays draw calls into those targets with derived left/right view matrices
  - restores FNV's original render target, depth surface, and transforms
- Optional stereo readback is implemented behind `FNVXR_D3D9_STEREO_READBACK=1`:
  - creates system-memory readback surfaces
  - logs left/right hashes, `worldCandidate`, and `separated`
  - menu/UI hashes are expected to match and must not be counted as in-world stereo proof
- Short live replay smoke passed without crashing:
  - created stereo replay targets: `2048x1280`, format `22`
  - replayed over 2500 draw calls
  - Present recovered to about 60 FPS in the menu path

## Current Blockers

- The current standalone sidecar path has a passing in-headset big-screen gameplay proof. It still needs polish for UI/UX, inventory/Pip-Boy ergonomics, and final interaction mapping.
- The OpenXR diagnostic prop layer is still using boxes/rays, not the extracted NIF meshes or the existing OpenMWVR hand/Pip-Boy renderer.
- The headset/runtime occasionally drops OpenXR focus during long passes; render poses are smoothed/held, but action active masks can still fall to zero until focus returns.
- The captured game panel is hidden by default during prop calibration (`FNVXR_SHOW_GAME_PLANE=0`) because desktop/window capture can introduce black occluder artifacts.
- The current pinned playable view is a large curved flat-FNV screen in OpenXR. Offscreen stereo replay is not yet the final in-headset world presentation.
- The offscreen replay/readback path has only been smoke-tested in the menu/windowed path. It still needs a user-in-character stress pass, depth/clear validation, and a transfer/share path into the OpenXR host.
- Menu/UI replay is not a valid stereo proof. A stereo proof requires an in-world/player-camera frame with `worldCandidate=1` and eventually `separated=1`.

## Safe Local Artifacts

- Staged plugin layout:
  `local/fnv-plugin-stage/Data/NVSE/Plugins/nvse_fnvxr.dll`
- Dependency manifest:
  `deps/manifest.json`
- FNV probe:
  `local/fnv-probe.json`
- Combined preflight:
  `local/preflight-fnvxr.json`
- Live D3D9 proxy log in the local retail FNV game root.

## Next Live Steps

1. Add DLL-side logging to `Data\NVSE\Plugins\Logs\` or a local lab log file.
2. Run a headset pass with deliberate trigger/squeeze/A/X/menu input and confirm nonzero `PoseFrame` values.
3. Use the in-headset game quad pointer to click Continue/Load without desktop mouse automation.
4. Wire the D3D9 left/right render target textures into an OpenXR-visible submission path.
5. Stress `FNVXR_D3D9_STEREO_REPLAY=1` in an in-world save and validate clears, depth, UI passes, and frame time.
6. Replace projection pass-through with asymmetric per-eye OpenXR frusta.
7. Port the existing OpenMWVR hand/Pip-Boy/pointer systems listed in `docs/openmwvr-reuse-map.md`.
