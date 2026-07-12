#!/usr/bin/env python3
"""Offline preview renderer for the FNVXR 2D wide-sheet shield.

This deliberately does not need Fallout, OpenXR, or Direct3D. It builds the
same kind of center plank + peripheral shell mesh, maps a synthetic world sheet
onto it, then renders PNG views and a numeric seam report.
"""

from __future__ import annotations

import argparse
import json
import math
import os
from dataclasses import dataclass
from datetime import datetime
from pathlib import Path
from typing import Iterable

import numpy as np
from PIL import Image, ImageDraw, ImageFont


@dataclass(frozen=True)
class Config:
    source_width: int
    source_height: int
    center_u0: float
    center_u1: float
    center_v0: float
    center_v1: float
    expand_x: float
    expand_y: float
    center_depth_x: float
    center_depth_y: float
    center_corner_depth: float
    shell_depth_x: float
    shell_depth_y: float
    shell_corner_depth: float
    bottom: bool
    center_source_sheet_uv: bool
    segments: int
    physical_width: float
    physical_height: float


def clamp(value: float, lo: float, hi: float) -> float:
    return max(lo, min(hi, value))


def lerp(a: float, b: float, t: float) -> float:
    return a + (b - a) * t


def load_font(size: int) -> ImageFont.ImageFont:
    candidates = [
        r"C:\Windows\Fonts\segoeui.ttf",
        r"C:\Windows\Fonts\arial.ttf",
        r"C:\Windows\Fonts\consola.ttf",
    ]
    for candidate in candidates:
        if os.path.exists(candidate):
            return ImageFont.truetype(candidate, size)
    return ImageFont.load_default()


def make_world_sheet(cfg: Config) -> Image.Image:
    image = Image.new("RGB", (cfg.source_width, cfg.source_height), (60, 72, 68))
    draw = ImageDraw.Draw(image)
    font = load_font(max(16, cfg.source_width // 72))
    big_font = load_font(max(24, cfg.source_width // 42))

    horizon = int(cfg.source_height * 0.47)
    draw.rectangle((0, 0, cfg.source_width, horizon), fill=(88, 115, 145))
    draw.rectangle((0, horizon, cfg.source_width, cfg.source_height), fill=(90, 83, 60))

    # Simple world-like bands so warping is visible without using game assets.
    for i in range(9):
        x0 = int((i - 1) * cfg.source_width / 7)
        peak = int(horizon - (0.10 + 0.06 * (i % 3)) * cfg.source_height)
        x1 = int((i + 1) * cfg.source_width / 7)
        x2 = int((i + 3) * cfg.source_width / 7)
        color = (93 + i * 5 % 45, 88 + i * 3 % 35, 70 + i * 4 % 35)
        draw.polygon([(x0, horizon), (x1, peak), (x2, horizon)], fill=color)

    for row in range(0, cfg.source_height, max(1, cfg.source_height // 12)):
        t = row / max(1, cfg.source_height - 1)
        color = (120, 135, 150) if row < horizon else (122, 110, 78)
        color = tuple(int(c * (0.75 + 0.25 * t)) for c in color)
        draw.line((0, row, cfg.source_width, row), fill=color, width=2)
    for col in range(0, cfg.source_width, max(1, cfg.source_width // 16)):
        draw.line((col, 0, col, cfg.source_height), fill=(210, 210, 190), width=1)

    # UV grid labels.
    for i in range(17):
        x = int(i * cfg.source_width / 16)
        draw.text((x + 4, 8), f"U {i/16:.2f}", fill=(245, 240, 190), font=font)
    for i in range(13):
        y = int(i * cfg.source_height / 12)
        draw.text((8, y + 4), f"V {i/12:.2f}", fill=(245, 240, 190), font=font)

    cx0 = int(cfg.center_u0 * cfg.source_width)
    cx1 = int(cfg.center_u1 * cfg.source_width)
    cy0 = int(cfg.center_v0 * cfg.source_height)
    cy1 = int(cfg.center_v1 * cfg.source_height)
    draw.rectangle((cx0, cy0, cx1, cy1), outline=(255, 230, 70), width=6)
    draw.text((cx0 + 12, cy0 + 12), "CENTER CROP: HUD PLATE SAMPLES HERE", fill=(255, 235, 95), font=big_font)
    draw.text((18, cfg.source_height - 42), "WORLD-ONLY SOURCE SHEET: no HUD/weapon baked into shell", fill=(245, 245, 220), font=big_font)
    return image


def make_hud_overlay(size: tuple[int, int]) -> Image.Image:
    width, height = size
    image = Image.new("RGBA", size, (0, 0, 0, 0))
    draw = ImageDraw.Draw(image)
    font = load_font(max(16, width // 30))
    amber = (255, 183, 38, 220)
    low = int(height * 0.82)
    draw.line((int(width * 0.18), low, int(width * 0.55), low), fill=amber, width=4)
    for i in range(20):
        x = int(lerp(width * 0.18, width * 0.55, i / 19))
        tick = 20 if i % 5 == 0 else 10
        draw.line((x, low, x, low + tick), fill=amber, width=2)
    draw.text((int(width * 0.17), low - 42), "HP", fill=amber, font=font)
    draw.text((int(width * 0.35), low + 26), "NE", fill=amber, font=font)
    draw.rectangle((int(width * 0.72), low - 4, int(width * 0.91), low + 32), outline=amber, width=3)
    draw.text((int(width * 0.73), low - 44), "CND", fill=amber, font=font)
    draw.polygon(
        [
            (int(width * 0.50), low + 36),
            (int(width * 0.485), low + 72),
            (int(width * 0.515), low + 72),
        ],
        fill=amber,
    )
    return image


def outer_bounds(cfg: Config) -> tuple[float, float, float, float]:
    cw = max(0.001, cfg.center_u1 - cfg.center_u0)
    ch = max(0.001, cfg.center_v1 - cfg.center_v0)
    left = -0.5 - cfg.expand_x * cfg.center_u0 / cw
    right = 0.5 + cfg.expand_x * (1.0 - cfg.center_u1) / cw
    top = 0.5 + cfg.expand_y * cfg.center_v0 / ch
    bottom = -0.5 - cfg.expand_y * (1.0 - cfg.center_v1) / ch
    return left, right, bottom, top


def vertex_at(cfg: Config, x: float, y: float) -> np.ndarray:
    cx = clamp(x, -0.5, 0.5)
    cy = clamp(y, -0.5, 0.5)
    nx = cx * 2.0
    ny = cy * 2.0
    nx2 = nx * nx
    ny2 = ny * ny
    center_z = cfg.center_depth_x * nx2 + cfg.center_depth_y * ny2 + cfg.center_corner_depth * nx2 * ny2
    left, right, bottom, top = outer_bounds(cfg)
    outer_span_x = max(0.001, max(abs(left), abs(right)) - 0.5)
    outer_span_y = max(0.001, max(abs(bottom), abs(top)) - 0.5)
    bx = clamp((abs(x) - 0.5) / outer_span_x, 0.0, 1.0)
    by = clamp((abs(y) - 0.5) / outer_span_y, 0.0, 1.0)
    extra_z = cfg.shell_depth_x * bx * bx + cfg.shell_depth_y * by * by + cfg.shell_corner_depth * bx * bx * by * by
    return np.array([x, y, center_z + extra_z], dtype=np.float32)


def uv_at(cfg: Config, x: float, y: float) -> np.ndarray:
    u = cfg.center_u0 + (x + 0.5) * (cfg.center_u1 - cfg.center_u0)
    v = cfg.center_v0 + (0.5 - y) * (cfg.center_v1 - cfg.center_v0)
    return np.array([clamp(u, 0.0, 1.0), clamp(v, 0.0, 1.0)], dtype=np.float32)


def add_patch(
    cfg: Config,
    triangles: list[tuple[np.ndarray, np.ndarray, np.ndarray]],
    x0: float,
    x1: float,
    y0: float,
    y1: float,
    sx: int,
    sy: int,
) -> None:
    if x1 <= x0 or y1 <= y0:
        return
    sx = max(1, sx)
    sy = max(1, sy)
    for iy in range(sy):
        ty0 = iy / sy
        ty1 = (iy + 1) / sy
        py0 = lerp(y0, y1, ty0)
        py1 = lerp(y0, y1, ty1)
        for ix in range(sx):
            tx0 = ix / sx
            tx1 = (ix + 1) / sx
            px0 = lerp(x0, x1, tx0)
            px1 = lerp(x0, x1, tx1)
            verts = []
            for px, py in [(px0, py0), (px0, py1), (px1, py1), (px1, py0)]:
                verts.append(np.concatenate([vertex_at(cfg, px, py), uv_at(cfg, px, py)]))
            bl, tl, tr, br = verts
            triangles.append((bl, tl, tr))
            triangles.append((bl, tr, br))


def build_mesh(cfg: Config) -> tuple[list[tuple[np.ndarray, np.ndarray, np.ndarray]], list[tuple[np.ndarray, np.ndarray, np.ndarray]]]:
    left, right, bottom, top = outer_bounds(cfg)
    base = cfg.segments
    shell: list[tuple[np.ndarray, np.ndarray, np.ndarray]] = []
    center: list[tuple[np.ndarray, np.ndarray, np.ndarray]] = []

    add_patch(cfg, shell, left, -0.5, -0.5, 0.5, max(4, int(base * (-0.5 - left))), base)
    add_patch(cfg, shell, 0.5, right, -0.5, 0.5, max(4, int(base * (right - 0.5))), base)
    add_patch(cfg, shell, left, -0.5, 0.5, top, max(4, int(base * (-0.5 - left))), max(4, int(base * (top - 0.5))))
    add_patch(cfg, shell, -0.5, 0.5, 0.5, top, base, max(4, int(base * (top - 0.5))))
    add_patch(cfg, shell, 0.5, right, 0.5, top, max(4, int(base * (right - 0.5))), max(4, int(base * (top - 0.5))))
    if cfg.bottom:
        add_patch(cfg, shell, left, -0.5, bottom, -0.5, max(4, int(base * (-0.5 - left))), max(4, int(base * (-0.5 - bottom))))
        add_patch(cfg, shell, -0.5, 0.5, bottom, -0.5, base, max(4, int(base * (-0.5 - bottom))))
        add_patch(cfg, shell, 0.5, right, bottom, -0.5, max(4, int(base * (right - 0.5))), max(4, int(base * (-0.5 - bottom))))
    add_patch(cfg, center, -0.5, 0.5, -0.5, 0.5, base, base)
    return shell, center


def rotation_matrix(yaw_deg: float, pitch_deg: float, roll_deg: float = 0.0) -> np.ndarray:
    yaw = math.radians(yaw_deg)
    pitch = math.radians(pitch_deg)
    roll = math.radians(roll_deg)
    cy, sy = math.cos(yaw), math.sin(yaw)
    cp, sp = math.cos(pitch), math.sin(pitch)
    cr, sr = math.cos(roll), math.sin(roll)
    ry = np.array([[cy, 0, sy], [0, 1, 0], [-sy, 0, cy]], dtype=np.float32)
    rx = np.array([[1, 0, 0], [0, cp, -sp], [0, sp, cp]], dtype=np.float32)
    rz = np.array([[cr, -sr, 0], [sr, cr, 0], [0, 0, 1]], dtype=np.float32)
    return rz @ rx @ ry


def project_vertices(
    vertices: np.ndarray,
    yaw: float,
    pitch: float,
    roll: float,
    width: int,
    height: int,
    zoom: float,
) -> tuple[np.ndarray, np.ndarray]:
    rot = rotation_matrix(yaw, pitch, roll)
    view = vertices @ rot.T
    xy = view[:, :2] * zoom
    xy[:, 0] += width * 0.5
    xy[:, 1] = height * 0.5 - xy[:, 1]
    return xy, view[:, 2]


def sample_texture(texture: np.ndarray, uv: np.ndarray) -> np.ndarray:
    h, w = texture.shape[:2]
    u = clamp(float(uv[0]), 0.0, 1.0)
    v = clamp(float(uv[1]), 0.0, 1.0)
    x = int(round(u * (w - 1)))
    y = int(round(v * (h - 1)))
    return texture[y, x]


def raster_tri(
    color_buffer: np.ndarray,
    depth_buffer: np.ndarray,
    pts: np.ndarray,
    depths: np.ndarray,
    uvs: np.ndarray,
    texture: np.ndarray,
    alpha_blend: bool,
) -> None:
    height, width = depth_buffer.shape
    xmin = max(0, int(math.floor(np.min(pts[:, 0]))))
    xmax = min(width - 1, int(math.ceil(np.max(pts[:, 0]))))
    ymin = max(0, int(math.floor(np.min(pts[:, 1]))))
    ymax = min(height - 1, int(math.ceil(np.max(pts[:, 1]))))
    if xmax < xmin or ymax < ymin:
        return
    p0, p1, p2 = pts
    area = (p1[0] - p0[0]) * (p2[1] - p0[1]) - (p2[0] - p0[0]) * (p1[1] - p0[1])
    if abs(area) < 1e-5:
        return
    for y in range(ymin, ymax + 1):
        py = y + 0.5
        for x in range(xmin, xmax + 1):
            px = x + 0.5
            w0 = ((p1[0] - px) * (p2[1] - py) - (p2[0] - px) * (p1[1] - py)) / area
            w1 = ((p2[0] - px) * (p0[1] - py) - (p0[0] - px) * (p2[1] - py)) / area
            w2 = 1.0 - w0 - w1
            if w0 < -1e-4 or w1 < -1e-4 or w2 < -1e-4:
                continue
            depth = w0 * depths[0] + w1 * depths[1] + w2 * depths[2]
            if not alpha_blend and depth < depth_buffer[y, x]:
                continue
            uv = w0 * uvs[0] + w1 * uvs[1] + w2 * uvs[2]
            src = sample_texture(texture, uv).astype(np.float32)
            if src.shape[0] == 4:
                alpha = src[3] / 255.0
                if alpha <= 0.0:
                    continue
                dst = color_buffer[y, x].astype(np.float32)
                color_buffer[y, x, :3] = (src[:3] * alpha + dst[:3] * (1.0 - alpha)).astype(np.uint8)
                color_buffer[y, x, 3] = 255
                if not alpha_blend:
                    depth_buffer[y, x] = depth
            else:
                color_buffer[y, x, :3] = src[:3].astype(np.uint8)
                color_buffer[y, x, 3] = 255
                depth_buffer[y, x] = depth


def draw_wire(draw: ImageDraw.ImageDraw, tris: Iterable[tuple[np.ndarray, np.ndarray, np.ndarray]], transform) -> None:
    for tri in tris:
        verts = np.array([v[:3] for v in tri], dtype=np.float32)
        pts, _ = transform(verts)
        p = [(float(x), float(y)) for x, y in pts]
        draw.line([p[0], p[1], p[2], p[0]], fill=(255, 255, 255, 70), width=1)


def render_view(
    cfg: Config,
    shell: list[tuple[np.ndarray, np.ndarray, np.ndarray]],
    center: list[tuple[np.ndarray, np.ndarray, np.ndarray]],
    world: Image.Image,
    center_world: Image.Image | None,
    hud: Image.Image,
    path: Path,
    yaw: float,
    pitch: float,
    roll: float = 0.0,
    size: tuple[int, int] = (1400, 900),
) -> None:
    width, height = size
    color = np.zeros((height, width, 4), dtype=np.uint8)
    color[:, :, :] = np.array([18, 20, 24, 255], dtype=np.uint8)
    depth = np.full((height, width), -1e9, dtype=np.float32)
    left, right, bottom, top = outer_bounds(cfg)
    extent_x = max(abs(left), abs(right))
    extent_y = max(abs(bottom), abs(top))
    zoom = min(width / (extent_x * 2.55), height / (extent_y * 2.35))
    world_tex = np.asarray(world.convert("RGB"))
    center_tex = np.asarray((center_world or world).convert("RGB"))
    hud_tex = np.asarray(hud.convert("RGBA"))

    def transform(v):
        return project_vertices(v, yaw, pitch, roll, width, height, zoom)

    all_solid = [(tri, world_tex, False) for tri in shell] + [(tri, center_tex, center_world is not None) for tri in center]
    sortable = []
    for tri, tex, use_panel_uv in all_solid:
        verts = np.array([v[:3] for v in tri], dtype=np.float32)
        pts, d = transform(verts)
        sortable.append((float(np.mean(d)), tri, pts, d, tex, use_panel_uv))
    for _, tri, pts, d, tex, use_panel_uv in sorted(sortable, key=lambda item: item[0]):
        if use_panel_uv and not cfg.center_source_sheet_uv:
            uvs = np.array([[v[0] + 0.5, 0.5 - v[1]] for v in tri], dtype=np.float32)
        else:
            uvs = np.array([v[3:5] for v in tri], dtype=np.float32)
        raster_tri(color, depth, pts, d, uvs, tex, alpha_blend=False)

    # HUD is a separate layer, mapped only onto the center plate.
    for tri in center:
        verts = np.array([v[:3] for v in tri], dtype=np.float32)
        pts, d = transform(verts)
        # The HUD overlay UV is normalized to the center panel, not the world sheet.
        uvs = []
        for v in tri:
            uvs.append([v[0] + 0.5, 0.5 - v[1]])
        raster_tri(color, depth, pts, d + 0.001, np.array(uvs, dtype=np.float32), hud_tex, alpha_blend=True)

    image = Image.fromarray(color, "RGBA")
    draw = ImageDraw.Draw(image, "RGBA")
    draw_wire(draw, shell, transform)
    for tri in center:
        verts = np.array([v[:3] for v in tri], dtype=np.float32)
        pts, _ = transform(verts)
        p = [(float(x), float(y)) for x, y in pts]
        draw.line([p[0], p[1], p[2], p[0]], fill=(255, 220, 50, 72), width=1)
    font = load_font(20)
    draw.text((18, 16), f"yaw={yaw} pitch={pitch} bottom={'on' if cfg.bottom else 'off'}", fill=(245, 245, 230, 235), font=font)
    image.convert("RGB").save(path)


def project_eye_vertices(
    cfg: Config,
    vertices: np.ndarray,
    width: int,
    height: int,
    fov_deg: float,
    distance: float,
) -> tuple[np.ndarray, np.ndarray]:
    world = vertices.copy().astype(np.float32)
    world[:, 0] *= cfg.physical_width
    world[:, 1] *= cfg.physical_height
    world[:, 2] = -distance + world[:, 2]
    forward_distance = np.maximum(0.05, -world[:, 2])
    focal = width / (2.0 * math.tan(math.radians(fov_deg) * 0.5))
    pts = np.empty((len(world), 2), dtype=np.float32)
    pts[:, 0] = width * 0.5 + world[:, 0] / forward_distance * focal
    pts[:, 1] = height * 0.5 - world[:, 1] / forward_distance * focal
    depth = 1.0 / forward_distance
    return pts, depth


def render_eye_pov(
    cfg: Config,
    shell: list[tuple[np.ndarray, np.ndarray, np.ndarray]],
    center: list[tuple[np.ndarray, np.ndarray, np.ndarray]],
    world: Image.Image,
    center_world: Image.Image | None,
    hud: Image.Image,
    path: Path,
    size: tuple[int, int] = (1600, 1000),
    fov_deg: float = 96.0,
    distance: float = 3.55,
    label: str = "",
    draw_debug: bool = False,
) -> None:
    width, height = size
    color = np.zeros((height, width, 4), dtype=np.uint8)
    color[:, :, :] = np.array([12, 13, 16, 255], dtype=np.uint8)
    depth = np.full((height, width), -1e9, dtype=np.float32)
    world_tex = np.asarray(world.convert("RGB"))
    center_tex = np.asarray((center_world or world).convert("RGB"))
    hud_tex = np.asarray(hud.convert("RGBA"))

    for tri in shell:
        verts = np.array([v[:3] for v in tri], dtype=np.float32)
        pts, d = project_eye_vertices(cfg, verts, width, height, fov_deg, distance)
        uvs = np.array([v[3:5] for v in tri], dtype=np.float32)
        raster_tri(color, depth, pts, d, uvs, world_tex, alpha_blend=False)

    for tri in center:
        verts = np.array([v[:3] for v in tri], dtype=np.float32)
        pts, d = project_eye_vertices(cfg, verts, width, height, fov_deg, distance)
        if center_world is not None and not cfg.center_source_sheet_uv:
            uvs = np.array([[v[0] + 0.5, 0.5 - v[1]] for v in tri], dtype=np.float32)
        else:
            uvs = np.array([v[3:5] for v in tri], dtype=np.float32)
        raster_tri(color, depth, pts, d, uvs, center_tex, alpha_blend=False)

    for tri in center:
        verts = np.array([v[:3] for v in tri], dtype=np.float32)
        pts, d = project_eye_vertices(cfg, verts, width, height, fov_deg, distance)
        uvs = np.array([[v[0] + 0.5, 0.5 - v[1]] for v in tri], dtype=np.float32)
        raster_tri(color, depth, pts, d + 0.001, uvs, hud_tex, alpha_blend=True)

    image = Image.fromarray(color, "RGBA")
    draw = ImageDraw.Draw(image, "RGBA")
    font = load_font(22)
    if draw_debug:
        left, right, bottom, top = outer_bounds(cfg)

        def draw_curve(points, fill, line_width):
            verts = np.array([vertex_at(cfg, x, y) for x, y in points], dtype=np.float32)
            pts, _ = project_eye_vertices(cfg, verts, width, height, fov_deg, distance)
            draw.line([(float(x), float(y)) for x, y in pts], fill=fill, width=line_width)

        samples = 80
        ts = [i / (samples - 1) for i in range(samples)]
        draw_curve([(lerp(-0.5, 0.5, t), 0.5) for t in ts], (255, 222, 70, 210), 3)
        draw_curve([(lerp(-0.5, 0.5, t), -0.5) for t in ts], (255, 222, 70, 210), 3)
        draw_curve([(-0.5, lerp(-0.5, 0.5, t)) for t in ts], (255, 222, 70, 210), 3)
        draw_curve([(0.5, lerp(-0.5, 0.5, t)) for t in ts], (255, 222, 70, 210), 3)
        draw_curve([(lerp(left, right, t), top) for t in ts], (70, 214, 255, 170), 2)
        draw_curve([(lerp(left, right, t), bottom) for t in ts], (70, 214, 255, 170), 2)
        draw_curve([(left, lerp(bottom, top, t)) for t in ts], (70, 214, 255, 170), 2)
        draw_curve([(right, lerp(bottom, top, t)) for t in ts], (70, 214, 255, 170), 2)

    caption = label or f"eye POV fov={fov_deg:g} distance={distance:g} bottom={'on' if cfg.bottom else 'off'}"
    draw.text(
        (18, 16),
        caption,
        fill=(245, 245, 230, 235),
        font=font,
    )
    image.convert("RGB").save(path)


def seam_report(cfg: Config) -> dict:
    samples = 101
    seams = {}

    def compare(name: str, points_a, points_b):
        max_position = 0.0
        max_uv = 0.0
        for (xa, ya), (xb, yb) in zip(points_a, points_b):
            max_position = max(max_position, float(np.linalg.norm(vertex_at(cfg, xa, ya) - vertex_at(cfg, xb, yb))))
            max_uv = max(max_uv, float(np.linalg.norm(uv_at(cfg, xa, ya) - uv_at(cfg, xb, yb))))
        seams[name] = {
            "maxPositionDelta": max_position,
            "maxUvDelta": max_uv,
            "pass": max_position < 1e-6 and max_uv < 1e-6,
        }

    ts = [i / (samples - 1) for i in range(samples)]
    compare("center_to_left", [(-0.5, lerp(-0.5, 0.5, t)) for t in ts], [(-0.5, lerp(-0.5, 0.5, t)) for t in ts])
    compare("center_to_right", [(0.5, lerp(-0.5, 0.5, t)) for t in ts], [(0.5, lerp(-0.5, 0.5, t)) for t in ts])
    compare("center_to_top", [(lerp(-0.5, 0.5, t), 0.5) for t in ts], [(lerp(-0.5, 0.5, t), 0.5) for t in ts])
    if cfg.bottom:
        compare("center_to_bottom", [(lerp(-0.5, 0.5, t), -0.5) for t in ts], [(lerp(-0.5, 0.5, t), -0.5) for t in ts])

    crop_aspect = ((cfg.center_u1 - cfg.center_u0) * cfg.source_width) / max(1.0, (cfg.center_v1 - cfg.center_v0) * cfg.source_height)
    physical_aspect = cfg.physical_width / cfg.physical_height
    left, right, bottom, top = outer_bounds(cfg)
    return {
        "seams": seams,
        "allSeamsPass": all(item["pass"] for item in seams.values()),
        "sourceSize": [cfg.source_width, cfg.source_height],
        "centerCropUv": [cfg.center_u0, cfg.center_v0, cfg.center_u1, cfg.center_v1],
        "centerCropPixelAspect": crop_aspect,
        "centerPhysicalAspect": physical_aspect,
        "aspectErrorPercent": (crop_aspect / physical_aspect - 1.0) * 100.0,
        "outerBounds": {"left": left, "right": right, "bottom": bottom, "top": top},
        "bottomEnabled": cfg.bottom,
    }


def write_uv_map(cfg: Config, world: Image.Image, path: Path) -> None:
    image = world.copy().convert("RGB")
    draw = ImageDraw.Draw(image, "RGBA")
    font = load_font(24)
    w, h = image.size
    cx0 = int(cfg.center_u0 * w)
    cx1 = int(cfg.center_u1 * w)
    cy0 = int(cfg.center_v0 * h)
    cy1 = int(cfg.center_v1 * h)
    draw.rectangle((cx0, cy0, cx1, cy1), outline=(255, 230, 50, 255), width=8)
    draw.text((cx0 + 18, cy1 - 48), "HUD overlay lives on this center panel only", fill=(255, 230, 50, 255), font=font)
    draw.rectangle((0, 0, w - 1, h - 1), outline=(255, 255, 255, 160), width=4)
    image.save(path)


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Render an offline 3D shield/UV preview.")
    parser.add_argument("--out-dir", default="", help="Output directory. Defaults under local/shield-preview.")
    parser.add_argument("--source", default="", help="Optional real source image to project instead of the synthetic sheet.")
    parser.add_argument("--center-source", default="", help="Optional normal-FOV center image. The shell still samples --source.")
    parser.add_argument("--center-source-sheet-uv", action="store_true", help="Sample --center-source with the same source-sheet UVs as --source.")
    parser.add_argument("--source-width", type=int, default=2048)
    parser.add_argument("--source-height", type=int, default=1280)
    parser.add_argument("--center-u0", type=float, default=0.12)
    parser.add_argument("--center-u1", type=float, default=0.88)
    parser.add_argument("--center-v0", type=float, default=0.08)
    parser.add_argument("--center-v1", type=float, default=0.92)
    parser.add_argument("--expand-x", type=float, default=1.0)
    parser.add_argument("--expand-y", type=float, default=0.85)
    parser.add_argument("--bottom", action="store_true", help="Preview the experimental bottom arc too.")
    parser.add_argument("--segments", type=int, default=40)
    parser.add_argument("--physical-width", type=float, default=4.85)
    parser.add_argument("--physical-height", type=float, default=3.45)
    parser.add_argument("--eye-fov", type=float, default=96.0)
    parser.add_argument("--eye-distance", type=float, default=3.55)
    parser.add_argument("--filled-eye-distance", type=float, default=2.65)
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    root = Path(__file__).resolve().parents[1]
    if args.out_dir:
        out_dir = Path(args.out_dir)
    else:
        out_dir = root / "local" / "shield-preview" / datetime.now().strftime("%Y%m%d-%H%M%S")
    out_dir.mkdir(parents=True, exist_ok=True)

    source_path = Path(args.source) if args.source else None
    center_source_path = Path(args.center_source) if args.center_source else None
    source_image = None
    center_source_image = None
    source_width = args.source_width
    source_height = args.source_height
    if source_path:
        source_image = Image.open(source_path).convert("RGB")
        source_width, source_height = source_image.size
    if center_source_path:
        center_source_image = Image.open(center_source_path).convert("RGB")

    cfg = Config(
        source_width=source_width,
        source_height=source_height,
        center_u0=clamp(args.center_u0, 0.0, 0.95),
        center_u1=clamp(args.center_u1, args.center_u0 + 0.01, 1.0),
        center_v0=clamp(args.center_v0, 0.0, 0.95),
        center_v1=clamp(args.center_v1, args.center_v0 + 0.01, 1.0),
        expand_x=max(0.1, args.expand_x),
        expand_y=max(0.1, args.expand_y),
        center_depth_x=0.22,
        center_depth_y=0.08,
        center_corner_depth=0.03,
        shell_depth_x=0.70,
        shell_depth_y=0.28,
        shell_corner_depth=0.14,
        bottom=args.bottom,
        center_source_sheet_uv=args.center_source_sheet_uv,
        segments=max(4, args.segments),
        physical_width=args.physical_width,
        physical_height=args.physical_height,
    )

    world = source_image if source_image is not None else make_world_sheet(cfg)
    hud = Image.new("RGBA", (1024, 728), (0, 0, 0, 0)) if source_image is not None else make_hud_overlay((1024, 728))
    shell, center = build_mesh(cfg)

    world.save(out_dir / "source_world_sheet.png")
    if center_source_image is not None:
        center_source_image.save(out_dir / "center_source_panel.png")
    hud.save(out_dir / "center_hud_overlay.png")
    write_uv_map(cfg, world, out_dir / "uv_map.png")
    render_view(cfg, shell, center, world, center_source_image, hud, out_dir / "preview_front.png", yaw=0.0, pitch=0.0)
    render_view(cfg, shell, center, world, center_source_image, hud, out_dir / "preview_oblique.png", yaw=-28.0, pitch=-10.0)
    render_view(cfg, shell, center, world, center_source_image, hud, out_dir / "preview_top.png", yaw=0.0, pitch=62.0)
    render_eye_pov(
        cfg,
        shell,
        center,
        world,
        center_source_image,
        hud,
        out_dir / "preview_eye_pov.png",
        fov_deg=args.eye_fov,
        distance=args.eye_distance,
    )
    render_eye_pov(
        cfg,
        shell,
        center,
        world,
        center_source_image,
        hud,
        out_dir / "preview_eye_filled_debug.png",
        fov_deg=args.eye_fov,
        distance=args.filled_eye_distance,
        label=(
            f"filled/debug POV: yellow=center crop, blue=outer sheet, "
            f"fov={args.eye_fov:g} distance={args.filled_eye_distance:g}"
        ),
        draw_debug=True,
    )

    report = seam_report(cfg)
    report["sourceImage"] = str(source_path) if source_path else "synthetic-world-sheet"
    report["centerSourceImage"] = str(center_source_path) if center_source_path else report["sourceImage"]
    report["triangleCounts"] = {"shell": len(shell), "center": len(center)}
    report["outputs"] = {
        "source": str(out_dir / "source_world_sheet.png"),
        "uvMap": str(out_dir / "uv_map.png"),
        "front": str(out_dir / "preview_front.png"),
        "oblique": str(out_dir / "preview_oblique.png"),
        "top": str(out_dir / "preview_top.png"),
        "eyePov": str(out_dir / "preview_eye_pov.png"),
        "eyePovFilledDebug": str(out_dir / "preview_eye_filled_debug.png"),
    }
    (out_dir / "seam-report.json").write_text(json.dumps(report, indent=2), encoding="utf-8")
    print(json.dumps(report, indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
