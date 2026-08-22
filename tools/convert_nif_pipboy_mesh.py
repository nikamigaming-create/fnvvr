#!/usr/bin/env python3
"""Convert the installed retail Pip-Boy housing into a screen-local mesh.

The output is a local derived artifact. It contains no texture pixels and must
not be committed or redistributed with FNVXR.
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
    parser.add_argument(
        "--screen-output",
        help="optional actual pipboyscreen geometry with live-retail crop UVs",
    )
    parser.add_argument("--housing-name", default="PipBoyArm:0")
    parser.add_argument("--screen-name", default="pipboyscreen:0")
    args = parser.parse_args()

    if not hasattr(time, "clock"):
        time.clock = time.perf_counter  # type: ignore[attr-defined]
    from pyffi.formats.nif import NifFormat

    source = Path(args.input).resolve()
    output = Path(args.output).resolve()
    data = NifFormat.Data()
    with source.open("rb") as stream:
        data.read(stream)
    if len(data.roots) != 1:
        raise SystemExit(f"expected one Pip-Boy root, found {len(data.roots)}")
    root = data.roots[0]
    geometries = [
        block for block in data.blocks
        if isinstance(block, NifFormat.NiGeometry) and block.data
    ]
    housing = [
        block for block in geometries
        if bytes(block.name).decode("utf-8", "replace") == args.housing_name
    ]
    screen = [
        block for block in geometries
        if bytes(block.name).decode("utf-8", "replace") == args.screen_name
    ]
    if len(housing) != 1 or len(screen) != 1:
        raise SystemExit(
            f"expected one housing and screen, found {len(housing)}/{len(screen)}"
        )
    housing = housing[0]
    screen = screen[0]
    if housing.data.num_uv_sets < 1:
        raise SystemExit("Pip-Boy housing has no diffuse UV set")

    housing_to_root = housing.get_transform(root)
    screen_to_root = screen.get_transform(root)
    housing_to_screen = housing_to_root * screen_to_root.get_inverse()
    _scale, housing_to_screen_rotation, _translation = (
        housing_to_screen.get_scale_rotation_translation()
    )
    screen_vertices = list(screen.data.vertices)
    screen_width = max(float(v.x) for v in screen_vertices) - min(
        float(v.x) for v in screen_vertices
    )
    screen_height = max(float(v.y) for v in screen_vertices) - min(
        float(v.y) for v in screen_vertices
    )
    if screen_width <= 0.001 or screen_height <= 0.001:
        raise SystemExit("Pip-Boy screen bounds are invalid")
    screen_min_x = min(float(v.x) for v in screen_vertices)
    screen_max_x = max(float(v.x) for v in screen_vertices)
    screen_min_y = min(float(v.y) for v in screen_vertices)
    screen_max_y = max(float(v.y) for v in screen_vertices)
    screen_min_z = min(float(v.z) for v in screen_vertices)
    screen_max_z = max(float(v.z) for v in screen_vertices)
    screen_center = (
        (screen_min_x + screen_max_x) * 0.5,
        (screen_min_y + screen_max_y) * 0.5,
        (screen_min_z + screen_max_z) * 0.5,
    )

    transformed_vertices = [vertex * housing_to_screen for vertex in housing.data.vertices]
    transformed_normals = [normal * housing_to_screen_rotation for normal in housing.data.normals]
    diffuse_uvs = housing.data.uv_sets[0]
    expanded: list[tuple[float, float, float, float, float, float, float, float]] = []
    for triangle in housing.data.get_triangles():
        for index in triangle:
            vertex = transformed_vertices[index]
            normal = transformed_normals[index]
            uv = diffuse_uvs[index]
            position = (
                (float(vertex.x) - screen_center[0]) / screen_width,
                (float(vertex.y) - screen_center[1]) / screen_height,
                (float(vertex.z) - screen_center[2]) / screen_width,
            )
            mapped_normal = normalized(
                (float(normal.x), float(normal.y), float(normal.z))
            )
            mapped_uv = (float(uv.u), float(uv.v))
            if not all(math.isfinite(value) for value in (*position, *mapped_uv)):
                raise SystemExit("Pip-Boy NIF produced non-finite geometry")
            expanded.append((*position, *mapped_normal, *mapped_uv))

    if not 1000 <= len(expanded) <= 100_000 or len(expanded) % 3:
        raise SystemExit(f"unexpected expanded vertex count: {len(expanded)}")
    output.parent.mkdir(parents=True, exist_ok=True)
    payload = bytearray(struct.pack("<4sIII", b"FPM1", 1, len(expanded), 0))
    for vertex in expanded:
        payload.extend(struct.pack("<8f", *vertex))
    output.write_bytes(payload)
    screen_output_hash = "disabled"
    screen_vertex_count = 0
    if args.screen_output:
        # The retail display is a subtly curved 84-triangle surface, not a
        # guessed rectangle. Normalize that exact mesh into the same
        # screen-local basis as the housing and map its geometry across the
        # live UI crop captured from Fallout's Present path.
        crop_left, crop_top, crop_right, crop_bottom = (
            0.255,
            0.315,
            0.745,
            0.600,
        )
        screen_expanded: list[tuple[float, float, float, float, float]] = []
        for triangle in screen.data.get_triangles():
            for index in triangle:
                vertex = screen_vertices[index]
                panel_u = (float(vertex.x) - screen_min_x) / screen_width
                panel_v_from_bottom = (
                    (float(vertex.y) - screen_min_y) / screen_height
                )
                position = (
                    (float(vertex.x) - screen_center[0]) / screen_width,
                    (float(vertex.y) - screen_center[1]) / screen_height,
                    (float(vertex.z) - screen_center[2]) / screen_width,
                )
                uv = (
                    crop_left + panel_u * (crop_right - crop_left),
                    crop_bottom
                    + panel_v_from_bottom * (crop_top - crop_bottom),
                )
                if not all(math.isfinite(value) for value in (*position, *uv)):
                    raise SystemExit("Pip-Boy screen produced non-finite geometry")
                screen_expanded.append((*position, *uv))
        if not 3 <= len(screen_expanded) <= 10_000 or len(screen_expanded) % 3:
            raise SystemExit(
                f"unexpected expanded screen vertex count: {len(screen_expanded)}"
            )
        screen_output = Path(args.screen_output).resolve()
        screen_output.parent.mkdir(parents=True, exist_ok=True)
        screen_payload = bytearray(
            struct.pack("<4sIII", b"FPS1", 1, len(screen_expanded), 0)
        )
        for vertex in screen_expanded:
            screen_payload.extend(struct.pack("<5f", *vertex))
        screen_output.write_bytes(screen_payload)
        screen_output_hash = hashlib.sha256(screen_payload).hexdigest()
        screen_vertex_count = len(screen_expanded)
    print(
        f"{output}\tvertices={len(expanded)}\t"
        f"screen_units={screen_width:.6f}x{screen_height:.6f}\t"
        f"source_sha256={hashlib.sha256(source.read_bytes()).hexdigest()}\t"
        f"output_sha256={hashlib.sha256(payload).hexdigest()}\t"
        f"screen_vertices={screen_vertex_count}\t"
        f"screen_output_sha256={screen_output_hash}"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
