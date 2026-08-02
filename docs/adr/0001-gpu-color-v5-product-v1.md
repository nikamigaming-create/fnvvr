# ADR 0001: Product GPU transport v1 uses color ABI v5 and render-local depth

Status: Accepted

Date: 2026-08-01

## Context

The checked-in GPU color ABI v5 deliberately publishes two shared color
textures, completion/release fences, adapter identity, and complete frame
lineage. It deliberately does not publish depth. Older architecture text and
the v4 GPU-frame research ABI instead described shared encoded-depth textures
and OpenXR depth-layer submission as product requirements. Treating both as
the active product contract would make a valid v5 producer appear incomplete
or tempt an implementation to fabricate a depth resource in the host.

## Decision

Product v1 uses `protocol/fnvxr_gpu_color_transport.h` ABI v5 as the only
production GPU transport. It has these non-negotiable properties:

- The producer shares only a non-aliased left/right color pair, matching
  adapter identity, producer/process/epoch lineage, exact retail-frame and
  pose/runtime identities, and one observed GPU completion/release interval.
- Each retail eye still owns an isolated depth/stencil target. Those targets
  are required to be complete, distinct, and validated before publication, but
  they remain render-local. The product proof calls this
  `renderLocalDepthPairComplete` to prevent it from being mistaken for a
  host-visible texture or an OpenXR submission claim.
- The OpenXR host copies the accepted color pair entirely on the GPU and
  submits color projection views. OpenXR depth composition-layer submission is
  not a v1 release gate.
- CPU-v8 remains a bounded visual-trial transport only. It is never a fallback
  for a failed v5 producer, fence, adapter, or resource-open operation.

`protocol/fnvxr_gpu_frame_transport.h` v4 and its encoded-depth helpers remain
available only for compatibility/research tests. They are not an alternate
product route and may not be mixed with v5 metadata in one transaction.

## Consequences

The product gates retain full per-eye render-depth validation, but no longer
require exported depth handles, encoded-depth copies, or OpenXR depth layers.
The renderer, product proof, and retail mutation evidence use the explicit
`renderLocalDepthPairComplete` name. Any future desire to submit depth requires
a new ABI, a new ADR, and independent color/depth ownership, fence, recovery,
and compositor tests; it must not reinterpret ABI v5.

This decision does not authorize product presentation. The v5 route, physical
headset evidence, first-person coverage, input, combat, compatibility, and
release qualification remain separately fail-closed.
