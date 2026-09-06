# Native VR recovery checkpoint — 2026-09-06

Work stopped at the user's request for a clean push to origin. This is an
engineering checkpoint, not a playable-VR release or a headset acceptance claim.
Continue on `codex/kernel-architecture`. Do not resume a game automatically.

## What changed

- Replaced the live CPU eye-pixel transport with D3D9On12 → D3D12 shared textures
  and fence → D3D11/OpenXR. The retail game still receives the ordinary D3D9 API.
  Deleted the unused D3D9Ex-only transport and corrected the x86
  `Direct3DCreate9On12` forwarding signature.
- GPU wire version 6 carries independent world and menu channels. The host can
  retain the binocular world while receiving live wrist-menu pixels. Publication
  includes the engine transaction, exact source pose and runtime identity.
- Separated a complete, renderable stereo transaction from full gameplay-feature
  acceptance. Repeated source frames remain eligible only inside their freshness
  budget. Menus and expired frames cannot keep a stale world alive.
- Moved final-eye PNG encoding to a bounded worker queue. GPU readback remains
  restricted to explicitly requested visual capture/verification.
- Added a shared hand/forearm wrist attachment, removed the second application
  of that offset, and selected the actual first-person pistol-grip KF. The former
  third-person clip omitted four first-person hand-bone tracks.
- Added render admission backpressure and one admission per exact host pose.
  These final timing changes compile and pass tests but still need a successful
  integrated retail trial.
- Preserved and integrated the earlier pending right-forearm extraction, product
  launcher and physical wrist-interaction changes. Earlier unrelated project
  documents and media are preserved separately as historical work.

## Validation completed

The latest incremental product build passed **141 Win32 + 139 x64 tests (280)**,
including lossless asynchronous PNG encoding, rotated wrist attachment,
consumer-release admission, frame expiry, and the x86 forwarding ABI.

Local attestation: `local/product-build/fnvxr-product-Release.json`.
Build nonce: `b6daa44d9c5747448a73ffb00249e9cf`.
Source SHA-256:
`266ef1a88560c20c88ba36c64c2dd75ee01e3a43a0043f1c8eb4adb107f88032`.
Artifact SHA-256:
`4e2fa81fa1b578e7e695afa94c05296e097588c2a5e706844dd4681382071b23`.
Build output: `local/astra-product-build4.log`.

A hardware interop probe exercised a real 32-bit producer and 64-bit consumer,
two channels sharing the same D3D9 eye sources, 1872×2016 RGBA pixels, 120 frames,
consumer ownership, resource replacement and producer-epoch replacement.
Its copy-submission median was 0.320 ms and p95 0.629 ms. These are transport
measurements, not game frame-rate measurements. Evidence:
`local/gpu-channel-probes/0067d4d14add4a95ac3d109b380d6b33`.

Retail runs `20260906-085830-678-6e0bd7cf1ecf` and
`20260906-091658-184-3cc7978508f4` produced real binocular world captures and
verified independent native head/controller movement in the simulator. Neither
passed the full sustained-output acceptance gate. In the latter run, 53 saved
stereo pairs were produced; maximum observed host frame work dropped from about
282 ms to 55 ms after asynchronous PNG encoding. Freshness failures remained.
The inspected captures still exposed a wrist seam and incorrect hand posture;
the final attachment/clip corrections were made afterward.

## Unresolved at the stop

1. Complete an integrated run of the final render-admission and hand corrections.
   Run `20260906-092604-167-446482338ef1` timed out during native fixture loading,
   before any gameplay capture. It does not validate those changes.
2. Fix the Pip-Boy verifier's missing `spatialHandsOverlay` field access in
   `Get-FnvxrProductPipBoyOutputProof` in `scripts/start-fnvxr-product.ps1`.
   Run `20260906-092850-675-211a9cccd376` loaded the owned fixture but the supervisor
   then failed on that property. No wrist-screen acceptance was obtained.
3. Verify unarmed/holstered world publication: the first-person gameplay lease
   no longer requires a weapon, but the render-time weapon-commit path still
   requires separate handling when no weapon is present.
4. Verify the remaining weapon families, live grip/reload behavior, combat,
   inventory, crafting and blocking-menu interaction. The host hand is still a
   baked pistol-grip mesh, not a complete live animation solution for every weapon.
5. Physical-headset visual and comfort acceptance remains outstanding. Simulator
   pose evidence, pixel checks and passing CTests do not establish it.

## Cleanup and evidence

All owned Fallout/NVSE/OpenXR-host processes are stopped. The temporary retail
plugin profile was restored. The last supervisor encountered transient access
denials removing four staged DLLs; a subsequent identity-checked cleanup removed
all four after the processes exited. The original failed run record is unchanged.
Recovery evidence is at:
`local/product-runs/20260906-092850-675-211a9cccd376/cleanup-recovery.json`.

Raw game files, generated retail meshes, build products, per-run logs and current
captures remain under ignored local directories. They are not part of this push.
Existing historical project media outside those directories are separate from
the acceptance evidence above.

After resuming implementation, rebuild with
`powershell -NoProfile -ExecutionPolicy Bypass -File scripts/build-fnvxr-product.ps1 -Incremental`.
The supervisor's attestation check rejects mismatched source/artifact builds.
