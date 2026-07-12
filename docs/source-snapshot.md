# Source Snapshot

This repository was published as a fresh, standalone source snapshot on
2026-07-12. The snapshot includes the current FNVVR-owned implementation and
the latest uncommitted source work present in the development tree at capture
time, including the native-stereo hook work, retail plugin updates, shared
protocol changes, FABRIK solver, pose fixture, and shield-preview tool.

The publication intentionally starts with a new Git root commit under the
Nikami Creative identity. It does not inherit unrelated workspace history or
personal author metadata.

## Included

- CMake project and complete FNVVR-owned C/C++ source
- xNVSE plugin source
- D3D9, DirectInput, and XInput proxy source
- OpenXR host and probe source
- shared-memory/shared-texture protocol source
- build, staging, launch, preflight, and audit scripts
- protocol, FABRIK, pose-fixture, and rendering diagnostic tools/tests
- architecture, status, and implementation documentation

## Excluded

- Fallout: New Vegas executables, plugins, game data, saves, and assets
- xNVSE and OpenXR dependency source/binaries downloaded by the fetcher
- OpenMW/OpenMWVR donor source
- generated DLLs, EXEs, libraries, symbols, build directories, logs, dumps,
  recordings, screenshots, telemetry, and machine-local configuration
- unrelated workspace projects and Git history

Dependencies are downloaded only into ignored directories. The fetcher pins
release tags to exact source commits and verifies the xNVSE runtime archive
SHA-256 before extraction.
