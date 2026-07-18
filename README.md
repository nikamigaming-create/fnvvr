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
  VR composition, the stable retail UI quad, and debug visualization. It opens
  accepted GPU-shared eye color/depth resources; it does not receive a CPU
  pixel ring.
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

The retained per-D3D-draw replay and CPU readback/ring work is diagnostic only;
it is a production NO-GO. **Every live OpenXR presentation path and every
retail mutation path is currently source-blocked.** The installed plugin is
inert by default, and config or environment variables cannot bypass the fuse.
The legacy stereo environment setup now throws before writing activation
variables, and separate host/D3D integration fuses prevent a one-line proof
flip from reviving mono gameplay fallback or the persistent-HUD path.

Read-only inspection of the loaded retail `1.4.0.525` executable has verified
the world-render boundary, explicit visible-array culling, and separate
accumulator render/finalize primitives. The clean replacement is therefore a
bounded engine transaction: build one conservative union visible set, render
it through fresh non-aliased left/right accumulators with each eye's complete
camera/color/depth/auxiliary state bound before population, restore retail
state, then publish GPU-native resources atomically.

The remaining implementation is the production engine backend, GPU-native
color/depth transport and OpenXR submission, authoritative weapon/muzzle
alignment, and full retail/headset acceptance. The launcher remains blocked
until those gates pass. See `docs/architecture-v2.md` for the production
contract, `docs/status.md` for the verified boundary, and `docs/next-steps.md`
for acceptance gates.

## Layout

- `protocol/` - fixed shared-memory ABI structs and validation helpers.
- `plugin/` - retail xNVSE plugin.
- `renderhook/` - retail D3D9, DirectInput, and XInput hooks.
- `host/` - standalone OpenXR host and probes.
- `scripts/` - build, staging, retail launch, probe, and audit entrypoints.
- `docs/experiment-brief.md` - process split and authority boundary.
- `docs/architecture-v2.md` - production stereo transaction and GPU transport.
- `docs/next-steps.md` - practical retail camera, weapon, UI, and stereo plan.
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

Only stage or validate artifacts through the guarded launcher; there is no
authorized headset launch command in the current tree:

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
