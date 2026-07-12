# FNVVR

FNVVR is a source-only VR injection mod for the retail Windows version of
Fallout: New Vegas. It keeps the original game executable authoritative and
adds OpenXR presentation, controller input, retail UI interaction, and the
in-process hooks needed for native stereo work.

This repository contains the complete FNVVR-owned C++ and PowerShell source.
It does not contain Fallout: New Vegas files, Bethesda assets, xNVSE binaries,
OpenXR binaries, generated build output, local logs, or private runtime data.

FNVVR does not fork or patch xNVSE, OBSE, SKSE, F4SE, or SFSE. `nvse_fnvxr.dll`
is a separate xNVSE plugin, and the D3D9/DirectInput/XInput proxies plus OpenXR
host are FNVVR-owned components. Upstream dependencies are fetched unchanged
into ignored directories.

## Architecture

- `FalloutNV.exe` remains the authoritative game and menu runtime.
- `nvse_fnvxr.dll` owns retail-side state, input, menu clicks, camera/player telemetry, and shared-memory contracts.
- `d3d9.dll` owns retail D3D9 frame capture, stereo replay/readback, and world/UI gating from retail runtime state.
- The standalone OpenXR host owns headset/controller tracking, VR presentation, menu quad placement, and hand/pointer UI.
- The live bridge nucleus is same-machine shared memory/shared texture state. It is not a network transport.

OpenMWVR is a donor/reference for proven hand, Pip-Boy, pointer, and IK design.
It is not the runtime for this mod: retail `FalloutNV.exe` remains the game.
The OpenMW comparison/harvest scripts are optional integration diagnostics and
are not required by the CMake build or the standalone OpenXR retail launcher.

## Current Status

The source includes the playable retail big-screen/OpenXR baseline, xNVSE and
input proxies, shared-state telemetry, pose hosting, hand/arm FABRIK work, and
the current native-stereo D3D9 replay/readback path. Native stereo presentation
is active development and is not represented here as finished.

## Layout

- `protocol/` - fixed shared-memory ABI structs and validation helpers.
- `plugin/` - xNVSE retail-side plugin.
- `renderhook/` - D3D9, DirectInput, and XInput retail hooks.
- `host/` - standalone OpenXR sidecar host and probes.
- `scripts/` - build, staging, launch, probe, and audit entrypoints.
- `docs/experiment-brief.md` - current shared-memory architecture notes.
- `docs/playable-big-screen-vr-baseline.md` - pinned first playable big-screen VR checkpoint.
- `docs/next-steps.md` - practical phases and integration checkpoints.
- `docs/status.md` - current proof results and live-test blockers.
- `docs/prop-layer-plan.md` - how hands/Pip-Boy props fit before true stereo/UI capture.

## Pinned Dependencies

The dependency fetcher verifies the release tag, exact source commit, and the
xNVSE runtime archive SHA-256 before extracting anything:

- xNVSE `6.4.8` at `062bccb15abd0397aaeb0a2cf58d7c3ca6140618`
- OpenXR SDK `release-1.1.60` at `64f2b37c8c6da3d83c9b4d11865ba1fb752cb8ec`
- OpenXR SDK Source `release-1.1.60` at `c07ad64839653712190e05dbd8cf460e1d239513`

Fetch them into the ignored `deps/` directory:

```powershell
.\scripts\fetch-deps.ps1
```

For the FNV-compatible 32-bit plugin and retail proxy DLLs:

```powershell
.\scripts\build-win32.ps1
```

To verify the local OpenXR loader/runtime is visible:

```powershell
.\scripts\run-openxr-probe.ps1
```

To stream real OpenXR poses into the bridge protocol:

```powershell
.\scripts\run-openxr-pose-host.ps1
```

Use `-Frames 10800` for roughly two minutes at 90 Hz.

To stage the xNVSE plugin layout without modifying the live game install:

```powershell
.\scripts\stage-plugin.ps1
```

To write a combined local preflight report:

```powershell
.\scripts\preflight-fnvxr.ps1
```

## Design Rule

FNV decides gameplay. The VR host may draw hands, menus, pointers, and debug aids, but actual activation, hits, firing, menus, and state changes must be confirmed by the in-game plugin.

See `docs/source-snapshot.md` for the publication boundary and exclusions.
