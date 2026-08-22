#!/usr/bin/env python3
"""Convert an installed retail first-person hand NIF to a tiny host mesh.

The output contains expanded position/normal/UV triangle vertices only. It is
a local derived artifact and must not be committed or redistributed with FNVXR.
"""

from __future__ import annotations

import argparse
import hashlib
import math
import struct
import time
from pathlib import Path


FLOAT_SENTINEL_LIMIT = 1.0e30


def normalized(value: tuple[float, float, float]) -> tuple[float, float, float]:
    length = math.sqrt(sum(component * component for component in value))
    if not math.isfinite(length) or length < 1.0e-7:
        return (0.0, 0.0, 1.0)
    return tuple(component / length for component in value)


def valid_scalar(value: float) -> bool:
    return math.isfinite(float(value)) and abs(float(value)) < FLOAT_SENTINEL_LIMIT


def quaternion_rotation(NifFormat, quaternion):
    """Return PyFFI's row-vector Matrix33 for a normalized NIF quaternion."""

    values = tuple(
        float(value)
        for value in (quaternion.w, quaternion.x, quaternion.y, quaternion.z)
    )
    length = math.sqrt(sum(value * value for value in values))
    if not math.isfinite(length) or length < 1.0e-7:
        raise ValueError("animation produced an invalid quaternion")
    w, x, y, z = (value / length for value in values)
    result = NifFormat.Matrix33()
    result.m_11 = 1.0 - 2.0 * (y * y + z * z)
    result.m_12 = 2.0 * (x * y + z * w)
    result.m_13 = 2.0 * (x * z - y * w)
    result.m_21 = 2.0 * (x * y - z * w)
    result.m_22 = 1.0 - 2.0 * (x * x + z * z)
    result.m_23 = 2.0 * (y * z + x * w)
    result.m_31 = 2.0 * (x * z + y * w)
    result.m_32 = 2.0 * (y * z - x * w)
    result.m_33 = 1.0 - 2.0 * (x * x + y * y)
    return result


def nearest_key(keys, sample_time: float):
    if not keys:
        return None
    return min(keys, key=lambda key: abs(float(key.time) - sample_time))


def apply_animation_pose(
    NifFormat,
    hand_data,
    skeleton_path: Path,
    animation_path: Path,
    sample_time: float,
) -> int:
    """Bake an authored first-person KF pose into the stripped hand skeleton.

    Retail hand NIFs keep the exact skin bones as root children. The full
    first-person skeleton retains their real hierarchy (including Hand01-04),
    so the KF is evaluated there and the resulting hand-space bone matrices
    are copied back before PyFFI performs linear-blend skinning.
    """

    skeleton_data = NifFormat.Data()
    with skeleton_path.open("rb") as stream:
        skeleton_data.read(stream)
    animation_data = NifFormat.Data()
    with animation_path.open("rb") as stream:
        animation_data.read(stream)
    sequences = [
        root
        for root in animation_data.roots
        if isinstance(root, NifFormat.NiControllerSequence)
    ]
    if len(sequences) != 1:
        raise ValueError(
            f"expected one NiControllerSequence, found {len(sequences)}"
        )
    sequence = sequences[0]
    if not math.isfinite(sample_time):
        raise ValueError("--animation-time must be finite")
    sample_time = min(
        max(sample_time, float(sequence.start_time)),
        float(sequence.stop_time),
    )

    skeleton_nodes = {
        bytes(block.name): block
        for block in skeleton_data.blocks
        if isinstance(block, NifFormat.NiNode)
    }
    applied = 0
    for link in sequence.controlled_blocks:
        node_name = bytes(link.get_node_name())
        node = skeleton_nodes.get(node_name)
        interpolator = link.interpolator
        if node is None or not isinstance(interpolator, NifFormat.NiTransformInterpolator):
            continue

        scale, rotation, translation = (
            node.get_transform().get_scale_rotation_translation()
        )
        if valid_scalar(interpolator.scale):
            scale = float(interpolator.scale)
        if all(
            valid_scalar(value)
            for value in (
                interpolator.translation.x,
                interpolator.translation.y,
                interpolator.translation.z,
            )
        ):
            translation = interpolator.translation
        if all(
            valid_scalar(value)
            for value in (
                interpolator.rotation.w,
                interpolator.rotation.x,
                interpolator.rotation.y,
                interpolator.rotation.z,
            )
        ):
            rotation = quaternion_rotation(NifFormat, interpolator.rotation)

        transform_data = interpolator.data
        if transform_data is not None:
            if transform_data.num_rotation_keys:
                if int(transform_data.rotation_type) != 4:
                    key = nearest_key(transform_data.quaternion_keys, sample_time)
                    if key is not None:
                        rotation = quaternion_rotation(NifFormat, key.value)
                # The hand-grip clip stores finger rotations as quaternion
                # keys. Preserve the skeleton value for unrelated XYZ tracks.
            translation_key = nearest_key(
                transform_data.translations.keys, sample_time
            )
            if translation_key is not None:
                translation = translation_key.value
            scale_key = nearest_key(transform_data.scales.keys, sample_time)
            if scale_key is not None:
                scale = float(scale_key.value)

        local_transform = NifFormat.Matrix44()
        local_transform.set_scale_rotation_translation(scale, rotation, translation)
        node.set_transform(local_transform)
        applied += 1

    if applied == 0:
        raise ValueError("animation did not target any skeleton nodes")

    skeleton_root = skeleton_data.roots[0]
    hand_nodes = {
        bytes(block.name): block
        for block in hand_data.blocks
        if isinstance(block, NifFormat.NiNode)
    }
    copied = 0
    for node_name, hand_node in hand_nodes.items():
        skeleton_node = skeleton_nodes.get(node_name)
        if skeleton_node is None or hand_node is hand_data.roots[0]:
            continue
        hand_node.set_transform(skeleton_node.get_transform(skeleton_root))
        copied += 1
    if copied < 10:
        raise ValueError(
            f"animation skeleton matched only {copied} stripped hand bones"
        )
    return copied


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--input", required=True)
    parser.add_argument("--output", required=True)
    parser.add_argument("--side", choices=("left", "right"), required=True)
    parser.add_argument(
        "--shape-name",
        help="exact NiTriShape name when the retail NIF contains material parts",
    )
    parser.add_argument("--units-per-meter", type=float, default=70.0)
    parser.add_argument(
        "--skeleton",
        help="full retail first-person skeleton used to evaluate --animation",
    )
    parser.add_argument(
        "--animation",
        help="retail KF pose baked into the output mesh",
    )
    parser.add_argument("--animation-time", type=float, default=0.0)
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
    if bool(args.skeleton) != bool(args.animation):
        raise SystemExit("--skeleton and --animation must be provided together")
    posed_bones = 0
    if args.skeleton and args.animation:
        if args.side != "right":
            raise SystemExit("the supplied pistol grip animation is right-hand only")
        try:
            posed_bones = apply_animation_pose(
                NifFormat,
                data,
                Path(args.skeleton).resolve(),
                Path(args.animation).resolve(),
                args.animation_time,
            )
        except ValueError as error:
            raise SystemExit(str(error)) from error
    shapes = [
        block
        for block in data.blocks
        if isinstance(block, NifFormat.NiTriShape)
        and (
            not args.shape_name
            or bytes(block.name).decode("utf-8", "replace")
                == args.shape_name
        )
    ]
    if len(shapes) != 1:
        raise SystemExit(f"expected one NiTriShape, found {len(shapes)}")
    shape = shapes[0]
    if not shape.skin_instance or not shape.skin_instance.data:
        raise SystemExit("hand geometry is not skinned")
    if not shape.data or shape.data.num_uv_sets < 1:
        raise SystemExit("hand geometry has no diffuse UV set")

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

    diffuse_uvs = shape.data.uv_sets[0]
    expanded: list[tuple[float, float, float, float, float, float, float, float]] = []
    for triangle in shape.data.get_triangles():
        for index in triangle:
            vertex = hand_vertices[index]
            normal = hand_normals[index]
            # NIF hand-local +X runs wrist-to-fingertip. OpenXR grip-local -Z
            # is controller-forward, +Y is up, and +X is right. Rotate the
            # retail hand into that basis (without a reflection) so it follows
            # the runtime grip pose instead of hanging downward from it.
            position = (
                float(vertex.y) / args.units_per_meter,
                -float(vertex.z) / args.units_per_meter,
                -float(vertex.x) / args.units_per_meter,
            )
            mapped_normal = normalized(
                (float(normal.y), -float(normal.z), -float(normal.x))
            )
            uv = diffuse_uvs[index]
            if not all(math.isfinite(value) for value in position):
                raise SystemExit("NIF produced a non-finite hand vertex")
            mapped_uv = (float(uv.u), float(uv.v))
            if not all(math.isfinite(value) for value in mapped_uv):
                raise SystemExit("NIF produced a non-finite hand UV")
            expanded.append((*position, *mapped_normal, *mapped_uv))

    if not 300 <= len(expanded) <= 100_000 or len(expanded) % 3:
        raise SystemExit(f"unexpected expanded vertex count: {len(expanded)}")
    output.parent.mkdir(parents=True, exist_ok=True)
    payload = bytearray(struct.pack("<4sIII", b"FHM2", 2, len(expanded), 0))
    for vertex in expanded:
        payload.extend(struct.pack("<8f", *vertex))
    output.write_bytes(payload)
    print(
        f"{output}\tvertices={len(expanded)}\t"
        f"posed_bones={posed_bones}\t"
        f"source_sha256={hashlib.sha256(source.read_bytes()).hexdigest()}\t"
        f"output_sha256={hashlib.sha256(payload).hexdigest()}"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
