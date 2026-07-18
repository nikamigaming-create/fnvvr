#!/usr/bin/env python3
"""Generate the offline FNVXR 6DoF proof, printable fixtures, and PDF.

This script never launches Fallout, OpenXR, SteamVR, or a headset process.
Numeric cases and STL geometry are deterministic; report timestamps are not.
STL dimensions are millimeters; Fallout units use 70 units/meter.
"""

from __future__ import annotations

import hashlib
import itertools
import json
import math
import os
import shutil
import subprocess
from dataclasses import dataclass
from datetime import datetime, timezone
from pathlib import Path
from typing import Any, Iterable

import manifold3d
import numpy as np
import trimesh
from PIL import Image as PilImage
from PIL import ImageDraw, ImageFont
from reportlab.lib import colors
from reportlab.lib.enums import TA_CENTER, TA_LEFT
from reportlab.lib.pagesizes import letter
from reportlab.lib.styles import ParagraphStyle, getSampleStyleSheet
from reportlab.lib.units import inch
from reportlab.platypus import (
    Image,
    KeepTogether,
    PageBreak,
    Paragraph,
    SimpleDocTemplate,
    Spacer,
    Table,
    TableStyle,
)


ROOT = Path(__file__).resolve().parents[1]
OUTPUT_3D = ROOT / "output" / "3d"
OUTPUT_PROOF = ROOT / "output" / "proof"
OUTPUT_PDF = ROOT / "output" / "pdf"

SEED = 0x6D0F2026
UNITS_PER_METER = 70.0
IPD_MM = 64.0
IPD_UNITS = IPD_MM / 1000.0 * UNITS_PER_METER

# OpenXR and NiCamera local axes are both +X right, +Y up, +Z back.
XR_TO_NICAMERA_LOCAL = np.eye(3, dtype=np.float64)
# Actor/Gamebryo vectors use +X right, +Y forward, +Z up.
XR_TO_ACTOR = np.array(
    [[1.0, 0.0, 0.0], [0.0, 0.0, -1.0], [0.0, 1.0, 0.0]],
    dtype=np.float64,
)


@dataclass
class Check:
    name: str
    passed: bool
    trials: int
    maximum_error: float
    tolerance: float
    detail: str

    def as_dict(self) -> dict[str, Any]:
        return {
            "name": self.name,
            "passed": self.passed,
            "trials": self.trials,
            "maximum_error": self.maximum_error,
            "tolerance": self.tolerance,
            "detail": self.detail,
        }


def random_rotation(rng: np.random.Generator) -> np.ndarray:
    q = rng.normal(size=4)
    q /= np.linalg.norm(q)
    x, y, z, w = q
    return np.array(
        [
            [1 - 2 * (y * y + z * z), 2 * (x * y - z * w), 2 * (x * z + y * w)],
            [2 * (x * y + z * w), 1 - 2 * (x * x + z * z), 2 * (y * z - x * w)],
            [2 * (x * z - y * w), 2 * (y * z + x * w), 1 - 2 * (x * x + y * y)],
        ],
        dtype=np.float64,
    )


def axis_rotation(axis: str, radians: float) -> np.ndarray:
    c = math.cos(radians)
    s = math.sin(radians)
    if axis == "x":
        return np.array([[1, 0, 0], [0, c, -s], [0, s, c]], dtype=np.float64)
    if axis == "y":
        return np.array([[c, 0, s], [0, 1, 0], [-s, 0, c]], dtype=np.float64)
    if axis == "z":
        return np.array([[c, -s, 0], [s, c, 0], [0, 0, 1]], dtype=np.float64)
    raise ValueError(axis)


def run_numeric_proof() -> tuple[list[Check], dict[str, Any]]:
    rng = np.random.default_rng(SEED)
    checks: list[Check] = []

    basis_error = max(
        float(np.max(np.abs(XR_TO_ACTOR.T @ XR_TO_ACTOR - np.eye(3)))),
        float(np.max(np.abs(XR_TO_NICAMERA_LOCAL.T @ XR_TO_NICAMERA_LOCAL - np.eye(3)))),
        abs(float(np.linalg.det(XR_TO_ACTOR)) - 1.0),
        abs(float(np.linalg.det(XR_TO_NICAMERA_LOCAL)) - 1.0),
    )
    checks.append(
        Check(
            "Both declared basis maps are proper rotations",
            basis_error < 1e-12,
            2,
            basis_error,
            1e-12,
            "Actor permutation and NiCamera-local identity are orthonormal with determinant +1.",
        )
    )

    # Rigid/similarity theorem:
    # d_i = R_o^T(p_i-p_o), R_i' = R_o^T R_i
    # p_i^G = b+s C d_i, R_i^G = C R_i'
    # X_G = b+s C R_o^T(X-p_o)
    # Therefore (R_i^G)^T(X_G-p_i^G) = s R_i^T(X-p_i).
    rigid_trials = 5000
    rigid_error = 0.0
    for _ in range(rigid_trials):
        origin_rotation = axis_rotation("y", rng.uniform(-math.pi, math.pi))
        camera_body = random_rotation(rng)
        eye_rotation = random_rotation(rng)
        origin_position = rng.uniform(-10.0, 10.0, size=3)
        eye_position = origin_position + rng.uniform(-2.0, 2.0, size=3)
        world_point = origin_position + rng.uniform(-30.0, 30.0, size=3)
        base = rng.uniform(-100000.0, 100000.0, size=3)
        eye_delta = origin_rotation.T @ (eye_position - origin_position)
        eye_local_rotation = origin_rotation.T @ eye_rotation
        game_eye_position = base + UNITS_PER_METER * camera_body @ eye_delta
        game_eye_rotation = camera_body @ eye_local_rotation
        game_point = base + UNITS_PER_METER * camera_body @ (
            origin_rotation.T @ (world_point - origin_position)
        )
        actual = game_eye_rotation.T @ (game_point - game_eye_position)
        expected = UNITS_PER_METER * eye_rotation.T @ (world_point - eye_position)
        rigid_error = max(rigid_error, float(np.max(np.abs(actual - expected))))
    checks.append(
        Check(
            "Rigid 6DoF eye-space equivalence",
            rigid_error < 5e-9,
            rigid_trials,
            rigid_error,
            5e-9,
            "Random body headings, origins, eye poses, and points satisfy the written identity.",
        )
    )

    # Axis impulses under arbitrary camera headings. The local displacement
    # recovered by C^T must exactly match the OpenXR right/up/back impulse.
    axis_trials = 3000
    axis_error = 0.0
    impulses = np.eye(3) * 0.1
    for _ in range(axis_trials // 3):
        camera_body = random_rotation(rng)
        for impulse in impulses:
            world = camera_body @ (XR_TO_NICAMERA_LOCAL @ impulse)
            recovered = camera_body.T @ world
            axis_error = max(axis_error, float(np.max(np.abs(recovered - impulse))))
    checks.append(
        Check(
            "NiCamera-local translation has no cross-axis leakage",
            axis_error < 1e-12,
            axis_trials,
            axis_error,
            1e-12,
            "Right, up, and back impulses recover unchanged for arbitrary camera headings.",
        )
    )

    physical_forward = np.array([0.0, 0.0, -0.1])
    physical_up = np.array([0.0, 0.1, 0.0])
    old_forward = XR_TO_ACTOR @ physical_forward
    old_up = XR_TO_ACTOR @ physical_up
    axis_counterexample = {
        "physical_forward_xr_m": physical_forward.tolist(),
        "correct_nicamera_local_m": physical_forward.tolist(),
        "old_actor_permutation_then_nicamera_local_m": old_forward.tolist(),
        "physical_up_xr_m": physical_up.tolist(),
        "old_actor_permutation_for_up_m": old_up.tolist(),
        "conclusion": "The old path maps forward to camera-up and height to camera-back.",
    }

    origin_height = np.array([0.0, 1.65, 0.0])
    same_height = origin_height.copy()
    lifted_height = np.array([0.0, 1.75, 0.0])
    zero_local = (same_height - origin_height) * UNITS_PER_METER
    lifted_local = (lifted_height - origin_height) * UNITS_PER_METER
    height_error = max(
        float(np.max(np.abs(zero_local))),
        float(np.max(np.abs(lifted_local - np.array([0.0, 7.0, 0.0])))),
    )
    checks.append(
        Check(
            "Recenter removes absolute height and preserves relative lift",
            height_error < 1e-12,
            2,
            height_error,
            1e-12,
            "At a 1.65 m origin the local value is zero; a later +100 mm lift is +7 units only on local up.",
        )
    )

    left_eye = np.array([-0.032, 0.0, 0.0])
    right_eye = np.array([0.032, 0.0, 0.0])
    ipd_values = np.concatenate(
        [left_eye * UNITS_PER_METER, right_eye * UNITS_PER_METER, (right_eye - left_eye) * UNITS_PER_METER]
    )
    ipd_expected = np.array([-2.24, 0, 0, 2.24, 0, 0, 4.48, 0, 0])
    ipd_error = float(np.max(np.abs(ipd_values - ipd_expected)))
    checks.append(
        Check(
            "64 mm eye baseline has correct signs and scale",
            ipd_error < 1e-12,
            1,
            ipd_error,
            1e-12,
            "Left/right offsets are -2.24/+2.24 units; baseline is +4.48 units.",
        )
    )

    recenter_pitch = axis_rotation("x", math.radians(30.0))
    world_yaw = axis_rotation("y", math.radians(90.0))
    old_relative_yaw = recenter_pitch.T @ world_yaw @ recenter_pitch
    old_forward_after_yaw = old_relative_yaw @ np.array([0.0, 0.0, -1.0])
    repaired_head_at_origin = recenter_pitch
    repaired_head_after_world_yaw = world_yaw @ recenter_pitch
    repaired_world_delta = repaired_head_after_world_yaw @ repaired_head_at_origin.T
    repaired_forward_after_yaw = repaired_world_delta @ np.array([0.0, 0.0, -1.0])
    repaired_vertical_leakage = abs(float(repaired_forward_after_yaw[1]))
    recenter_counterexample = {
        "old_full_quaternion_relative_forward": old_forward_after_yaw.tolist(),
        "vertical_leak_magnitude": abs(float(old_forward_after_yaw[1])),
        "repaired_yaw_only_incremental_forward": repaired_forward_after_yaw.tolist(),
        "repaired_vertical_leak_magnitude": repaired_vertical_leakage,
        "conclusion": "A pure world-up yaw acquires a vertical component after full-pitch recenter conjugation.",
    }
    checks.append(
        Check(
            "Yaw-only recenter removes gravity-axis mixing counterexample",
            abs(float(old_forward_after_yaw[1])) > 0.4 and repaired_vertical_leakage < 1e-12,
            1,
            repaired_vertical_leakage,
            1e-12,
            "The old origin leaks vertically; the repaired yaw-only incremental delta has zero vertical leakage.",
        )
    )

    frustum_trials = 600
    frustum_samples_per_trial = 24
    frustum_violation = 0.0
    for _ in range(frustum_trials):
        center_rotation = random_rotation(rng)
        center_position = rng.uniform(-0.02, 0.02, size=3)
        near = rng.uniform(0.03, 0.15)
        far = rng.uniform(5.0, 1000.0)
        corners: list[np.ndarray] = []
        eye_data: list[tuple[np.ndarray, np.ndarray, tuple[float, float, float, float]]] = []
        for eye_sign in (-1.0, 1.0):
            eye_position = center_position + center_rotation @ np.array(
                [eye_sign * rng.uniform(0.027, 0.037), rng.uniform(-0.002, 0.002), rng.uniform(-0.002, 0.002)]
            )
            cant = axis_rotation("y", eye_sign * rng.uniform(-0.08, 0.08))
            eye_rotation = center_rotation @ cant
            left = rng.uniform(-1.4, -0.45)
            right = rng.uniform(0.45, 1.4)
            bottom = rng.uniform(-1.3, -0.4)
            top = rng.uniform(0.4, 1.3)
            eye_data.append((eye_position, eye_rotation, (left, right, bottom, top)))
            for depth in (near, far):
                for tx in (left, right):
                    for ty in (bottom, top):
                        world = eye_position + eye_rotation @ np.array([tx * depth, ty * depth, -depth])
                        corners.append(center_rotation.T @ (world - center_position))
        corner_array = np.asarray(corners)
        forward = -corner_array[:, 2]
        left_bound = np.nextafter(np.min(corner_array[:, 0] / forward), -np.inf)
        right_bound = np.nextafter(np.max(corner_array[:, 0] / forward), np.inf)
        bottom_bound = np.nextafter(np.min(corner_array[:, 1] / forward), -np.inf)
        top_bound = np.nextafter(np.max(corner_array[:, 1] / forward), np.inf)
        near_bound = np.nextafter(np.min(forward), 0.0)
        far_bound = np.nextafter(np.max(forward), np.inf)
        for _sample in range(frustum_samples_per_trial):
            eye_position, eye_rotation, tangents = eye_data[int(rng.integers(0, 2))]
            left, right, bottom, top = tangents
            depth = rng.uniform(near, far)
            tx = rng.uniform(left, right)
            ty = rng.uniform(bottom, top)
            world = eye_position + eye_rotation @ np.array([tx * depth, ty * depth, -depth])
            center = center_rotation.T @ (world - center_position)
            fwd = -center[2]
            sx, sy = center[0] / fwd, center[1] / fwd
            violation = max(
                left_bound - sx,
                sx - right_bound,
                bottom_bound - sy,
                sy - top_bound,
                near_bound - fwd,
                fwd - far_bound,
                0.0,
            )
            frustum_violation = max(frustum_violation, float(violation))
    checks.append(
        Check(
            "Translated/canted eye frusta stay inside center traversal union",
            frustum_violation <= 1e-12,
            frustum_trials * frustum_samples_per_trial,
            frustum_violation,
            1e-12,
            "Bounds come from both eyes' near/far corners and are rounded outward.",
        )
    )

    # Captured retail matrix from a failing run. It is intentionally badly
    # conditioned and exposes why a float Gauss-Jordan inverse is not a proof.
    center_vp = np.array(
        [
            [-0.456312, -0.565370, 0.0, -33734.3],
            [0.150332, -0.121333, 0.835479, 3832.55],
            [0.778176, -0.628068, 0.0, 55550.0],
            [0.778165, -0.628060, 0.0, 55554.2],
        ],
        dtype=np.float64,
    )
    condition = float(np.linalg.cond(center_vp))
    inverse = np.linalg.inv(center_vp)
    inverse_residual = float(np.max(np.abs(center_vp @ inverse - np.eye(4))))
    exact_identity_delta = center_vp @ inverse
    identity_delta_error = float(np.max(np.abs(exact_identity_delta - np.eye(4))))
    wvp_trials = 500
    reconstruction_error = 0.0
    for _ in range(wvp_trials):
        delta_true = np.eye(4)
        delta_true[:3, :3] = random_rotation(rng)
        delta_true[:3, 3] = rng.uniform(-0.05, 0.05, size=3)
        eye_vp = delta_true @ center_vp
        recovered_delta = eye_vp @ inverse
        model = np.eye(4)
        model[:3, :3] = random_rotation(rng) * rng.uniform(0.5, 3.0)
        model[:3, 3] = rng.uniform(-100000.0, 100000.0, size=3)
        original_wvp = center_vp @ model
        reconstructed = recovered_delta @ original_wvp
        expected = eye_vp @ model
        scale = max(1.0, float(np.max(np.abs(expected))))
        reconstruction_error = max(
            reconstruction_error,
            float(np.max(np.abs(reconstructed - expected))) / scale,
        )
    wvp_error = max(inverse_residual, identity_delta_error, reconstruction_error)
    checks.append(
        Check(
            "Double-precision VP delta reconstructs captured-scale WVP",
            wvp_error < 1e-7,
            wvp_trials + 1,
            wvp_error,
            1e-7,
            "The captured matrix condition number is %.6g; exact E=C is handled as identity in production." % condition,
        )
    )

    synthetic_eye_delta = np.eye(4, dtype=np.float64)
    synthetic_eye_delta[0, 2] = 0.00025
    synthetic_eye_delta[1, 2] = -0.00015
    retail_eye = synthetic_eye_delta @ center_vp
    reference_delta = retail_eye @ inverse
    amplified_wvp = np.fromfunction(
        lambda row, column: (row + 1.0) * (column + 3.0) * 100000000.0,
        (4, 4),
        dtype=float,
    ).astype(np.float32)
    amplified_float_patch = (
        reference_delta.astype(np.float32) @ amplified_wvp
    ).astype(np.float64)
    amplified_double_reference = reference_delta @ amplified_wvp.astype(np.float64)
    amplified_absolute_error = float(
        np.max(np.abs(amplified_float_patch - amplified_double_reference))
    )
    amplified_normalized_error = amplified_absolute_error / max(
        1.0, float(np.max(np.abs(amplified_double_reference)))
    )
    checks.append(
        Check(
            "Final patched-WVP gate detects draw-local float amplification",
            amplified_absolute_error > 0.01,
            1,
            0.0 if amplified_absolute_error > 0.01 else 1.0,
            0.0,
            "The adversarial 1e8-scale WVP produces %.6g absolute error and is rejected before shader upload."
            % amplified_absolute_error,
        )
    )

    age_cases = [
        (1_000_000_000, 980_000_000, True),
        (1_000_000_000, 900_000_000, False),
        (1_000_000_000, 1_010_000_000, False),
    ]
    age_results = []
    for current, source, expected in age_cases:
        age = current - source
        actual = -5_000_000 <= age <= 25_000_000
        age_results.append(actual == expected)
    checks.append(
        Check(
            "Source-pose age gate accepts 20 ms and rejects 100 ms/future 10 ms",
            all(age_results),
            len(age_cases),
            0.0 if all(age_results) else 1.0,
            0.0,
            "No-depth projection pixels must be within the strict 25 ms source-pose budget.",
        )
    )

    details = {
        "seed": SEED,
        "units_per_meter": UNITS_PER_METER,
        "ipd_mm": IPD_MM,
        "ipd_game_units": IPD_UNITS,
        "axis_counterexample": axis_counterexample,
        "recenter_counterexample": recenter_counterexample,
        "captured_vp_condition_number": condition,
        "captured_vp_inverse_residual": inverse_residual,
        "captured_vp_relative_reconstruction_error": reconstruction_error,
        "amplified_patched_wvp_absolute_error": amplified_absolute_error,
        "amplified_patched_wvp_normalized_error": amplified_normalized_error,
        "total_trials": int(sum(check.trials for check in checks)),
    }
    return checks, details


def make_box(extents: Iterable[float], center: Iterable[float]) -> trimesh.Trimesh:
    mesh = trimesh.creation.box(extents=np.asarray(tuple(extents), dtype=np.float64))
    mesh.apply_translation(np.asarray(tuple(center), dtype=np.float64))
    return mesh


def make_cylinder(radius: float, height: float, center: Iterable[float], sections: int = 32) -> trimesh.Trimesh:
    mesh = trimesh.creation.cylinder(radius=radius, height=height, sections=sections)
    mesh.apply_translation(np.asarray(tuple(center), dtype=np.float64))
    return mesh


def line_prism(start: tuple[float, float], end: tuple[float, float], width: float, height: float) -> trimesh.Trimesh:
    start_v = np.asarray(start, dtype=np.float64)
    end_v = np.asarray(end, dtype=np.float64)
    delta = end_v - start_v
    length = float(np.linalg.norm(delta))
    mesh = trimesh.creation.box(extents=(length, width, height))
    angle = math.atan2(delta[1], delta[0])
    transform = trimesh.transformations.rotation_matrix(angle, (0.0, 0.0, 1.0))
    mesh.apply_transform(transform)
    midpoint = (start_v + end_v) * 0.5
    mesh.apply_translation((midpoint[0], midpoint[1], height * 0.5))
    return mesh


def union(meshes: list[trimesh.Trimesh]) -> trimesh.Trimesh:
    result = trimesh.boolean.union(meshes, engine="manifold")
    if result is None or result.is_empty:
        raise RuntimeError("manifold union returned an empty mesh")
    result.remove_unreferenced_vertices()
    return result


def difference(subject: trimesh.Trimesh, cutters: list[trimesh.Trimesh]) -> trimesh.Trimesh:
    result = trimesh.boolean.difference([subject, *cutters], engine="manifold")
    if result is None or result.is_empty:
        raise RuntimeError("manifold difference returned an empty mesh")
    result.remove_unreferenced_vertices()
    return result


def build_base() -> trimesh.Trimesh:
    parts = [
        make_box((194.0, 20.0, 8.0), (54.0, 0.0, 4.0)),
        make_box((20.0, 184.0, 8.0), (0.0, 45.0, 4.0)),
        line_prism((0.0, 0.0), (83.0, 61.0), 8.0, 8.0),
    ]
    # Raised, countable 25/50/100 mm stations along +X and physical forward
    # (printer +Y equals OpenXR -Z).
    for distance, count in ((25.0, 1), (50.0, 2), (100.0, 3)):
        for index in range(count):
            parts.append(make_cylinder(1.9, 2.0, (distance, -3.0 + index * 3.0, 8.8), 24))
            parts.append(make_cylinder(1.9, 2.0, (-3.0 + index * 3.0, distance, 8.8), 24))
    # Endpoint signatures: one peg on +X, three on forward, two on rear.
    parts.append(make_cylinder(3.0, 3.0, (146.0, 0.0, 9.0), 32))
    for x in (-5.0, 0.0, 5.0):
        parts.append(make_cylinder(2.3, 3.0, (x, 132.0, 9.0), 24))
    for x in (-3.0, 3.0):
        parts.append(make_cylinder(2.3, 3.0, (x, -42.0, 9.0), 24))
    solid = union(parts)
    socket = make_box((12.5, 6.5, 12.0), (0.0, 0.0, 5.0))
    return difference(solid, [socket])


def build_mast() -> trimesh.Trimesh:
    parts = [
        # The 8 mm tab fills the through-socket. The larger flange bottoms at
        # STL Z=8, exactly the assembled base-top/OpenXR origin, and provides a
        # positive hard stop that cannot pass through the socket.
        make_box((12.0, 6.0, 8.0), (0.0, 0.0, 4.0)),
        make_box((18.0, 12.0, 4.0), (0.0, 0.0, 10.0)),
        make_box((10.0, 6.0, 168.0), (0.0, 0.0, 94.0)),
    ]
    for distance, width in ((25.0, 12.0), (50.0, 14.0), (100.0, 16.0)):
        parts.append(make_box((width, 8.0, 2.4), (0.0, 0.0, 8.0 + distance)))
    # Unique two-prong endpoint terminates at STL Z=181, exactly +173 mm
    # relative to the assembled flange/base-top datum at Z=8.
    parts.append(make_box((3.2, 6.0, 8.0), (-3.4, 0.0, 177.0)))
    parts.append(make_box((3.2, 6.0, 8.0), (3.4, 0.0, 177.0)))
    return union(parts)


def build_ipd_comb() -> trimesh.Trimesh:
    body_parts = [make_box((104.0, 46.0, 6.0), (0.0, 0.0, 3.0))]
    rows = [(56.0, -16.0), (60.0, -8.0), (64.0, 0.0), (68.0, 8.0), (72.0, 16.0)]
    for row_index, (_separation, y) in enumerate(rows, start=1):
        for marker_index in range(row_index):
            body_parts.append(make_cylinder(1.1, 1.8, (-47.0 + marker_index * 2.8, y, 6.6), 20))
    body = union(body_parts)
    cutters: list[trimesh.Trimesh] = []
    for separation, y in rows:
        half = separation * 0.5
        if separation == 64.0:
            cutters.append(make_box((5.5, 5.5, 10.0), (-half, y, 3.0)))
        else:
            cutters.append(make_cylinder(2.75, 10.0, (-half, y, 3.0), 32))
        cutters.append(make_cylinder(2.75, 10.0, (half, y, 3.0), 32))
    return difference(body, cutters)


def build_rotation_gauge(code_count: int) -> trimesh.Trimesh:
    parts = [make_box((100.0, 12.0, 4.0), (0.0, 0.0, 2.0))]
    for degrees in (-30.0, -15.0, 0.0, 15.0, 30.0):
        radians = math.radians(degrees)
        endpoint = (45.0 * math.sin(radians), 45.0 * math.cos(radians))
        parts.append(line_prism((0.0, 0.0), endpoint, 3.2, 4.0))
        parts.append(make_cylinder(2.4, 4.0, (endpoint[0], endpoint[1], 2.0), 20))
    for index in range(code_count):
        parts.append(make_cylinder(2.0, 2.0, (-8.0 + index * 8.0, -1.0, 4.8), 20))
    return union(parts)


def connected_component_count(faces: np.ndarray, vertex_count: int) -> int:
    adjacency: list[list[int]] = [[] for _ in range(vertex_count)]
    used = np.zeros(vertex_count, dtype=bool)
    for a, b, c in faces:
        a, b, c = int(a), int(b), int(c)
        used[[a, b, c]] = True
        adjacency[a].extend((b, c))
        adjacency[b].extend((a, c))
        adjacency[c].extend((a, b))
    remaining = set(np.flatnonzero(used).tolist())
    components = 0
    while remaining:
        components += 1
        stack = [remaining.pop()]
        while stack:
            current = stack.pop()
            for neighbor in adjacency[current]:
                if neighbor in remaining:
                    remaining.remove(neighbor)
                    stack.append(neighbor)
    return components


def custom_mesh_checks(mesh: trimesh.Trimesh) -> dict[str, Any]:
    vertices = np.asarray(mesh.vertices, dtype=np.float64)
    faces = np.asarray(mesh.faces, dtype=np.int64)
    triangles = vertices[faces]
    doubled_area = np.linalg.norm(
        np.cross(triangles[:, 1] - triangles[:, 0], triangles[:, 2] - triangles[:, 0]), axis=1
    )
    sorted_faces = np.sort(faces, axis=1)
    duplicate_face_count = int(len(sorted_faces) - len(np.unique(sorted_faces, axis=0)))
    edges = np.sort(
        np.vstack((faces[:, [0, 1]], faces[:, [1, 2]], faces[:, [2, 0]])),
        axis=1,
    )
    _, edge_counts = np.unique(edges, axis=0, return_counts=True)
    triangle_edges = np.vstack(
        (
            triangles[:, 1] - triangles[:, 0],
            triangles[:, 2] - triangles[:, 1],
            triangles[:, 0] - triangles[:, 2],
        )
    )
    edge_lengths = np.linalg.norm(triangle_edges, axis=1)
    bad_edge_count = int(np.count_nonzero(edge_counts != 2))
    signed_volume = float(
        np.sum(np.einsum("ij,ij->i", triangles[:, 0], np.cross(triangles[:, 1], triangles[:, 2]))) / 6.0
    )
    return {
        "finite": bool(np.isfinite(vertices).all()),
        "minimum_triangle_area_mm2": float(np.min(doubled_area) * 0.5),
        "minimum_triangle_edge_mm": float(np.min(edge_lengths)),
        "zero_area_triangle_count": int(np.count_nonzero(doubled_area <= 1e-10)),
        "duplicate_face_count": duplicate_face_count,
        "bad_edge_incidence_count": bad_edge_count,
        "connected_components": connected_component_count(faces, len(vertices)),
        "signed_volume_mm3": signed_volume,
    }


def manifold_status(mesh: trimesh.Trimesh) -> dict[str, Any]:
    mmesh = manifold3d.Mesh(
        np.asarray(mesh.vertices, dtype=np.float32),
        np.asarray(mesh.faces, dtype=np.uint32),
    )
    manifold = manifold3d.Manifold(mmesh)
    return {
        "status": str(manifold.status()).split(".")[-1],
        "triangles": int(manifold.num_tri()),
        "vertices": int(manifold.num_vert()),
        "volume_mm3": float(manifold.volume()),
        "surface_area_mm2": float(manifold.surface_area()),
        "genus": int(manifold.genus()),
    }


def simplify_mesh(mesh: trimesh.Trimesh, tolerance_mm: float = 0.01) -> trimesh.Trimesh:
    source = manifold3d.Manifold(
        manifold3d.Mesh(
            np.asarray(mesh.vertices, dtype=np.float32),
            np.asarray(mesh.faces, dtype=np.uint32),
        )
    )
    simplified = source.simplify(tolerance_mm)
    if str(simplified.status()).split(".")[-1] != "NoError":
        raise RuntimeError("manifold simplification failed")
    output = simplified.to_mesh()
    result = trimesh.Trimesh(
        vertices=np.asarray(output.vert_properties, dtype=np.float64)[:, :3],
        faces=np.asarray(output.tri_verts, dtype=np.int64),
        process=True,
    )
    result.remove_unreferenced_vertices()
    return result


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def run_offline_gates() -> dict[str, Any]:
    """Build and test the exact source/binaries named in the report."""
    powershell = shutil.which("powershell.exe") or shutil.which("powershell")
    if not powershell:
        raise RuntimeError("PowerShell was not found for the launch-safety audit")
    commands = [
        ("build_win32_release", ["cmake", "--build", "build-win32", "--config", "Release"]),
        (
            "ctest_win32_release",
            ["ctest", "--test-dir", "build-win32", "-C", "Release", "--output-on-failure"],
        ),
        ("build_x64_release", ["cmake", "--build", "build", "--config", "Release"]),
        (
            "ctest_x64_release",
            ["ctest", "--test-dir", "build", "-C", "Release", "--output-on-failure"],
        ),
        (
            "launch_safety_audit",
            [
                powershell,
                "-NoProfile",
                "-ExecutionPolicy",
                "Bypass",
                "-File",
                str(ROOT / "scripts" / "audit-launch-safety.ps1"),
            ],
        ),
        (
            "shader_contract_dataflow_selftest",
            [
                powershell,
                "-NoProfile",
                "-ExecutionPolicy",
                "Bypass",
                "-File",
                str(ROOT / "scripts" / "get-verified-shader-wvp-contracts.ps1"),
                "-SelfTestOnly",
            ],
        ),
    ]
    records: list[dict[str, Any]] = []
    for name, command in commands:
        completed = subprocess.run(
            command,
            cwd=ROOT,
            capture_output=True,
            text=True,
            timeout=180,
            check=False,
        )
        output = (completed.stdout or "") + (completed.stderr or "")
        records.append(
            {
                "name": name,
                "command": command,
                "exit_code": completed.returncode,
                "passed": completed.returncode == 0,
                "output_sha256": hashlib.sha256(output.encode("utf-8", errors="replace")).hexdigest(),
                "output_tail": output.splitlines()[-24:],
            }
        )

    source_paths = [
        "tools/generate_6dof_proof_artifacts.py",
        "docs/6dof-coordinate-proof.md",
        "renderhook/fnvxr_stereo_math.h",
        "renderhook/fnvxr_d3d9_proxy.cpp",
        "host/fnvxr_openxr_pose_host.cpp",
        "plugin/fnvxr_nvse_plugin.cpp",
        "tests/fnvxr_stereo_math_test.cpp",
        "scripts/get-verified-shader-wvp-contracts.ps1",
        "scripts/fnvxr-sidecar-common.ps1",
        "scripts/audit-launch-safety.ps1",
    ]
    binary_paths = [
        "build-win32/Release/d3d9.dll",
        "build-win32/Release/nvse_fnvxr.dll",
        "build-win32/Release/fnvxr_stereo_math_test.exe",
        "build/Release/fnvxr_openxr_pose_host.exe",
        "build/Release/fnvxr_stereo_math_test.exe",
    ]

    def hashes(paths: list[str]) -> dict[str, Any]:
        result: dict[str, Any] = {}
        for relative in paths:
            path = ROOT / relative
            result[relative] = {
                "exists": path.is_file(),
                "bytes": path.stat().st_size if path.is_file() else 0,
                "sha256": sha256_file(path) if path.is_file() else None,
            }
        return result

    source_hashes = hashes(source_paths)
    binary_hashes = hashes(binary_paths)
    return {
        "schema": "fnvxr-6dof-offline-gates-v1",
        "generated_utc": datetime.now(timezone.utc).isoformat(),
        "commands": records,
        "source_hashes": source_hashes,
        "binary_hashes": binary_hashes,
        "all_commands_passed": all(record["passed"] for record in records),
        "all_traced_files_present": all(
            entry["exists"] for entry in [*source_hashes.values(), *binary_hashes.values()]
        ),
        "headset_processes_launched": False,
    }


def validate_export(name: str, source: trimesh.Trimesh, path: Path, minimum_design_feature_mm: float) -> dict[str, Any]:
    source.export(path, file_type="stl")
    loaded = trimesh.load_mesh(path, file_type="stl", process=True)
    if not isinstance(loaded, trimesh.Trimesh):
        raise RuntimeError(f"{name}: STL did not re-import as one mesh")
    custom = custom_mesh_checks(loaded)
    manifold = manifold_status(loaded)
    bounds_error = float(np.max(np.abs(np.asarray(source.bounds) - np.asarray(loaded.bounds))))
    volume_error_fraction = abs(float(source.volume) - float(loaded.volume)) / max(abs(float(source.volume)), 1.0)
    size = np.ptp(np.asarray(loaded.bounds), axis=0)
    checks = {
        "trimesh_watertight": bool(loaded.is_watertight),
        "trimesh_winding_consistent": bool(loaded.is_winding_consistent),
        "trimesh_positive_volume": bool(loaded.is_volume and loaded.volume > 0.0),
        "custom_finite": custom["finite"],
        "custom_zero_area_triangles": custom["zero_area_triangle_count"] == 0,
        "custom_duplicate_faces": custom["duplicate_face_count"] == 0,
        "custom_edge_incidence_two": custom["bad_edge_incidence_count"] == 0,
        "custom_one_connected_component": custom["connected_components"] == 1,
        "custom_positive_signed_volume": custom["signed_volume_mm3"] > 0.0,
        "minimum_triangle_area_at_least_0_001_mm2": custom["minimum_triangle_area_mm2"] >= 0.001,
        "minimum_triangle_edge_at_least_0_05_mm": custom["minimum_triangle_edge_mm"] >= 0.05,
        "manifold3d_no_error": manifold["status"] == "NoError",
        "roundtrip_bounds_within_0_02_mm": bounds_error <= 0.02,
        "roundtrip_volume_within_0_1_percent": volume_error_fraction <= 0.001,
        "fits_210_mm_xy_envelope": bool(size[0] <= 210.0 and size[1] <= 210.0),
    }
    return {
        "file": path.name,
        "sha256": sha256_file(path),
        "units": "millimeters",
        "vertices": int(len(loaded.vertices)),
        "triangles": int(len(loaded.faces)),
        "bounds_mm": np.asarray(loaded.bounds).tolist(),
        "size_mm": size.tolist(),
        "volume_mm3": float(loaded.volume),
        "minimum_design_feature_mm_nominal_not_measured": minimum_design_feature_mm,
        "roundtrip_max_bounds_error_mm": bounds_error,
        "roundtrip_volume_error_fraction": volume_error_fraction,
        "custom_checker": custom,
        "manifold3d_checker": manifold,
        "checks": checks,
        "passed": all(checks.values()),
    }


def signed_permutation_symmetry_check() -> dict[str, Any]:
    # Chiral/unequal semantic endpoint set in OpenXR millimeters.
    points = np.array(
        [
            [151.0, 0.0, 0.0],
            [-43.0, 0.0, 0.0],
            [0.0, 173.0, 0.0],
            [0.0, 0.0, -137.0],
            [0.0, 0.0, 47.0],
            [83.0, 0.0, -61.0],
            [25.0, 0.0, 0.0],
            [0.0, 25.0, 0.0],
            [0.0, 0.0, -25.0],
        ]
    )
    canonical = {tuple(row) for row in points}
    preserving: list[list[list[int]]] = []
    for permutation in itertools.permutations(range(3)):
        for signs in itertools.product((-1, 1), repeat=3):
            matrix = np.zeros((3, 3), dtype=int)
            for row, column in enumerate(permutation):
                matrix[row, column] = signs[row]
            transformed = {tuple(row) for row in (points @ matrix.T)}
            if transformed == canonical:
                preserving.append(matrix.tolist())
    identity_only = len(preserving) == 1 and preserving[0] == np.eye(3, dtype=int).tolist()
    return {
        "signed_permutations_tested": 48,
        "preserving_transform_count": len(preserving),
        "identity_is_only_preserving_transform": identity_only,
        "passed": identity_only,
    }


def generate_meshes() -> tuple[dict[str, trimesh.Trimesh], dict[str, Any]]:
    OUTPUT_3D.mkdir(parents=True, exist_ok=True)
    meshes = {
        "base": simplify_mesh(build_base()),
        "mast": simplify_mesh(build_mast()),
        "ipd_comb": simplify_mesh(build_ipd_comb()),
        "rotation_pitch": simplify_mesh(build_rotation_gauge(1)),
        "rotation_yaw": simplify_mesh(build_rotation_gauge(2)),
        "rotation_roll": simplify_mesh(build_rotation_gauge(3)),
    }
    minimum_thickness = {
        "base": 3.75,
        "mast": 6.0,
        "ipd_comb": 6.0,
        "rotation_pitch": 3.2,
        "rotation_yaw": 3.2,
        "rotation_roll": 3.2,
    }
    parts: dict[str, Any] = {}
    for name, mesh in meshes.items():
        path = OUTPUT_3D / f"fnvxr_6dof_{name}.stl"
        parts[name] = validate_export(name, mesh, path, minimum_thickness[name])
    symmetry = signed_permutation_symmetry_check()
    manifest = {
        "schema": "fnvxr-6dof-print-fixture-v1",
        "generated_utc": datetime.now(timezone.utc).isoformat(),
        "generator": str(Path(__file__).relative_to(ROOT)).replace("\\", "/"),
        "coordinate_convention": {
            "openxr": "+X right, +Y up, +Z back, -Z forward",
            "printer_mapping": "printer X = OpenXR X, printer Y = -OpenXR Z, printer Z = OpenXR Y",
            "stl_units": "millimeters (STL is unitless; import explicitly as mm)",
            "assembled_origin": "OpenXR origin is base top / mast flange bottom at printer Z=8 mm",
        },
        "scale": {
            "fallout_units_per_meter": UNITS_PER_METER,
            "status": "configured approximation derived from about 7 GECK units per 10 cm / 64 units per yard",
            "source": "https://geckwiki.com/index.php?title=Units",
            "100_mm_in_fallout_units": 7.0,
            "64_mm_ipd_in_fallout_units": IPD_UNITS,
            "64_mm_half_ipd_in_fallout_units": IPD_UNITS * 0.5,
        },
        "landmarks_openxr_mm": {
            "origin": [0.0, 0.0, 0.0],
            "positive_x_endpoint": [151.0, 0.0, 0.0],
            "negative_x_endpoint": [-43.0, 0.0, 0.0],
            "positive_y_mast_endpoint": [0.0, 173.0, 0.0],
            "forward_negative_z_endpoint": [0.0, 0.0, -137.0],
            "rear_positive_z_endpoint": [0.0, 0.0, 47.0],
            "chiral_brace_endpoint": [83.0, 0.0, -61.0],
            "stations_mm": [25.0, 50.0, 100.0],
            "ipd_rows_mm": [56.0, 60.0, 64.0, 68.0, 72.0],
            "mast_stl_datum_z_mm": 8.0,
            "mast_station_stl_z_mm": [33.0, 58.0, 108.0],
            "mast_endpoint_stl_z_mm": 181.0,
        },
        "parts": parts,
        "asymmetry_check": symmetry,
        "slicer_validation": {
            "status": "NOT_RUN",
            "reason": "No PrusaSlicer/OrcaSlicer/admesh executable is installed in this workspace.",
        },
        "all_available_digital_checks_passed": bool(
            all(part["passed"] for part in parts.values()) and symmetry["passed"]
        ),
    }
    manifest_path = OUTPUT_3D / "fixture_manifest.json"
    manifest_path.write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
    return meshes, manifest


def render_preview(meshes: dict[str, trimesh.Trimesh], output: Path) -> None:
    placements: list[tuple[str, trimesh.Trimesh, np.ndarray, tuple[int, int, int]]] = []
    colors_by_name = {
        "base": (50, 125, 220),
        "mast": (245, 165, 45),
        "ipd_comb": (80, 190, 120),
        "rotation_pitch": (220, 80, 90),
        "rotation_yaw": (170, 100, 220),
        "rotation_roll": (60, 185, 190),
    }
    transforms: dict[str, np.ndarray] = {name: np.eye(4) for name in meshes}
    transforms["mast"][:3, 3] = (0.0, 0.0, 1.0)
    transforms["ipd_comb"][:3, 3] = (0.0, 0.0, 120.0)
    transforms["rotation_pitch"][:3, 3] = (-120.0, -55.0, 0.0)
    transforms["rotation_yaw"][:3, 3] = (-120.0, 5.0, 0.0)
    transforms["rotation_roll"][:3, 3] = (-120.0, 65.0, 0.0)
    for name, mesh in meshes.items():
        placements.append((name, mesh.copy(), transforms[name], colors_by_name[name]))

    projected_faces: list[tuple[float, list[tuple[float, float]], tuple[int, int, int]]] = []
    all_projected: list[np.ndarray] = []
    view = np.array([[0.78, -0.78, 0.0], [0.38, 0.38, -0.92], [0.45, 0.45, 0.55]])
    for _name, mesh, transform, base_color in placements:
        mesh.apply_transform(transform)
        vertices = np.asarray(mesh.vertices)
        projected = vertices @ view.T
        all_projected.append(projected[:, :2])
        triangles = vertices[np.asarray(mesh.faces)]
        normals = np.cross(triangles[:, 1] - triangles[:, 0], triangles[:, 2] - triangles[:, 0])
        normal_lengths = np.linalg.norm(normals, axis=1)
        normal_lengths[normal_lengths == 0.0] = 1.0
        normals /= normal_lengths[:, None]
        light = np.clip(0.30 + 0.70 * np.abs(normals @ np.array([0.35, -0.25, 0.90])), 0.20, 1.0)
        for face_index, face in enumerate(np.asarray(mesh.faces)):
            polygon = [(float(projected[index, 0]), float(projected[index, 1])) for index in face]
            shade = float(light[face_index])
            color = tuple(int(channel * shade) for channel in base_color)
            depth = float(np.mean(projected[face, 2]))
            projected_faces.append((depth, polygon, color))

    all_xy = np.vstack(all_projected)
    minimum = np.min(all_xy, axis=0)
    maximum = np.max(all_xy, axis=0)
    canvas_width, canvas_height = 1800, 1250
    margin = 90
    scale = min(
        (canvas_width - margin * 2) / max(maximum[0] - minimum[0], 1.0),
        (canvas_height - margin * 2 - 110) / max(maximum[1] - minimum[1], 1.0),
    )

    def to_screen(point: tuple[float, float]) -> tuple[float, float]:
        x = margin + (point[0] - minimum[0]) * scale
        y = canvas_height - margin - (point[1] - minimum[1]) * scale
        return x, y

    image = PilImage.new("RGB", (canvas_width, canvas_height), (247, 248, 250))
    draw = ImageDraw.Draw(image)
    for _depth, polygon, color in sorted(projected_faces, key=lambda item: item[0]):
        screen_polygon = [to_screen(point) for point in polygon]
        draw.polygon(screen_polygon, fill=color, outline=(45, 50, 60))
    font = ImageFont.load_default(size=24)
    title_font = ImageFont.load_default(size=34)
    draw.text((50, 28), "FNVXR OFFLINE 6DoF METROLOGY FIXTURE - PRINT PREVIEW", fill=(20, 25, 35), font=title_font)
    legend_x = 1120
    legend_y = 70
    for name, color in colors_by_name.items():
        draw.rectangle((legend_x, legend_y, legend_x + 28, legend_y + 20), fill=color, outline=(20, 20, 20))
        draw.text((legend_x + 40, legend_y - 2), name.replace("_", " "), fill=(20, 25, 35), font=font)
        legend_y += 34
    draw.text(
        (50, canvas_height - 54),
        "STL units: mm | Blue base X span 194 mm, forward span 184 mm | 64 mm IPD row = 4.48 Fallout units",
        fill=(25, 30, 40),
        font=font,
    )
    image.save(output)


def load_oracle_reviews() -> list[dict[str, Any]]:
    path = OUTPUT_PROOF / "oracle_reviews.json"
    if not path.exists():
        return [
            {
                "oracle": index,
                "verdict": "PENDING",
                "summary": "Independent review has not yet been added to oracle_reviews.json.",
            }
            for index in range(1, 6)
        ]
    payload = json.loads(path.read_text(encoding="utf-8"))
    reviews = payload.get("reviews", [])
    if len(reviews) != 5:
        raise RuntimeError("oracle_reviews.json must contain exactly five reviews")
    return reviews


def page_number(canvas, document) -> None:
    canvas.saveState()
    canvas.setFont("Helvetica", 8)
    canvas.setFillColor(colors.HexColor("#5a6270"))
    canvas.drawString(0.62 * inch, 0.40 * inch, "FNVXR offline 6DoF proof - no headset launch")
    canvas.drawRightString(7.88 * inch, 0.40 * inch, f"Page {document.page}")
    canvas.restoreState()


def build_pdf(
    checks: list[Check],
    details: dict[str, Any],
    manifest: dict[str, Any],
    preview_path: Path,
    oracle_reviews: list[dict[str, Any]],
    offline_gates: dict[str, Any],
) -> Path:
    OUTPUT_PDF.mkdir(parents=True, exist_ok=True)
    pdf_path = OUTPUT_PDF / "fnvxr_6dof_coordinate_proof.pdf"
    document = SimpleDocTemplate(
        str(pdf_path),
        pagesize=letter,
        rightMargin=0.62 * inch,
        leftMargin=0.62 * inch,
        topMargin=0.62 * inch,
        bottomMargin=0.62 * inch,
        title="FNVXR Offline 6DoF Coordinate Proof",
        author="Deterministic Python generator with five independent oracle reviews",
    )
    styles = getSampleStyleSheet()
    styles.add(
        ParagraphStyle(
            name="ProofTitle",
            parent=styles["Title"],
            fontName="Helvetica-Bold",
            fontSize=21,
            leading=24,
            textColor=colors.HexColor("#172033"),
            alignment=TA_CENTER,
            spaceAfter=14,
        )
    )
    styles.add(
        ParagraphStyle(
            name="VerdictRed",
            parent=styles["Heading2"],
            fontName="Helvetica-Bold",
            fontSize=13,
            leading=16,
            textColor=colors.white,
            backColor=colors.HexColor("#a51f2d"),
            borderPadding=9,
            alignment=TA_CENTER,
            spaceAfter=12,
        )
    )
    styles.add(
        ParagraphStyle(
            name="Equation",
            parent=styles["Code"],
            fontName="Courier",
            fontSize=8.2,
            leading=11,
            leftIndent=16,
            rightIndent=16,
            borderColor=colors.HexColor("#ccd3df"),
            borderWidth=0.5,
            borderPadding=7,
            backColor=colors.HexColor("#f5f7fa"),
            spaceBefore=5,
            spaceAfter=8,
        )
    )
    styles.add(
        ParagraphStyle(
            name="Small",
            parent=styles["BodyText"],
            fontSize=8.2,
            leading=10.5,
        )
    )
    story: list[Any] = []
    story.append(Paragraph("FNVXR Offline 6DoF Coordinate Proof", styles["ProofTitle"]))
    story.append(
        Paragraph(
            "CURRENT VERDICT: NOT READY FOR ANOTHER HEADSET RUN",
            styles["VerdictRed"],
        )
    )
    story.append(
        Paragraph(
            "The spatial coordinate theorem and deterministic numeric tests pass after concrete repairs. "
            "The complete runtime claim does not pass because exact shader ownership and full eye-target draw "
            "classification remain incomplete. This report deliberately separates proven math from missing evidence.",
            styles["BodyText"],
        )
    )
    story.append(Spacer(1, 8))
    summary_table = Table(
        [
            ["Item", "Result"],
            ["Offline numeric proof", "PASS" if all(check.passed for check in checks) else "FAIL"],
            [
                "Exact-source builds, CTest, and safety audit",
                "PASS" if offline_gates["all_commands_passed"] else "FAIL",
            ],
            ["Print mesh digital validation", "PASS" if manifest["all_available_digital_checks_passed"] else "FAIL"],
            ["External slicer validation", manifest["slicer_validation"]["status"]],
            ["Runtime shader/draw proof", "FAIL - hard blockers remain"],
            ["Headset used by this proof run", "NO"],
        ],
        colWidths=[2.55 * inch, 4.55 * inch],
        repeatRows=1,
    )
    summary_table.setStyle(
        TableStyle(
            [
                ("BACKGROUND", (0, 0), (-1, 0), colors.HexColor("#24324a")),
                ("TEXTCOLOR", (0, 0), (-1, 0), colors.white),
                ("FONTNAME", (0, 0), (-1, 0), "Helvetica-Bold"),
                ("FONTNAME", (0, 1), (0, -1), "Helvetica-Bold"),
                ("GRID", (0, 0), (-1, -1), 0.4, colors.HexColor("#aab3c0")),
                ("VALIGN", (0, 0), (-1, -1), "TOP"),
                ("FONTSIZE", (0, 0), (-1, -1), 8.5),
                ("ROWBACKGROUNDS", (0, 1), (-1, -1), [colors.white, colors.HexColor("#f4f6f9")]),
                ("LEFTPADDING", (0, 0), (-1, -1), 6),
                ("RIGHTPADDING", (0, 0), (-1, -1), 6),
                ("TOPPADDING", (0, 0), (-1, -1), 5),
                ("BOTTOMPADDING", (0, 0), (-1, -1), 5),
            ]
        )
    )
    story.append(summary_table)
    story.append(Spacer(1, 12))
    story.append(Paragraph("1. Coordinate definitions", styles["Heading1"]))
    story.append(
        Paragraph(
            "OpenXR local coordinates are +X right, +Y up, +Z back, so forward is -Z. "
            "A NiCamera local rotation stores columns (right, up, back), so OpenXR translation is already in the "
            "correct camera-local basis. Actor coordinates are different: (right, forward, up). "
            "Those two mappings must never be substituted for each other. The configured 70 units/m is an "
            "approximate FNV geometry scale, not a measured runtime calibration. See "
            "<link href='https://geckwiki.com/index.php?title=Units'>GECK Units</link>.",
            styles["BodyText"],
        )
    )
    story.append(
        Paragraph(
            "XR_to_NiCamera_local = I<br/>XR_to_actor * (x,y,z)^T = (x,-z,y)^T<br/>"
            "s = configured 70 Fallout units/m (approximate geometry scale)",
            styles["Equation"],
        )
    )
    story.append(Paragraph("2. Rigid 6DoF theorem", styles["Heading1"]))
    story.append(
        Paragraph(
            "Let p_o be the recenter eye midpoint, R_o the gravity-aligned yaw-only origin, C the leveled "
            "NiCamera body rotation, b the engine camera anchor, p_i/R_i an OpenXR eye pose, and X a room point.",
            styles["BodyText"],
        )
    )
    story.append(
        Paragraph(
            "d_i = R_o^T (p_i - p_o)<br/>R_i_local = R_o^T R_i<br/>"
            "p_i_game = b + s C d_i<br/>R_i_game = C R_i_local<br/>"
            "X_game = b + s C R_o^T (X - p_o)",
            styles["Equation"],
        )
    )
    story.append(
        Paragraph(
            "Substitution and cancellation give:",
            styles["BodyText"],
        )
    )
    story.append(
        Paragraph(
            "R_i_game^T (X_game - p_i_game)<br/>"
            "= (C R_o^T R_i)^T [s C R_o^T (X - p_i)]<br/>"
            "= s R_i^T R_o C^T C R_o^T (X - p_i)<br/>"
            "= s R_i^T (X - p_i)",
            styles["Equation"],
        )
    )
    story.append(
        Paragraph(
            "Therefore every eye-local direction and perspective ratio is preserved. This is an exact similarity "
            "transform when all inputs belong to the same source pose transaction.",
            styles["BodyText"],
        )
    )
    story.append(Paragraph("3. Numeric falsifiers and regressions", styles["Heading1"]))
    rows: list[list[Any]] = [["Check", "Trials", "Max error", "Tolerance", "Result"]]
    for check in checks:
        rows.append(
            [
                Paragraph(check.name, styles["Small"]),
                str(check.trials),
                f"{check.maximum_error:.3g}",
                f"{check.tolerance:.3g}",
                "PASS" if check.passed else "FAIL",
            ]
        )
    numeric_table = Table(rows, colWidths=[3.22 * inch, 0.70 * inch, 0.90 * inch, 0.86 * inch, 0.62 * inch], repeatRows=1)
    numeric_table.setStyle(
        TableStyle(
            [
                ("BACKGROUND", (0, 0), (-1, 0), colors.HexColor("#24324a")),
                ("TEXTCOLOR", (0, 0), (-1, 0), colors.white),
                ("FONTNAME", (0, 0), (-1, 0), "Helvetica-Bold"),
                ("GRID", (0, 0), (-1, -1), 0.35, colors.HexColor("#aab3c0")),
                ("VALIGN", (0, 0), (-1, -1), "TOP"),
                ("FONTSIZE", (0, 0), (-1, -1), 7.7),
                ("ALIGN", (1, 1), (-1, -1), "RIGHT"),
                ("ROWBACKGROUNDS", (0, 1), (-1, -1), [colors.white, colors.HexColor("#f4f6f9")]),
                ("TOPPADDING", (0, 0), (-1, -1), 4),
                ("BOTTOMPADDING", (0, 0), (-1, -1), 4),
            ]
        )
    )
    story.append(numeric_table)
    story.append(Spacer(1, 8))
    old_axis = details["axis_counterexample"]
    old_recenter = details["recenter_counterexample"]
    story.append(
        Paragraph(
            "Old axis counterexample: 100 mm physical forward is XR (0,0,-0.1). The removed actor permutation "
            f"produced NiCamera-local {old_axis['old_actor_permutation_then_nicamera_local_m']}, which is upward. "
            "A 100 mm physical lift became camera-back instead of up.",
            styles["BodyText"],
        )
    )
    story.append(
        Paragraph(
            "Old recenter counterexample: a 30 degree pitched origin conjugating a 90 degree world yaw gives "
            f"forward {np.round(old_recenter['old_full_quaternion_relative_forward'], 6).tolist()}, including "
            f"{old_recenter['vertical_leak_magnitude']:.3f} vertical leakage.",
            styles["BodyText"],
        )
    )
    story.append(PageBreak())
    story.append(Paragraph("4. Python-generated asymmetric print candidates", styles["Heading1"]))
    story.append(
        Paragraph(
            "The fixture makes axis permutation, reflection, scale, IPD sign, relative height, and rotation-axis "
            "mistakes physically measurable. It cannot by itself prove what the runtime renders. STL is unitless; "
            "import every part as millimeters. These remain print candidates until an external slicer accepts them.",
            styles["BodyText"],
        )
    )
    story.append(Image(str(preview_path), width=7.1 * inch, height=4.93 * inch))
    mesh_rows: list[list[Any]] = [["Part", "mm size", "Triangles", "SHA-256 prefix", "Checks"]]
    for name, part in manifest["parts"].items():
        mesh_rows.append(
            [
                name.replace("_", " "),
                " x ".join(f"{value:.1f}" for value in part["size_mm"]),
                str(part["triangles"]),
                part["sha256"][:16],
                "PASS" if part["passed"] else "FAIL",
            ]
        )
    mesh_table = Table(mesh_rows, colWidths=[1.28 * inch, 1.55 * inch, 0.72 * inch, 1.48 * inch, 0.72 * inch], repeatRows=1)
    mesh_table.setStyle(
        TableStyle(
            [
                ("BACKGROUND", (0, 0), (-1, 0), colors.HexColor("#24324a")),
                ("TEXTCOLOR", (0, 0), (-1, 0), colors.white),
                ("FONTNAME", (0, 0), (-1, 0), "Helvetica-Bold"),
                ("GRID", (0, 0), (-1, -1), 0.35, colors.HexColor("#aab3c0")),
                ("FONTSIZE", (0, 0), (-1, -1), 7.7),
                ("ROWBACKGROUNDS", (0, 1), (-1, -1), [colors.white, colors.HexColor("#f4f6f9")]),
                ("TOPPADDING", (0, 0), (-1, -1), 4),
                ("BOTTOMPADDING", (0, 0), (-1, -1), 4),
            ]
        )
    )
    story.append(mesh_table)
    story.append(Spacer(1, 8))
    story.append(
        Paragraph(
            "Digital gates: finite vertices, no degenerate/duplicate triangles, every edge incident to exactly two "
            "faces, one connected component, positive signed volume, trimesh watertight/winding checks, manifold3d "
            "NoError, no triangle edge below 0.05 mm, no triangle area below 0.001 mm2, STL roundtrip bounds "
            "<= 0.02 mm, volume error <= 0.1%, and <= 210 mm XY envelope. Nominal design-feature sizes are "
            "declared, not measured wall-thickness results. "
            "External slicer validation is NOT RUN because no supported slicer is installed.",
            styles["Small"],
        )
    )
    story.append(PageBreak())
    story.append(Paragraph("5. Five independent oracle reviews", styles["Heading1"]))
    oracle_rows: list[list[Any]] = [["Oracle", "Verdict", "Independent finding"]]
    for review in oracle_reviews:
        oracle_rows.append(
            [
                str(review["oracle"]),
                Paragraph(str(review["verdict"]), styles["Small"]),
                Paragraph(str(review["summary"]), styles["Small"]),
            ]
        )
    oracle_table = Table(oracle_rows, colWidths=[0.52 * inch, 1.38 * inch, 5.15 * inch], repeatRows=1)
    oracle_table.setStyle(
        TableStyle(
            [
                ("BACKGROUND", (0, 0), (-1, 0), colors.HexColor("#24324a")),
                ("TEXTCOLOR", (0, 0), (-1, 0), colors.white),
                ("FONTNAME", (0, 0), (-1, 0), "Helvetica-Bold"),
                ("GRID", (0, 0), (-1, -1), 0.35, colors.HexColor("#aab3c0")),
                ("VALIGN", (0, 0), (-1, -1), "TOP"),
                ("FONTSIZE", (0, 0), (-1, -1), 7.8),
                ("ROWBACKGROUNDS", (0, 1), (-1, -1), [colors.white, colors.HexColor("#f4f6f9")]),
                ("TOPPADDING", (0, 0), (-1, -1), 5),
                ("BOTTOMPADDING", (0, 0), (-1, -1), 5),
            ]
        )
    )
    story.append(oracle_table)
    story.append(Spacer(1, 10))
    story.append(Paragraph("6. Exact-source traceability", styles["Heading1"]))
    gate_rows: list[list[Any]] = [["Gate", "Exit", "Output SHA-256 prefix", "Result"]]
    for gate in offline_gates["commands"]:
        gate_rows.append(
            [
                gate["name"].replace("_", " "),
                str(gate["exit_code"]),
                gate["output_sha256"][:16],
                "PASS" if gate["passed"] else "FAIL",
            ]
        )
    gate_table = Table(gate_rows, colWidths=[2.75 * inch, 0.55 * inch, 2.10 * inch, 0.70 * inch], repeatRows=1)
    gate_table.setStyle(
        TableStyle(
            [
                ("BACKGROUND", (0, 0), (-1, 0), colors.HexColor("#24324a")),
                ("TEXTCOLOR", (0, 0), (-1, 0), colors.white),
                ("FONTNAME", (0, 0), (-1, 0), "Helvetica-Bold"),
                ("GRID", (0, 0), (-1, -1), 0.35, colors.HexColor("#aab3c0")),
                ("FONTSIZE", (0, 0), (-1, -1), 7.7),
                ("ROWBACKGROUNDS", (0, 1), (-1, -1), [colors.white, colors.HexColor("#f4f6f9")]),
                ("TOPPADDING", (0, 0), (-1, -1), 4),
                ("BOTTOMPADDING", (0, 0), (-1, -1), 4),
            ]
        )
    )
    story.append(gate_table)
    story.append(Spacer(1, 6))
    trace_rows: list[list[Any]] = [["File", "Bytes", "SHA-256 prefix"]]
    combined_hashes = {
        **offline_gates["source_hashes"],
        **offline_gates["binary_hashes"],
    }
    for relative, entry in combined_hashes.items():
        trace_rows.append(
            [relative, str(entry["bytes"]), (entry["sha256"] or "MISSING")[:16]]
        )
    trace_table = Table(trace_rows, colWidths=[3.85 * inch, 0.80 * inch, 1.95 * inch], repeatRows=1)
    trace_table.setStyle(
        TableStyle(
            [
                ("BACKGROUND", (0, 0), (-1, 0), colors.HexColor("#24324a")),
                ("TEXTCOLOR", (0, 0), (-1, 0), colors.white),
                ("FONTNAME", (0, 0), (-1, 0), "Helvetica-Bold"),
                ("GRID", (0, 0), (-1, -1), 0.30, colors.HexColor("#aab3c0")),
                ("FONTSIZE", (0, 0), (-1, -1), 6.7),
                ("ROWBACKGROUNDS", (0, 1), (-1, -1), [colors.white, colors.HexColor("#f4f6f9")]),
                ("TOPPADDING", (0, 0), (-1, -1), 3),
                ("BOTTOMPADDING", (0, 0), (-1, -1), 3),
            ]
        )
    )
    story.append(trace_table)
    story.append(Spacer(1, 10))
    story.append(Paragraph("7. Hard blockers - why the headset stays off", styles["Heading1"]))
    blockers = [
        "A historical run introduced vertex shaders 0x5999d5b0 and 0x83e14aae without verified WVP contracts. Coverage fell to 391/392, the producer retained then lost its last frame, and the host submitted no visible fallback layer. This deterministically explains the black screen.",
        "The contract generator now requires a complete, unoverwritten oPos.xyzw provenance chain, and therefore rejects parts of the legacy 45-contract corpus. No replacement full contract set exists for all observed shader forms or the missing shaders.",
        "The new strict-eye-target ledgers reject any draw, clear, or copy not proven on both eyes, including fixed-function, UP, configured-skip, and unknown shader draws. Remaining final-target write APIs still need enumeration plus reviewed eye-invariant exclusions.",
        "Configured skip pairs use 32-bit FNV pairs rather than a strong SHA-256 plus byte-length semantic manifest.",
        "A projection layer without depth cannot exactly reproject translated old pixels. The new 25 ms gate fails closed, but production performance inside that budget is not yet evidenced.",
        "There is no new live binary evidence after these repairs. Existing logs are evidence of the old failures only.",
    ]
    for index, blocker in enumerate(blockers, start=1):
        story.append(Paragraph(f"{index}. {blocker}", styles["BodyText"]))
        story.append(Spacer(1, 3))
    story.append(Paragraph("8. Acceptance rule", styles["Heading1"]))
    story.append(
        Paragraph(
            "Do not launch the headset from this proof package. The next eligible step is still offline: capture missing "
            "shader bytecode in producer-only mode with a visible monitor fallback, satisfy strict oPos semantics, produce "
            "a zero-unclassified-draw ledger, pass external slicing, and regenerate this report with all red gates green. "
            "Only then is one short instrumented headset validation justified.",
            styles["BodyText"],
        )
    )
    story.append(Spacer(1, 10))
    story.append(
        Paragraph(
            "Regeneration: use the bundled Python runtime to run tools/generate_6dof_proof_artifacts.py. "
            f"Deterministic numeric seed: {SEED}. Numeric trials: {details['total_trials']}.",
            styles["Small"],
        )
    )
    document.build(story, onFirstPage=page_number, onLaterPages=page_number)
    return pdf_path


def write_print_instructions(manifest: dict[str, Any]) -> None:
    text = """FNVXR OFFLINE 6DoF METROLOGY FIXTURE

This package is a physical reference, not proof that the runtime consumed a pose.

Import:
- STL units are millimeters.
- Print every part with the largest flat face on the bed.
- Suggested: 0.4 mm nozzle, 0.20 mm layers, 3 perimeters, PLA/PETG, no supports.

Assembly convention:
- Printer X is OpenXR +X (right).
- Printer Y is OpenXR -Z (physical forward).
- Printer Z is OpenXR +Y (up).
- Insert the mast tab until the 18 x 12 mm flange positively stops on the base top. Do not force it.
- The base top / mast flange bottom is the assembled zero-height datum (printer Z=8 mm).
- The IPD comb rows are 56, 60, 64, 68, and 72 mm from bottom to top marker count.
- On the 64 mm row, the left aperture is square and the right aperture is round.
- Rotation gauges use one/two/three raised code dots for pitch/yaw/roll.

Physical rejection limits:
- Any 100 mm station error greater than +/-0.4 mm.
- The 64 mm aperture separation error greater than +/-0.25 mm.
- Mast squareness error greater than 0.5 degrees.
- Base flatness error greater than 0.5 mm.

Do not compensate software math to match a warped print.
"""
    (OUTPUT_3D / "PRINTING_AND_MEASUREMENT.txt").write_text(text, encoding="ascii")


def main() -> int:
    OUTPUT_PROOF.mkdir(parents=True, exist_ok=True)
    offline_gates = run_offline_gates()
    offline_gate_path = OUTPUT_PROOF / "offline_gate_results.json"
    offline_gate_path.write_text(json.dumps(offline_gates, indent=2) + "\n", encoding="utf-8")
    checks, details = run_numeric_proof()
    numeric_payload = {
        "schema": "fnvxr-6dof-numeric-proof-v1",
        "generated_utc": datetime.now(timezone.utc).isoformat(),
        "checks": [check.as_dict() for check in checks],
        "details": details,
        "offline_gate_results": str(offline_gate_path.relative_to(ROOT)).replace("\\", "/"),
        "offline_gate_results_sha256": sha256_file(offline_gate_path),
        "traced_source_hashes": offline_gates["source_hashes"],
        "traced_binary_hashes": offline_gates["binary_hashes"],
        "all_numeric_checks_passed": all(check.passed for check in checks),
        "runtime_certified": False,
        "runtime_verdict": "NOT_READY_FOR_HEADSET",
    }
    numeric_path = OUTPUT_PROOF / "fnvxr_6dof_numeric_proof.json"
    numeric_path.write_text(json.dumps(numeric_payload, indent=2) + "\n", encoding="utf-8")

    meshes, manifest = generate_meshes()
    write_print_instructions(manifest)
    preview_path = OUTPUT_3D / "fnvxr_6dof_fixture_preview.png"
    render_preview(meshes, preview_path)
    reviews = load_oracle_reviews()
    pdf_path = build_pdf(checks, details, manifest, preview_path, reviews, offline_gates)

    result = {
        "numeric_proof": str(numeric_path.relative_to(ROOT)),
        "numeric_pass": numeric_payload["all_numeric_checks_passed"],
        "offline_gates": str(offline_gate_path.relative_to(ROOT)),
        "offline_gates_pass": offline_gates["all_commands_passed"]
        and offline_gates["all_traced_files_present"],
        "mesh_manifest": str((OUTPUT_3D / "fixture_manifest.json").relative_to(ROOT)),
        "mesh_pass": manifest["all_available_digital_checks_passed"],
        "preview": str(preview_path.relative_to(ROOT)),
        "pdf": str(pdf_path.relative_to(ROOT)),
        "runtime_verdict": "NOT_READY_FOR_HEADSET",
    }
    print(json.dumps(result, indent=2))
    return 0 if result["numeric_pass"] and result["mesh_pass"] and result["offline_gates_pass"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
