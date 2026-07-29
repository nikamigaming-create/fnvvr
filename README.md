# FNVVR

FNVVR is a source-only VR injection mod for the retail Windows version of
Fallout: New Vegas. The original `FalloutNV.exe` remains authoritative while a
standalone OpenXR host supplies headset/controller tracking and VR
presentation. An xNVSE plugin and retail proxy DLLs expose the engine state and
rendering hooks needed to connect the two sides.

This repository contains only FNVVR-owned C++ and PowerShell source. It does
not contain Fallout: New Vegas files, Bethesda assets, xNVSE binaries, OpenXR
binaries, generated build output, local logs, or private runtime data.

FNVVR does not fork or patch xNVSE. `nvse_fnvxr.dll` is a separate xNVSE
plugin. The D3D9, DirectInput, and XInput proxies and the OpenXR host are also
FNVVR-owned components. Upstream dependencies are fetched unchanged into
ignored directories.

## Retail-Only Architecture

- `FalloutNV.exe` owns simulation, saves, quests, collision, menus, weapons,
  projectiles, hit tests, and animation state.
- The in-process engine backend owns exact-version discovery, guarded world
  scheduling, retail camera/weapon application, UI classification, and
  authoritative telemetry.
- `d3d9.dll` retains mono retail UI capture and graphics interop. Per-draw D3D
  replay is not the production stereo renderer.
- The standalone OpenXR host owns headset/controller actions, frame timing,
  VR composition, the stable retail UI quad, and debug visualization. The
  production target opens GPU-shared eye color/depth resources. The separately
  bounded visual trial currently transfers engine-rendered BGRA8 eye images
  through the versioned CPU-v8 transport.
- Same-machine fixed-size shared mappings carry poses, input intent, retail
  state, commands, and versioned transaction/resource metadata. There is no
  second game runtime and no network transport.

The presentation contract has two modes:

- All non-blocking gameplay, exploration, combat, and world interaction require
  true binocular 3D, independent 6DoF head motion, a tracked retail weapon,
  and no persistent gameplay HUD. Mono gameplay is never accepted as success.
- Startup, pause, inventory, barter, terminals, dialogue, VATS, loading,
  Pip-Boy, and other blocking retail UI use the stable mono quad. The
  controller ray drives the ordinary retail mouse pointer and click path.
- Leaving UI holds the last valid quad until one fresh, complete, pose-matched
  stereo transaction from a strictly newer retail source frame is ready, then
  changes to world stereo atomically. Stale stereo remains rejected even after
  the bounded quad hold expires to a safety blank.

## Current Status

The retained per-D3D-draw replay remains a production NO-GO. A separate,
exact-profile `stereo-visual-trial-v5` route is now implemented for bounded
binocular verification: one native Present lease defers authority until the
xNVSE plugin has published authenticated main-loop state, then one exact world
hook renders private left/right cameras, accumulators, color targets, and
depth/stencil targets from one conservative visible set. The OpenXR host
submits the resulting two projection views. Each eye target is cleared before
rendering, outputs must be non-aliased and distinct, and mono gameplay
fallbacks remain disabled.

This is not full product acceptance. The CPU-v8 image transfer is intentionally
bounded and too expensive for the production transport; controller mutation,
tracked weapon/muzzle behavior, encoded depth submission, and the retained D3D
draw-hook set remain source-fused. The plugin is inert outside explicit
profiles, and the visual trial keeps its full input/camera/rig bridge disabled
so it cannot invalidate the D3D engine-authority proof.

The process-local headless OpenXR Simulator is now built and can supply
deterministic HMD/controller poses and final-eye capture without changing the
machine-wide OpenXR runtime. The latest TTW simulator run started the host and
retail process, verified advancing runtime/pose publication and the CPU-v8
bridge, and published non-black mono Start Menu frames. It did not reach
gameplay or accepted binocular output. No recorded run has yet authorized
controller mutation or a tracked weapon, so this tree must not be described as
a stable playable VR build.

Read-only inspection of the loaded retail `1.4.0.525` executable has verified
the world-render boundary, explicit visible-array culling, and separate
accumulator render/finalize primitives. The clean replacement is therefore a
bounded engine transaction: build one conservative union visible set, render
it through fresh non-aliased left/right accumulators with each eye's complete
camera/color/depth/auxiliary state bound before population, restore retail
state, then publish GPU-native resources atomically.

The remaining production work is GPU-native color/depth transport,
authoritative weapon/muzzle alignment, complete stock-branch and scene
coverage, performance, and full retail/headset acceptance. The guarded product
launcher admits only the bounded stereo visual trial; it never marks the full
product accepted. See `docs/architecture-v2.md` for the production contract,
`docs/status.md` for the verified boundary, and `docs/next-steps.md` for
acceptance gates.

## Layout

- `protocol/` - fixed shared-memory ABI structs and validation helpers.
- `plugin/` - retail xNVSE plugin.
- `renderhook/` - retail D3D9, DirectInput, and XInput hooks.
- `host/` - standalone OpenXR host and probes.
- `scripts/` - build, staging, retail launch, probe, and audit entrypoints.
- `docs/experiment-brief.md` - process split and authority boundary.
- `docs/architecture-v2.md` - production stereo transaction and GPU transport.
- `docs/next-steps.md` - practical retail camera, weapon, UI, and stereo plan.
- `docs/assist-mode.md` - deterministic, headset-free head/body acceptance harness.
- `docs/status.md` - current proof results and live-test blockers.
- `docs/prop-layer-plan.md` - retail hands, weapon, pointer, and UI plan.

## Pinned Dependencies

The dependency fetcher verifies the release tag, exact source commit, and the
xNVSE runtime archive SHA-256 before extracting anything:

- xNVSE `6.4.8` at `062bccb15abd0397aaeb0a2cf58d7c3ca6140618`
- OpenXR SDK `release-1.1.60` at `64f2b37c8c6da3d83c9b4d11865ba1fb752cb8ec`
- OpenXR SDK Source `release-1.1.60` at `c07ad64839653712190e05dbd8cf460e1d239513`

Fetch dependencies:

```powershell
.\scripts\fetch-deps.ps1
```

Build the FNV-compatible 32-bit plugin and retail proxy DLLs:

```powershell
.\scripts\build-win32.ps1
```

Build and attest both product architectures, then perform the read-only
game/install validation:

```powershell
.\scripts\build-fnvxr-product.ps1
.\scripts\start-fnvxr-product.ps1 -UseAttestedBuild -ValidateOnly
```

With Quest Link/Air Link active and a loaded gameplay world available, run the
bounded binocular trial:

```powershell
.\scripts\start-fnvxr-product.ps1 `
  -UseAttestedBuild `
  -MaximumRunSeconds 60 `
  -RetailReadyTimeoutSeconds 90
```

The supervisor stages the exact attested Win32 product set, launches the host
before retail FNV, requires advancing runtime/pose and distinct binocular
OpenXR output, stops only its owned processes, and restores every prior
game-root file on success or failure.

OpenXR diagnostic (blocked): `scripts/run-openxr-probe.ps1` intentionally
refuses before configure, build, loader, or runtime access until its reviewed
runtime-touch proof and compiled source fuse are complete.

Stage the xNVSE plugin without modifying the live game install:

```powershell
.\scripts\stage-plugin.ps1
```

Write a combined local preflight report:

```powershell
.\scripts\preflight-fnvxr.ps1
```

Review the headset-free desktop-assist configuration without staging or
launching the game:

```powershell
.\scripts\validate-desktop-assist.ps1 `
  -GameRoot 'D:\SteamLibrary\steamapps\common\Fallout New Vegas'
```

Only stage or validate the separate desktop-assist profile through its guarded
launcher:

```powershell
.\scripts\start-desktop-assist.ps1 `
  -GameRoot 'D:\SteamLibrary\steamapps\common\Fallout New Vegas' `
  -ValidateOnly
```

The live desktop-assist supervisor requires its own explicit
`-ApproveStageAndLaunch` switch, temporarily stages only `d3d9.dll` and
`nvse_fnvxr.dll`, and restores the prior files when its owned desktop game
session ends. Adding `-RunAcceptanceTrial` waits for first-person body-root
readiness, then records a current-pose head/body and menu-source transition
report without launching OpenXR or enabling world stereo, and automatically
restores the two staged files immediately after a passing trial. Before any
approved live staging, it rebuilds the exact Win32 stage artifacts and x64
evidence tools; `-ValidateOnly` only reports that build plan and leaves all
artifacts untouched. Adding `-AutomateAcceptance` to `-RunAcceptanceTrial`
uses the fixed `FNVXR_HostExitRecovery` save and one foreground-verified
Escape open/close pair, so the head/body and menu-source proof can run without
headset or manual desktop input.

```powershell
.\scripts\start-openxr-retail-sidecar.ps1 -StageOnly
.\scripts\start-openxr-retail-sidecar.ps1 -ValidateOnly
```

## Design Rules

FNV decides gameplay. The host may compose retail frames, hands, pointers, and
debug aids, but activation, firing, projectile direction, hits, menus, and
state changes must be applied or confirmed inside retail FNV.

The xNVSE plugin can be extended to inspect additional retail structures and
publish versioned telemetry. Unknown engine offsets and hook points still need
runtime-version guards and live proof; copying a value to the host does not by
itself reproduce the corresponding engine behavior.

See `docs/source-snapshot.md` for the publication boundary and exclusions.
