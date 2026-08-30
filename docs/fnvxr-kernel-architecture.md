# FNVXR product architecture

FNVXR now has one platform-neutral product policy entry point:
`fnvxr::kernel::ProductKernel`. The OpenXR host translates external state into
typed snapshots, asks the kernel for a presentation decision, and executes that
decision. Win32, OpenXR, D3D, environment parsing, and Gamebryo records do not
belong in the kernel.

## Runtime flow

1. `fnvxr_runtime_config_loader` reads the process environment once and returns
   a `ValidatedRuntimeConfig`. Invalid or empty values stop startup before
   OpenXR is touched.
2. The host publishes immutable pose/view samples into `ExactFrameJoiner<128>`.
   An image can join only its exact producer epoch, reference-space generation,
   pose sequence, and OpenXR display time. There is no nearest/last-pose
   fallback.
3. `ProductKernel` evaluates runtime, image, GPU-ownership, retail-render,
   stereo-identity, UI, and pose-join evidence. Missing evidence selects the
   safety blank; the adapter cannot override that decision.
4. The OpenXR spatial adapter feeds exact historical poses to the deterministic
   arm solver and wrist-surface kernel. Invalid or untracked inputs produce no
   rig or wrist surface.
5. Both eyes render from the same accepted image transaction and source-pose
   generation. The production path does not map a D3D staging texture or wait
   for a per-eye CPU readback. Readback exists only for explicit diagnostics or
   mirror capture.
6. `CadenceTracker` records frame work time, observed host FPS, frame-budget
   overruns, fresh/repeated/stale image transactions, and left/right/pair drops
   in constant time and fixed memory. The host emits sampled submit telemetry
   and one final `fnvxrCadenceSummary` record.

## Layers

- `kernel/config_*`: typed configuration and cross-field validation.
- `kernel/exact_frame_joiner.h`: bounded O(1) image/pose lineage join.
- `kernel/presentation`: fail-closed product presentation state machine.
- `kernel/rig`: deterministic two-bone first-person arm solve.
- `kernel/wrist`: coordinate contract, grip-local placement, and activation
  hysteresis.
- `kernel/performance`: monotonic, saturation-safe cadence/drop accounting.
- `host/fnvxr_*_adapter.*`: OpenXR/protocol translation only.
- `host/fnvxr_openxr_pose_host.cpp`: remaining orchestration and D3D execution.
- `plugin/` and `renderhook/`: x86 retail-engine integration and GPU producer.

All kernel operations use fixed storage or scalar state. Pose lookup, frame
accounting, presentation decisions, arm solves, and wrist placement are O(1).

## Non-negotiable invariants

- One validated configuration snapshot per process.
- One image transaction for both submitted eyes.
- Exact source-pose generation; no current-pose overlay fallback.
- Spatial props render only from tracked, finite, exact historical poses.
- UI source watermarks cannot regress or resurrect expired content.
- Reference-space/session/producer changes reset dependent histories together.
- No synchronous per-eye GPU-to-CPU readback on the product route.

## Remaining extraction debt

The OpenXR host, NVSE plugin, and D3D9 proxy still contain legacy orchestration
and diagnostic paths and remain larger than the target architecture permits.
The next safe cuts are the host input/publication loop, swapchain renderer,
retail runtime-evidence producer, and D3D9 publication transaction. Those cuts
must preserve the kernel contracts above and keep the product route green while
the old diagnostic routes are retired.

Unit tests prove deterministic policy and adapter behavior on x64 and Win32.
Actual headset FPS and final-eye appearance remain physical-runtime acceptance
measurements; the cadence stream now makes those measurements explicit instead
of inferred from ad hoc logs.
