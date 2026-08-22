#!/usr/bin/env python3
"""Print compact, read-only mesh/material evidence from a retail NIF."""

from __future__ import annotations

import argparse
import json
import time
from pathlib import Path


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("path")
    parser.add_argument(
        "--skeleton",
        action="store_true",
        help="include NiNode hierarchy and local transforms",
    )
    args = parser.parse_args()
    if not hasattr(time, "clock"):
        time.clock = time.perf_counter  # type: ignore[attr-defined]
    from pyffi.formats.nif import NifFormat

    source = Path(args.path).resolve()
    data = NifFormat.Data()
    with source.open("rb") as stream:
        data.read(stream)
    shapes = []
    for block in data.blocks:
        if not isinstance(block, NifFormat.NiGeometry) or not block.data:
            continue
        vertices = block.get_skin_deformation()[0] if block.is_skin() else block.data.vertices
        bounds = {
            "minimum": [min(float(getattr(vertex, axis)) for vertex in vertices)
                        for axis in ("x", "y", "z")],
            "maximum": [max(float(getattr(vertex, axis)) for vertex in vertices)
                        for axis in ("x", "y", "z")],
        }
        textures = []
        for property_block in getattr(block, "bs_properties", []):
            texture_set = getattr(property_block, "texture_set", None)
            if texture_set:
                textures.extend(
                    bytes(texture).decode("utf-8", "replace")
                    for texture in texture_set.textures
                    if bytes(texture)
                )
        shapes.append({
            "name": bytes(block.name).decode("utf-8", "replace"),
            "vertices": int(block.data.num_vertices),
            "triangles": len(block.data.get_triangles()),
            "skinned": bool(block.is_skin()),
            "uv_sets": int(block.data.num_uv_sets),
            "bounds": bounds,
            "textures": textures,
        })
    payload = {
        "source": str(source),
        "version": f"0x{data.version:08x}",
        "roots": [bytes(root.name).decode("utf-8", "replace") for root in data.roots],
        "shapes": shapes,
    }
    if args.skeleton:
        parents = {}
        for block in data.blocks:
            for child in getattr(block, "children", []):
                if child is not None:
                    parents[id(child)] = block
        nodes = []
        for block in data.blocks:
            if not isinstance(block, NifFormat.NiNode):
                continue
            scale, rotation, translation = (
                block.get_transform().get_scale_rotation_translation()
            )
            parent = parents.get(id(block))
            nodes.append({
                "name": bytes(block.name).decode("utf-8", "replace"),
                "parent": (
                    bytes(parent.name).decode("utf-8", "replace")
                    if parent is not None else None
                ),
                "scale": float(scale),
                "translation": [
                    float(translation.x),
                    float(translation.y),
                    float(translation.z),
                ],
                "rotation": [
                    [float(rotation.m_11), float(rotation.m_12), float(rotation.m_13)],
                    [float(rotation.m_21), float(rotation.m_22), float(rotation.m_23)],
                    [float(rotation.m_31), float(rotation.m_32), float(rotation.m_33)],
                ],
            })
        payload["nodes"] = nodes
    print(json.dumps(payload, indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
