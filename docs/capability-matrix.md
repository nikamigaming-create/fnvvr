# FNVVR Phase 0 capability matrix

Audit baseline: `6faed4620d029803ceb129487ebacb64237111ef` on 2026-08-01.

`Partial` means a bounded implementation or evidence slice exists; it is not a
release claim. `Yes` in the test columns means a retained automated proof for
that bounded slice exists. Every `Production-authorized` cell remains `No`
until the capability's phase gate and the final aggregate release gate pass.

| Capability | Implemented | Unit-tested | Simulator-tested | Physically-tested | Production-authorized | Current boundary |
|---|---|---|---|---|---|---|
| `CameraStereoVisual` | Partial | Yes | Yes | No | No | CPU-v8 engine-center visual trial proves simulator motion only; physical body/head behavior is unproven. |
| `ControllerInput` | Partial | Yes | Partial | No | No | The xNVSE main-game-loop consumer exists, but the release semantic map and physical action matrix are incomplete. |
| `UiInteraction` | Partial | Yes | Partial | No | No | Current mono capture/basic pointer work is not the complete retail menu matrix. |
| `VisualRig` | Partial | Yes | No | No | No | Rig discovery/FABRIK and a visual-only assist seam exist; they do not authorize weapon mutation. |
| `CombatAim` | No | Partial | No | No | No | Only read-only weapon/projectile observations exist; no retail firing seam is authorized. |
| `GpuPresentation` | Partial | Yes | No | No | No | Color ABI v5 contracts and components exist; a live supported producer/consumer recovery proof does not. |
| `FullProduct` | No | No | No | No | No | Only a complete aggregate of the six narrow capabilities plus release qualification can claim this state. |

## Checked invariants

- `protocol/fnvxr_product_capabilities.h` selects
  `InputOwner::NvseMainGameLoop` as the sole product-side input owner. The
  DirectInput and XInput proxy paths remain transparent production paths.
- `fnvxr_product_capabilities_test` checks that every individual lease needs
  current loaded-image, function-byte, ancestry, compatibility, producer,
  pose, runtime, mode, concrete lineage identities, and capability-specific
  evidence; it also checks that `FullProduct` aggregates all six leases and
  release qualification. Each mutation lease retains its own exact lineage;
  UI and gameplay are not incorrectly required to be concurrent modes.
- `fnvxr_phase0_records` checks this matrix, ADR 0001, the mutation inventory,
  and the color-v5/render-local-depth naming boundary together.

This is a status record, not an authorization input. Runtime code must still
perform its own synchronous current-process validation at each mutation site.
