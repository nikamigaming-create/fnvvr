# FNVXR product architecture

The product route now has one platform-neutral transport authority entry point:
`fnvxr::kernel::transport::resolvePresentationAuthority`. Each immutable
runtime snapshot selects exactly one policy owner: the GPU `ProductKernel`, the
CPU engine-center policy, or fail-closed `None`. The selected policy must still
prove its own pixels and exact frame lineage; authority selection never turns
configuration into image evidence. Win32, OpenXR, D3D, environment parsing,
and Gamebryo records do not belong in the kernel.

## Runtime flow

1. `fnvxr_runtime_config_loader` reads the supported typed runtime settings once
   and returns a `ValidatedRuntimeConfig`. Invalid or empty values stop startup
   before OpenXR is touched. Legacy diagnostic settings remain extraction debt.
2. The host publishes immutable pose/view samples into `ExactFrameJoiner<128>`.
   An image can join only its exact producer epoch, reference-space generation,
   pose sequence, and OpenXR display time. There is no nearest/last-pose
   fallback.
3. The transport authority resolver normalizes the runtime/menu state and
   selects the sole policy owner for that snapshot. Unknown, stale,
   contradictory, or unsupported input selects no owner. `ProductKernel`
   evaluates runtime, image, GPU-ownership, retail-render,
   stereo-identity, UI, and pose-join evidence. Missing evidence selects the
   safety blank on the product-authorized GPU route; the adapter cannot
   override that decision. The unproven GPU visual-trial path is disabled by
   default and can run only behind its explicit diagnostic switch.
4. The OpenXR spatial adapter feeds exact historical poses to the deterministic
   arm solver and wrist-surface kernel. Authored grip and Pip-Boy calibration
   uses a bounded exact-identity history keyed by producer epoch, reference
   generation, and pose sequence. Invalid or untracked inputs produce no rig or
   wrist surface.
   Retained Pip-Boy pixels pass a separate `ContentReadiness` gate keyed by a
   monotonic resource generation, producer process, renderer epoch, acceptance
   time, source runtime sample, stable source/current lineage, and current
   world-continuation state. Producer/renderer transitions immediately clear
   retained content. A valid texture object alone cannot authorize the wrist
   screen. Pixel acceptance also requires non-uniform authored structure;
   solid dark/green placeholders and weakened environment thresholds fail.
5. Both eyes render from the same accepted image transaction and source-pose
   generation. The production path does not map a D3D staging texture or wait
   for a per-eye CPU readback. Readback exists only for explicit diagnostics or
   mirror capture.
6. `CadenceTracker` records frame work time, observed host FPS, frame-budget
   overruns, fresh/repeated/stale image transactions, and left/right/pair drops
   in constant time and fixed memory. Host-loop and render-requested work/rate
   metrics remain separate, so suppressed frames cannot improve the reported
   render cadence. The host emits sampled submit telemetry
   and one final `fnvxrCadenceSummary` record. Runtime-not-requested zero-layer
   frames and failed `xrEndFrame` calls have distinct terminal dispositions, so
   neither fabricates an application eye drop. `deriveCadencePerformance`
   derives host, requested-frame, and completed-pair rates in one tested place;
   per-submit telemetry and the final summary use the same math. The host also
   emits `fnvxrFrameJoinSummary` with bounded-history occupancy, overwrites,
   exact lookup attempts, accepted presented exact joins, misses,
   epoch/generation failures, and time mismatches.

## Layers

- `kernel/config_*`: typed configuration and cross-field validation.
- `kernel/exact_frame_joiner.h`: bounded O(1) image/pose lineage join.
- `kernel/spatial_calibration_history.h`: bounded O(1) authored-transform join.
- `kernel/presentation`: fail-closed product presentation state machine.
- `kernel/transport`: sole per-snapshot presentation-policy authority.
- `kernel/rig`: deterministic two-bone first-person arm solve.
- `kernel/wrist`: coordinate contract, grip-local placement, activation
  hysteresis, and retained-content readiness.
- `kernel/performance`: monotonic, saturation-safe cadence/drop accounting and
  canonical derived performance metrics.
- `host/fnvxr_*_adapter.*`: OpenXR/protocol translation only.
- `host/fnvxr_openxr_pose_host.cpp`: remaining orchestration and D3D execution.
- `plugin/` and `renderhook/`: x86 retail-engine integration and GPU producer.

All kernel operations use fixed storage or scalar state. Pose lookup, frame
accounting, presentation decisions, arm solves, and wrist placement are O(1).

## Non-negotiable invariants

- One validated configuration snapshot per process.
- One presentation-policy owner per immutable runtime snapshot.
- One image transaction for both submitted eyes.
- Exact source-pose generation; no current-pose overlay fallback.
- Spatial props render only from tracked, finite, exact historical poses.
- UI source watermarks cannot regress or resurrect expired content.
- Retained wrist pixels require a nonzero resource generation, bounded age, and
  stable source-to-current runtime lineage.
- Reference-space/session/producer changes reset dependent histories together.
- No synchronous per-eye GPU-to-CPU readback on the product route.

Pixel-verification runs deliberately enable synchronous readback and therefore
measure diagnostic overhead, not shipping cadence. Production performance must
be measured with verification disabled; mirror captures remain pre-scheduled
and bounded.

## Remaining extraction debt

The OpenXR host, NVSE plugin, and D3D9 proxy still contain legacy orchestration
and diagnostic paths and remain larger than the target architecture permits.
The CPU and GPU policies now share one authority boundary, but their
transport-specific evidence evaluators remain separate by design. The next
safe cuts are the host input/publication loop, swapchain renderer, legacy
environment-option reads, retail runtime-evidence producer, and D3D9
publication transaction. Those cuts
must preserve the kernel contracts above and keep the product route green while
the old diagnostic routes are retired.

Unit tests prove deterministic policy and adapter behavior on x64 and Win32.
Actual headset FPS and final-eye appearance remain physical-runtime acceptance
measurements; the cadence stream now makes those measurements explicit instead
of inferred from ad hoc logs.
