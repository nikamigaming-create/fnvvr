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
- `nvse_fnvxr.dll` owns in-process engine discovery, guarded transform/input
  changes, runtime/UI classification, and retail telemetry.
- `d3d9.dll` owns retail frame capture and the work toward complete same-frame
  left/right world rendering.
- The standalone OpenXR host owns headset/controller actions, frame timing,
  VR composition, the flat retail UI surface, and debug visualization.
- Same-machine fixed-size shared mappings carry poses, input intent, retail
  state, commands, and complete frame surfaces. There is no second game
  runtime and no network transport.

The presentation contract has two modes:

- Gameplay may enter native stereo only when retail reports an unobstructed
  world state and the render hook publishes a valid complete eye pair.
- Startup menus, pause/menu mode, inventory, Pip-Boy, dialogue, VATS, loading,
  and any other retail UI state stay on a flat mono surface. Native stereo is
  suppressed until retail proves it is safely back in gameplay.

## Current Status

The repository includes the playable retail flat-screen-in-headset baseline,
live OpenXR pose streaming, xNVSE state/input plumbing, controller mappings,
flat retail UI presentation and input plumbing, D3D9 capture/replay
diagnostics, FABRIK arm work, and retail skeleton/weapon-node discovery.

Three headset-critical behaviors are not signed off yet:

- The native camera now composes the fully recentered HMD transform on the
  body-local side and renders one union-frustum Gamebryo traversal. This needs
  one live headset acceptance run; the previous flat run is not 3D evidence.
- The right OpenXR aim pose now reaches right-hand IK as a separate,
  validity-flagged orientation source. It does not yet drive the weapon node,
  muzzle, projectile, or hit ray.
- Retained cell-transition telemetry proves that two stateful full-engine eye
  traversals can submit materially different scene work. Production stereo no
  longer uses that path: one exact D3D9 draw stream is replayed into both eyes,
  with explicit single-traversal provenance and live acceptance gates.

Native same-frame stereo world presentation also remains active development.
See `docs/status.md` and `docs/next-steps.md` for the current proof boundary and
acceptance gates.

## Layout

- `protocol/` - fixed shared-memory ABI structs and validation helpers.
- `plugin/` - retail xNVSE plugin.
- `renderhook/` - retail D3D9, DirectInput, and XInput hooks.
- `host/` - standalone OpenXR host and probes.
- `scripts/` - build, staging, retail launch, probe, and audit entrypoints.
- `docs/experiment-brief.md` - process split and authority boundary.
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

Verify the local OpenXR loader/runtime:

```powershell
.\scripts\run-openxr-probe.ps1
```

Stage the xNVSE plugin without modifying the live game install:

```powershell
.\scripts\stage-plugin.ps1
```

Write a combined local preflight report:

```powershell
.\scripts\preflight-fnvxr.ps1
```

Launch the retail/OpenXR path through the guarded launcher:

```powershell
.\scripts\start-openxr-retail-sidecar.ps1
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
