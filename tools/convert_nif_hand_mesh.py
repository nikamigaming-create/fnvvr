#!/usr/bin/env python3
"""Convert an installed retail first-person hand NIF to a tiny host mesh.

The output contains expanded triangle vertices only. It is a local derived
artifact and must not be committed or redistributed with FNVXR.
"""

from __future__ import annotations

import argparse
import hashlib
import math
import struct
import time
from pathlib import Path


def normalized(value: tuple[float, float, float]) -> tuple[float, float, float]:
    length = math.sqrt(sum(component * component for component in value))
    if not math.isfinite(length) or length < 1.0e-7:
        return (0.0, 0.0, 1.0)
    return tuple(component / length for component in value)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--input", required=True)
    parser.add_argument("--output", required=True)
    parser.add_argument("--side", choices=("left", "right"), required=True)
    parser.add_argument("--units-per-meter", type=float, default=70.0)
    args = parser.parse_args()

    # PyFFI 2.2 predates Python 3.8's removal of time.clock.
    if not hasattr(time, "clock"):
        time.clock = time.perf_counter  # type: ignore[attr-defined]
    from pyffi.formats.nif import NifFormat

    source = Path(args.input).resolve()
    output = Path(args.output).resolve()
    if not math.isfinite(args.units_per_meter) or args.units_per_meter <= 0.0:
        raise SystemExit("--units-per-meter must be positive")

    data = NifFormat.Data()
    with source.open("rb") as stream:
        data.read(stream)
    shapes = [block for block in data.blocks if isinstance(block, NifFormat.NiTriShape)]
    if len(shapes) != 1:
        raise SystemExit(f"expected one NiTriShape, found {len(shapes)}")
    shape = shapes[0]
    if not shape.skin_instance or not shape.skin_instance.data:
        raise SystemExit("hand geometry is not skinned")

    side_token = b" L Hand" if args.side == "left" else b" R Hand"
    hand_bones = [
        block
        for block in data.blocks
        if isinstance(block, NifFormat.NiNode)
        and side_token in bytes(getattr(block, "name", b""))
        and bytes(getattr(block, "name", b"")).endswith(b"Hand")
    ]
    if len(hand_bones) != 1:
        raise SystemExit(f"expected one wrist hand bone, found {len(hand_bones)}")
    hand_bone = hand_bones[0]
    skeleton_root = shape.skin_instance.skeleton_root

    deformed_vertices, deformed_normals = shape.get_skin_deformation()
    geometry_to_hand = (
        shape.skin_instance.data.get_transform().get_inverse()
        * hand_bone.get_transform(skeleton_root).get_inverse()
    )
    _scale, geometry_to_hand_rotation, _translation = (
        geometry_to_hand.get_scale_rotation_translation()
    )
    hand_vertices = [vertex * geometry_to_hand for vertex in deformed_vertices]
    hand_normals = [normal * geometry_to_hand_rotation for normal in deformed_normals]

    expanded: list[tuple[float, float, float, float, float, float]] = []
    for triangle in shape.data.get_triangles():
        for index in triangle:
            vertex = hand_vertices[index]
            normal = hand_normals[index]
            # NIF hand-local +X runs wrist-to-fingertip. Host hand-local -Y
            # is forward/down in its controller model, while +X remains the
            # palm-width axis and +Z is palm thickness.
            position = (
                float(vertex.y) / args.units_per_meter,
                -float(vertex.x) / args.units_per_meter,
                float(vertex.z) / args.units_per_meter,
            )
            mapped_normal = normalized(
                (float(normal.y), -float(normal.x), float(normal.z))
            )
            if not all(math.isfinite(value) for value in position):
                raise SystemExit("NIF produced a non-finite hand vertex")
            expanded.append((*position, *mapped_normal))

    if not 300 <= len(expanded) <= 100_000 or len(expanded) % 3:
        raise SystemExit(f"unexpected expanded vertex count: {len(expanded)}")
    output.parent.mkdir(parents=True, exist_ok=True)
    payload = bytearray(struct.pack("<4sIII", b"FHM1", 1, len(expanded), 0))
    for vertex in expanded:
        payload.extend(struct.pack("<6f", *vertex))
    output.write_bytes(payload)
    print(
        f"{output}\tvertices={len(expanded)}\t"
        f"source_sha256={hashlib.sha256(source.read_bytes()).hexdigest()}\t"
        f"output_sha256={hashlib.sha256(payload).hexdigest()}"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
