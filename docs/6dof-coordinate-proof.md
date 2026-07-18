# FNVXR offline 6DoF coordinate proof

Current verdict: **not ready for another headset run**. The spatial transform is
now coherent and passes deterministic tests, but complete runtime shader and
draw ownership is not yet proven. This document is the human-readable source;
the printable report is generated at `output/pdf/fnvxr_6dof_coordinate_proof.pdf`.

## Frames

OpenXR local axes are `(right, up, back)`, with `-Z` forward. NiCamera local
axes are also `(right, up, back)`, so native camera translation uses the
identity map:

```text
xr_to_nicamera_local(x, y, z) = (x, y, z)
```

Actor/Gamebryo vectors instead use `(right, forward, up)`:

```text
xr_to_actor(x, y, z) = (x, -z, y)
```

Applying the actor permutation before a NiCamera basis is the old fault. A
physical `100 mm` forward displacement `(0,0,-0.1)` became local camera-up
`(0,+0.1,0)`. A physical lift became camera-back.

The configured Fallout scale used here is the approximation `s = 70 units/m`,
derived from the documented roughly 7 units per 10 cm / 64 units per yard; it
is not a measured runtime calibration. Source: <https://geckwiki.com/index.php?title=Units>.
Therefore:

```text
100 mm = 7.00 units
64 mm IPD = 4.48 units
left/right half IPD = -2.24/+2.24 units
```

## Rigid transform theorem

Let:

- `p_o` be the eye-midpoint position captured at recenter.
- `R_o` be the gravity-aligned, yaw-only recenter rotation.
- `p_i, R_i` be the source OpenXR pose of eye `i`.
- `C` be the leveled NiCamera world rotation.
- `b` be the engine-authored camera anchor.
- `X` be any point in the OpenXR room frame.

The native eye camera is:

```text
d_i       = R_o^T (p_i - p_o)
R_i_local = R_o^T R_i
p_i_game  = b + s C d_i
R_i_game  = C R_i_local
```

Map the room point by the same similarity transform:

```text
X_game = b + s C R_o^T (X - p_o)
```

Then its coordinates in eye `i` are:

```text
R_i_game^T (X_game - p_i_game)
= (C R_o^T R_i)^T [s C R_o^T (X - p_i)]
= s R_i^T R_o C^T C R_o^T (X - p_i)
= s R_i^T (X - p_i)
```

Thus the engine eye frame is exactly the OpenXR eye frame multiplied by one
uniform scale. Perspective direction ratios are unchanged.

This theorem requires the pixels, `p_i/R_i`, center camera factor, eye camera
factor, and FOV to belong to one accepted source-pose transaction. The host now
also rejects a source pose older than `25 ms` because translation cannot be
exactly late-reprojected without a depth layer.

## Other repaired proof obligations

- Recenter stores yaw only and latches position/yaw from the same pose.
- Looking vertically retains the previous yaw instead of snapping at the yaw
  singularity.
- Eye baselines must be finite, `30-120 mm`, correctly ordered left-to-right,
  and predominantly lateral in HMD-local coordinates.
- FOV angles must stay strictly inside `(-pi/2,+pi/2)` with ordered finite
  tangents.
- The conservative center traversal transforms both translated/canted eye
  frusta into the center frame and outward-rounds all bounds.
- The exact programmable WVP lane computes `D = E C^-1` in double precision,
  checks inverse/reconstruction residuals, and applies `D O` only to a verified
  column-vector contract `O = C M`. The final float `D O` upload is also checked
  against the retained double-precision delta, so a large draw-local model
  matrix cannot silently amplify a small camera-factor error.
- Publication now compares the final post-traversal center world-to-camera and
  frustum against the factors captured before traversal.

## Why the verdict remains red

Historical logs show the black screen was deterministic. An unverified shader
reduced contract coverage to `391/392`, the producer stopped publishing, and
the configured no-fallback host eventually submitted no visible gameplay
layer. The missing shader bytecode has not been captured.

Before headset use, the project still needs:

1. A complete exact-bytecode contract set that passes the new conservative
   `xyzw` def-use trace to `oPos`, including the missing and legacy-rejected
   shader forms.
2. Enumerate any remaining final-target write APIs and add them to the new
   fail-closed draw/clear/copy ledgers, with reviewed eye-invariant exclusions.
3. Strong SHA-256 plus byte-length identities and semantic evidence for any
   configured skip.
4. Producer-only evidence that pixels stay inside the source-pose age budget.
5. External slicer validation of the generated STLs.

## Reproducible artifacts

Run:

```powershell
& 'C:\Users\nbrys\.cache\codex-runtimes\codex-primary-runtime\dependencies\python\python.exe' `
  .\tools\generate_6dof_proof_artifacts.py
```

The deterministic generator emits:

- `output/proof/fnvxr_6dof_numeric_proof.json`
- `output/3d/fixture_manifest.json`
- six binary STL print candidates and a fixture preview under `output/3d`
- `output/pdf/fnvxr_6dof_coordinate_proof.pdf`

The fixture is an asymmetric metrology reference, not runtime proof. Its STL
coordinates are millimeters and map printer `(X,Y,Z)` to OpenXR `(X,-Z,Y)`.
The mast flange bottoms on the base top to define the assembled height datum;
external slicer validation is still required before calling the parts
print-ready.
