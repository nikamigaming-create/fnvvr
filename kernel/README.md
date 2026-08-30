# FNVXR product kernel

This directory is the platform-neutral application core. Its dependency rule
is one-way:

`plugin / renderhook / host -> kernel -> C++ standard library`

The kernel owns validated runtime configuration, immutable frame identity and
pose/view values, bounded O(1) history, and presentation policy. It does not
read environment variables, call Win32/OpenXR/D3D, import retail assets, or
serialize shared memory. Those are adapter responsibilities.

`ProductKernel` is the single policy entry point for the live GPU-v5 host route.
The compatibility shape in `protocol/fnvxr_product_contract.h` is translated by
`host/fnvxr_product_kernel_adapter.*`; it no longer owns the host decision.

Live host boundaries are deliberately separate:

- `fnvxr_runtime_config_loader.*` reads supported process configuration once.
- `fnvxr_source_pose_history.h` translates OpenXR values into `ExactFrameJoiner`.
- `fnvxr_runtime_evidence_history.h` provides direct O(1) sample lookup.
- `fnvxr_tracking_lifetime.h` owns session/reference-space generations.

Spatial arm, weapon, and wrist rendering is authorized only when the submitted
image resolves its exact producer epoch, reference-space generation, pose
sequence, and OpenXR display time. Missing lineage produces no spatial overlay.
