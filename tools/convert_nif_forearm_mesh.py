#!/usr/bin/env python3
"""Bake the installed retail male left forearm into a host-local mesh.

The output is a private, locally derived artifact. It contains expanded
position/normal/UV vertices only and must never be committed or redistributed.
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
        return (0.0, 1.0, 0.0)
    return tuple(component / length for component in value)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--input", required=True)
    parser.add_argument("--output", required=True)
    parser.add_argument("--shape-name", default="Arms:1")
    parser.add_argument("--units-per-meter", type=float, default=70.0)
    args = parser.parse_args()

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
    shapes = [
        block
        for block in data.blocks
        if isinstance(block, NifFormat.NiTriShape)
        and bytes(block.name).decode("utf-8", "replace") == args.shape_name
    ]
    if len(shapes) != 1:
        raise SystemExit(f"expected one '{args.shape_name}' shape, found {len(shapes)}")
    shape = shapes[0]
    if not shape.skin_instance or not shape.skin_instance.data:
        raise SystemExit("forearm geometry is not skinned")
    if not shape.data or shape.data.num_uv_sets < 1:
        raise SystemExit("forearm geometry has no diffuse UV set")

    bone_names = [bytes(bone.name) for bone in shape.skin_instance.bones]
    required_names = (
        b"Bip01 L Hand",
        b"Bip01 L Forearm",
        b"Bip01 L ForeTwist",
        b"Bip01 L UpperArm",
        b"Bip01 LUpArmTwistBone",
    )
    if any(name not in bone_names for name in required_names):
        raise SystemExit("retail upper body is missing required left-arm bones")
    left_bone_indices = [bone_names.index(name) for name in required_names]
    forearm = shape.skin_instance.bones[bone_names.index(b"Bip01 L Forearm")]
    hand = shape.skin_instance.bones[bone_names.index(b"Bip01 L Hand")]
    skeleton_root = shape.skin_instance.skeleton_root

    hand_to_forearm = (
        hand.get_transform(skeleton_root)
        * forearm.get_transform(skeleton_root).get_inverse()
    )
    hand_in_forearm = hand_to_forearm.get_translation()
    forearm_length = float(hand_in_forearm.x)
    if not math.isfinite(forearm_length) or not 8.0 <= forearm_length <= 40.0:
        raise SystemExit("retail left forearm length is invalid")

    vertex_weights: list[dict[int, float]] = [
        {} for _ in range(shape.data.num_vertices)
    ]
    for bone_index, skin_data in enumerate(shape.skin_instance.data.bone_list):
        for weighted_vertex in skin_data.vertex_weights:
            vertex_weights[int(weighted_vertex.index)][bone_index] = float(
                weighted_vertex.weight
            )

    deformed_vertices, deformed_normals = shape.get_skin_deformation()
    geometry_to_forearm = (
        shape.skin_instance.data.get_transform().get_inverse()
        * forearm.get_transform(skeleton_root).get_inverse()
    )
    _scale, geometry_to_forearm_rotation, _translation = (
        geometry_to_forearm.get_scale_rotation_translation()
    )
    forearm_vertices = [
        vertex * geometry_to_forearm for vertex in deformed_vertices
    ]
    forearm_normals = [
        normal * geometry_to_forearm_rotation for normal in deformed_normals
    ]
    diffuse_uvs = shape.data.uv_sets[0]

    # The retail forearm bone's +X axis runs elbow-to-wrist. Select only the
    # connected left-arm skin band ending at that wrist, then rotate +X onto
    # host segment +Z. drawSolvedArm already supplies the tracked elbow/wrist
    # orientation, so no guessed arm transform remains in the mesh.
    selected_triangles = []
    for triangle in shape.data.get_triangles():
        centroid = tuple(
            sum(float(getattr(forearm_vertices[int(index)], axis)) for index in triangle)
            / 3.0
            for axis in ("x", "y", "z")
        )
        left_weight = sum(
            sum(
                vertex_weights[int(index)].get(bone_index, 0.0)
                for bone_index in left_bone_indices
            )
            for index in triangle
        ) / 3.0
        if (
            -2.0 <= centroid[0] <= forearm_length + 1.0
            and math.hypot(centroid[1], centroid[2]) <= 8.0
            and left_weight >= 0.5
        ):
            selected_triangles.append(triangle)

    expanded: list[tuple[float, float, float, float, float, float, float, float]] = []
    half_length = forearm_length * 0.5
    for triangle in selected_triangles:
        for index in triangle:
            vertex = forearm_vertices[int(index)]
            normal = forearm_normals[int(index)]
            # Proper, non-reflecting basis: NIF forearm (x,y,z) -> host
            # segment (y,z,x), centered on the elbow/wrist midpoint.
            position = (
                float(vertex.y) / args.units_per_meter,
                float(vertex.z) / args.units_per_meter,
                (float(vertex.x) - half_length) / args.units_per_meter,
            )
            mapped_normal = normalized(
                (float(normal.y), float(normal.z), float(normal.x))
            )
            uv = diffuse_uvs[int(index)]
            mapped_uv = (float(uv.u), float(uv.v))
            if not all(math.isfinite(value) for value in (*position, *mapped_uv)):
                raise SystemExit("retail forearm produced non-finite geometry")
            expanded.append((*position, *mapped_normal, *mapped_uv))

    if not 300 <= len(expanded) <= 30_000 or len(expanded) % 3:
        raise SystemExit(f"unexpected expanded forearm vertex count: {len(expanded)}")
    output.parent.mkdir(parents=True, exist_ok=True)
    payload = bytearray(struct.pack("<4sIII", b"FHM2", 2, len(expanded), 0))
    for vertex in expanded:
        payload.extend(struct.pack("<8f", *vertex))
    output.write_bytes(payload)
    print(
        f"{output}\tvertices={len(expanded)}\t"
        f"forearm_units={forearm_length:.6f}\t"
        f"source_sha256={hashlib.sha256(source.read_bytes()).hexdigest()}\t"
        f"output_sha256={hashlib.sha256(payload).hexdigest()}"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
