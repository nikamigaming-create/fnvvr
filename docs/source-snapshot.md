# Source Snapshot

This repository was published as a fresh, standalone source snapshot on
2026-07-12. It includes the current FNVVR-owned retail implementation and the
latest source work present at capture time: native-stereo hook work, retail
plugin updates, shared protocol changes, the FABRIK solver, pose fixture, and
shield-preview tool.

The publication starts with a new Git root commit under the Nikami Creative
identity. It does not inherit unrelated workspace history or personal author
metadata.

## Included

- CMake project and complete FNVVR-owned C/C++ source;
- retail xNVSE plugin source;
- D3D9, DirectInput, and XInput proxy source;
- standalone OpenXR host and probe source;
- shared-memory/shared-frame protocol source;
- retail build, staging, launch, preflight, and audit scripts;
- protocol, FABRIK, pose-fixture, and rendering diagnostic tools/tests;
- retail-only architecture, status, and implementation documentation.

## Excluded

- Fallout: New Vegas executables, plugins, game data, saves, and assets;
- xNVSE and OpenXR dependency source/binaries downloaded by the fetcher;
- generated DLLs, EXEs, libraries, symbols, build directories, logs, dumps,
  recordings, screenshots, telemetry, and machine-local configuration;
- unrelated workspace projects and Git history.

Dependencies are downloaded only into ignored directories. The fetcher pins
release tags to exact source commits and verifies the xNVSE runtime archive
SHA-256 before extraction.
