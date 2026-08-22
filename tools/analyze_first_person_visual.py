#!/usr/bin/env python3
"""Fixture-specific pixel gate for FNVXR submitted side-by-side eyes."""

from __future__ import annotations

import argparse
import json
import math
import statistics
import sys
from pathlib import Path

try:
    import cv2
    import numpy as np
except ImportError as exc:
    raise SystemExit("requires OpenCV (cv2) and numpy") from exc


SPATIAL_ALPHA_TAGS = {
    "left_hand": 51,
    "right_hand": 102,
    "pipboy_housing": 153,
    "pipboy_screen": 204,
}


def largest_component_ratio(mask: "np.ndarray") -> float:
    count, _labels, stats, _centroids = cv2.connectedComponentsWithStats(
        mask.astype(np.uint8), 8
    )
    if count <= 1:
        return 0.0
    largest = int(stats[1:, cv2.CC_STAT_AREA].max())
    return largest / float(mask.shape[0] * mask.shape[1])


def eye_rois(frame: "np.ndarray", args: argparse.Namespace) -> list["np.ndarray"]:
    height, width = frame.shape[:2]
    top = max(0, args.content_top)
    bottom = height if args.content_height <= 0 else min(
        height, top + args.content_height
    )
    content = frame[top:bottom]
    if content.size == 0 or content.shape[1] < 2:
        raise ValueError("content crop cannot contain two eyes")
    eye_width = content.shape[1] // 2
    roi_top = int(content.shape[0] * args.roi_top_ratio)
    return [
        content[roi_top:, :eye_width],
        content[roi_top:, eye_width : eye_width * 2],
    ]


def crop_single_eye(frame: "np.ndarray", args: argparse.Namespace) -> "np.ndarray":
    height = frame.shape[0]
    top = max(0, args.content_top)
    bottom = height if args.content_height <= 0 else min(
        height, top + args.content_height
    )
    content = frame[top:bottom]
    if content.size == 0:
        raise ValueError("content crop is empty")
    roi_top = int(content.shape[0] * args.roi_top_ratio)
    return content[roi_top:]


def color_and_alpha(frame: "np.ndarray") -> tuple["np.ndarray", "np.ndarray | None"]:
    if frame.ndim != 3 or frame.shape[2] not in (3, 4):
        raise ValueError("expected a BGR or BGRA image")
    if frame.shape[2] == 4:
        return frame[:, :, :3], frame[:, :, 3]
    return frame, None


def paired_eye_paths(directory: Path) -> list[tuple[Path, Path]]:
    pairs = []
    for left in sorted(directory.glob("pair_*_left.png")):
        right = left.with_name(left.name.replace("_left.png", "_right.png"))
        if not right.is_file():
            raise ValueError(f"missing right eye for {left.name}")
        pairs.append((left, right))
    if not pairs:
        raise ValueError(f"no complete eye pairs found in {directory}")
    return pairs


def bbox_overlap_fraction(
    first: dict[str, int] | None,
    second: dict[str, int] | None,
) -> float:
    if first is None or second is None:
        return 0.0
    width = max(
        0,
        min(first["right"], second["right"])
        - max(first["left"], second["left"]),
    )
    height = max(
        0,
        min(first["bottom"], second["bottom"])
        - max(first["top"], second["top"]),
    )
    first_area = max(0, first["right"] - first["left"]) * max(
        0, first["bottom"] - first["top"]
    )
    second_area = max(0, second["right"] - second["left"]) * max(
        0, second["bottom"] - second["top"]
    )
    denominator = min(first_area, second_area)
    return width * height / float(denominator) if denominator else 0.0


def bbox_gap_pixels(
    first: dict[str, int] | None,
    second: dict[str, int] | None,
) -> float:
    if first is None or second is None:
        return float("inf")
    horizontal = max(
        0,
        first["left"] - second["right"],
        second["left"] - first["right"],
    )
    vertical = max(
        0,
        first["top"] - second["bottom"],
        second["top"] - first["bottom"],
    )
    return math.hypot(horizontal, vertical)


def masks(roi: "np.ndarray") -> dict[str, "np.ndarray"]:
    hue, saturation, value = cv2.split(cv2.cvtColor(roi, cv2.COLOR_BGR2HSV))
    return {
        "skin_component_ratio": (
            (hue >= 3)
            & (hue <= 24)
            & (saturation >= 40)
            & (saturation <= 190)
            & (value >= 45)
            & (value <= 245)
        ),
        "brown_component_ratio": (
            (hue >= 5)
            & (hue <= 25)
            & (saturation >= 60)
            & (value >= 35)
            & (value <= 180)
        ),
        "bright_component_ratio": (saturation < 45) & (value > 120),
        "red_ratio": (
            ((hue <= 8) | (hue >= 170))
            & (saturation >= 90)
            & (value >= 60)
        ),
    }


def analyze(args: argparse.Namespace) -> dict[str, object]:
    source = Path(args.video or args.image or args.pair_directory).resolve()
    keys = (
        "skin_component_ratio",
        "brown_component_ratio",
        "bright_component_ratio",
        "red_ratio",
        "red_jump",
    )
    worst = [{key: 0.0 for key in keys} for _ in range(2)]
    worst_at = [{key: None for key in keys} for _ in range(2)]
    previous_red = [0.0, 0.0]
    frame_count = 0
    morphology_samples = 0
    fps = 0.0
    spatial_values = [
        {name: [] for name in SPATIAL_ALPHA_TAGS}
        for _ in range(2)
    ]
    spatial_smallest = [
        {name: None for name in SPATIAL_ALPHA_TAGS}
        for _ in range(2)
    ]
    spatial_bboxes = [
        {name: [] for name in SPATIAL_ALPHA_TAGS}
        for _ in range(2)
    ]
    screen_content = [
        {
            "nonblack_fraction": [],
            "blue_dominant_fraction": [],
            "mean_luma": [],
        }
        for _ in range(2)
    ]
    world_background = [
        {
            "nonblack_fraction": [],
            "luma_stddev": [],
        }
        for _ in range(2)
    ]

    def consume(
        rois: list["np.ndarray"],
        alpha_rois: list["np.ndarray | None"],
        index: int,
        seconds: float,
    ) -> None:
        nonlocal frame_count, morphology_samples
        for eye, roi in enumerate(rois):
            selected = masks(roi)
            alpha = alpha_rois[eye]
            if args.spatial_overlay and alpha is None:
                raise ValueError(
                    "spatial-overlay analysis requires retained BGRA eye pixels"
                )
            if args.spatial_overlay:
                housing = (
                    np.abs(
                        alpha.astype(np.int16)
                        - SPATIAL_ALPHA_TAGS["pipboy_housing"]
                    )
                    <= args.alpha_tag_tolerance
                )
                red = float((selected["red_ratio"] & housing).mean())
            else:
                red = float(selected["red_ratio"].mean())
            jump = abs(red - previous_red[eye]) if index else 0.0
            previous_red[eye] = red
            for key, value in (("red_ratio", red), ("red_jump", jump)):
                if value > worst[eye][key]:
                    worst[eye][key] = value
                    worst_at[eye][key] = {"frame": index, "time_seconds": seconds}
            if index % args.morphology_step == 0:
                for key in keys[:3]:
                    value = largest_component_ratio(selected[key])
                    if value > worst[eye][key]:
                        worst[eye][key] = value
                        worst_at[eye][key] = {
                            "frame": index,
                            "time_seconds": seconds,
                        }
            if args.spatial_overlay:
                area = float(alpha.shape[0] * alpha.shape[1])
                overlay_tagged = np.zeros(alpha.shape, dtype=bool)
                for name, tag in SPATIAL_ALPHA_TAGS.items():
                    tagged = np.abs(alpha.astype(np.int16) - tag) <= args.alpha_tag_tolerance
                    overlay_tagged |= tagged
                    ratio = float(tagged.sum()) / area
                    spatial_values[eye][name].append(ratio)
                    ys, xs = np.nonzero(tagged)
                    bbox = None if not len(xs) else {
                        "left": int(xs.min()),
                        "top": int(ys.min()),
                        "right": int(xs.max()) + 1,
                        "bottom": int(ys.max()) + 1,
                    }
                    spatial_bboxes[eye][name].append(bbox)
                    previous = spatial_smallest[eye][name]
                    if previous is None or ratio < previous["ratio"]:
                        spatial_smallest[eye][name] = {
                            "ratio": ratio,
                            "frame": index,
                            "time_seconds": seconds,
                            "bbox": bbox,
                        }
                screen_tagged = (
                    np.abs(
                        alpha.astype(np.int16)
                        - SPATIAL_ALPHA_TAGS["pipboy_screen"]
                    )
                    <= args.alpha_tag_tolerance
                )
                if screen_tagged.any():
                    pixels = roi[screen_tagged]
                    nonblack = float((pixels.max(axis=1) > 12).mean())
                    blue_dominant = float(
                        (
                            (pixels[:, 0] > 50)
                            & (pixels[:, 0] > pixels[:, 1] * 1.10)
                            & (pixels[:, 0] > pixels[:, 2] * 1.10)
                        ).mean()
                    )
                    mean_luma = float(
                        (
                            pixels[:, 0] * 0.114
                            + pixels[:, 1] * 0.587
                            + pixels[:, 2] * 0.299
                        ).mean()
                    )
                else:
                    nonblack = None
                    blue_dominant = None
                    mean_luma = None
                screen_content[eye]["nonblack_fraction"].append(nonblack)
                screen_content[eye]["blue_dominant_fraction"].append(
                    blue_dominant
                )
                screen_content[eye]["mean_luma"].append(mean_luma)
                background_pixels = roi[~overlay_tagged]
                if len(background_pixels):
                    background_nonblack = float(
                        (background_pixels.max(axis=1) > 12).mean()
                    )
                    background_luma = (
                        background_pixels[:, 0].astype(np.float32) * 0.114
                        + background_pixels[:, 1].astype(np.float32) * 0.587
                        + background_pixels[:, 2].astype(np.float32) * 0.299
                    )
                    background_luma_stddev = float(background_luma.std())
                else:
                    background_nonblack = 0.0
                    background_luma_stddev = 0.0
                world_background[eye]["nonblack_fraction"].append(
                    background_nonblack
                )
                world_background[eye]["luma_stddev"].append(
                    background_luma_stddev
                )
        if index % args.morphology_step == 0:
            morphology_samples += 1
        frame_count += 1

    if args.pair_directory:
        fps = args.pair_fps
        for index, pair in enumerate(paired_eye_paths(source)):
            colors = []
            alphas = []
            shape = None
            for path in pair:
                frame = cv2.imread(str(path), cv2.IMREAD_UNCHANGED)
                if frame is None:
                    raise ValueError(f"could not decode image: {path}")
                color, alpha = color_and_alpha(frame)
                if shape is not None and color.shape != shape:
                    raise ValueError("left/right eye dimensions do not match")
                shape = color.shape
                colors.append(crop_single_eye(color, args))
                alphas.append(
                    None if alpha is None else crop_single_eye(alpha, args)
                )
            consume(colors, alphas, index, index / fps)
    elif args.image:
        frame = cv2.imread(str(source), cv2.IMREAD_UNCHANGED)
        if frame is None:
            raise ValueError(f"could not decode image: {source}")
        color, alpha = color_and_alpha(frame)
        consume(
            eye_rois(color, args),
            [None, None] if alpha is None else eye_rois(alpha, args),
            0,
            0.0,
        )
    else:
        capture = cv2.VideoCapture(str(source))
        if not capture.isOpened():
            raise ValueError(f"could not open video: {source}")
        fps = float(capture.get(cv2.CAP_PROP_FPS))
        if not math.isfinite(fps) or fps <= 0.0:
            fps = 30.0
        index = 0
        while True:
            ok, frame = capture.read()
            if not ok:
                break
            consume(eye_rois(frame, args), [None, None], index, index / fps)
            index += 1
        capture.release()
    if frame_count == 0:
        raise ValueError("source yielded no frames")

    limits = {
        "skin_component_ratio": args.max_skin_component_ratio,
        "brown_component_ratio": args.max_brown_component_ratio,
        "bright_component_ratio": args.max_bright_component_ratio,
        "red_ratio": args.max_red_ratio,
        "red_jump": args.max_red_jump,
    }
    failures: list[dict[str, object]] = []
    for eye, metrics in enumerate(worst):
        for key, limit in limits.items():
            if args.spatial_overlay and key not in ("red_ratio", "red_jump"):
                continue
            if metrics[key] > limit:
                failures.append(
                    {
                        "eye": "left" if eye == 0 else "right",
                        "metric": key,
                        "observed": metrics[key],
                        "limit": limit,
                        "at": worst_at[eye][key],
                    }
                )

    spatial_report = None
    if args.spatial_overlay:
        minimum_ratios = {
            "left_hand": args.min_left_hand_ratio,
            "right_hand": args.min_right_hand_ratio,
            "pipboy_housing": args.min_pipboy_housing_ratio,
        }
        spatial_report = {"alpha_tags": SPATIAL_ALPHA_TAGS, "eyes": {}}
        for eye, eye_name in enumerate(("left", "right")):
            tag_report = {}
            for name, values in spatial_values[eye].items():
                if not values:
                    continue
                median = float(statistics.median(values))
                minimum = min(values)
                maximum = max(values)
                relative_floor = minimum / median if median > 0.0 else 0.0
                normalized_jumps = [
                    abs(current - previous) / median
                    for previous, current in zip(values, values[1:])
                ] if median > 0.0 else []
                maximum_jump = max(normalized_jumps, default=0.0)
                absolute_limit = minimum_ratios.get(name, 0.0)
                missing_fraction = (
                    sum(value < absolute_limit for value in values) / len(values)
                    if absolute_limit > 0.0 else 0.0
                )
                tag_report[name] = {
                    "minimum_ratio": minimum,
                    "maximum_ratio": maximum,
                    "median_ratio": median,
                    "minimum_to_median": relative_floor,
                    "maximum_normalized_jump": maximum_jump,
                    "missing_fraction": missing_fraction,
                    "smallest": spatial_smallest[eye][name],
                }
                if name not in minimum_ratios:
                    continue
                if minimum < absolute_limit:
                    failures.append({
                        "eye": eye_name,
                        "metric": f"{name}_minimum_ratio",
                        "observed": minimum,
                        "limit": absolute_limit,
                        "at": spatial_smallest[eye][name],
                    })
                if relative_floor < args.min_spatial_relative_floor:
                    failures.append({
                        "eye": eye_name,
                        "metric": f"{name}_minimum_to_median",
                        "observed": relative_floor,
                        "limit": args.min_spatial_relative_floor,
                        "at": spatial_smallest[eye][name],
                    })
                if maximum_jump > args.max_spatial_normalized_jump:
                    failures.append({
                        "eye": eye_name,
                        "metric": f"{name}_normalized_jump",
                        "observed": maximum_jump,
                        "limit": args.max_spatial_normalized_jump,
                        "at": None,
                    })
                if missing_fraction > args.max_spatial_missing_fraction:
                    failures.append({
                        "eye": eye_name,
                        "metric": f"{name}_missing_fraction",
                        "observed": missing_fraction,
                        "limit": args.max_spatial_missing_fraction,
                        "at": spatial_smallest[eye][name],
                    })
            wrist_socket_overlaps = [
                bbox_overlap_fraction(hand, housing)
                for hand, housing in zip(
                    spatial_bboxes[eye]["left_hand"],
                    spatial_bboxes[eye]["pipboy_housing"],
                )
            ]
            wrist_socket_gaps = [
                bbox_gap_pixels(hand, housing)
                for hand, housing in zip(
                    spatial_bboxes[eye]["left_hand"],
                    spatial_bboxes[eye]["pipboy_housing"],
                )
            ]
            minimum_wrist_socket_overlap = min(
                wrist_socket_overlaps, default=0.0
            )
            maximum_wrist_socket_gap = max(
                wrist_socket_gaps, default=float("inf")
            )
            wrist_socket_overlap_frame = (
                wrist_socket_overlaps.index(minimum_wrist_socket_overlap)
                if wrist_socket_overlaps else None
            )
            wrist_socket_gap_frame = (
                wrist_socket_gaps.index(maximum_wrist_socket_gap)
                if wrist_socket_gaps else None
            )
            tag_report["pipboy_wrist_socket"] = {
                "minimum_bbox_overlap_fraction": minimum_wrist_socket_overlap,
                "maximum_bbox_gap_pixels": maximum_wrist_socket_gap,
                "overlap_worst_frame": wrist_socket_overlap_frame,
                "gap_worst_frame": wrist_socket_gap_frame,
            }
            if (
                maximum_wrist_socket_gap
                > args.max_left_pipboy_bbox_gap_pixels
            ):
                failures.append({
                    "eye": eye_name,
                    "metric": "pipboy_wrist_socket_bbox_gap_pixels",
                    "observed": maximum_wrist_socket_gap,
                    "limit": args.max_left_pipboy_bbox_gap_pixels,
                    "at": {
                        "frame": wrist_socket_gap_frame,
                        "time_seconds": (
                            None if wrist_socket_gap_frame is None
                            else wrist_socket_gap_frame / fps
                        ),
                    },
                })
            if args.require_world_background:
                background_nonblack = world_background[eye][
                    "nonblack_fraction"
                ]
                background_stddev = world_background[eye]["luma_stddev"]
                minimum_background_nonblack = min(
                    background_nonblack, default=0.0
                )
                minimum_background_stddev = min(
                    background_stddev, default=0.0
                )
                nonblack_frame = (
                    background_nonblack.index(minimum_background_nonblack)
                    if background_nonblack else None
                )
                stddev_frame = (
                    background_stddev.index(minimum_background_stddev)
                    if background_stddev else None
                )
                tag_report["world_background"] = {
                    "minimum_nonblack_fraction": minimum_background_nonblack,
                    "minimum_luma_stddev": minimum_background_stddev,
                    "nonblack_worst_frame": nonblack_frame,
                    "luma_stddev_worst_frame": stddev_frame,
                }
                if (
                    minimum_background_nonblack
                    < args.min_world_background_nonblack_fraction
                ):
                    failures.append({
                        "eye": eye_name,
                        "metric": "world_background_nonblack_fraction",
                        "observed": minimum_background_nonblack,
                        "limit": args.min_world_background_nonblack_fraction,
                        "at": {
                            "frame": nonblack_frame,
                            "time_seconds": (
                                None if nonblack_frame is None
                                else nonblack_frame / fps
                            ),
                        },
                    })
                if (
                    minimum_background_stddev
                    < args.min_world_background_luma_stddev
                ):
                    failures.append({
                        "eye": eye_name,
                        "metric": "world_background_luma_stddev",
                        "observed": minimum_background_stddev,
                        "limit": args.min_world_background_luma_stddev,
                        "at": {
                            "frame": stddev_frame,
                            "time_seconds": (
                                None if stddev_frame is None
                                else stddev_frame / fps
                            ),
                        },
                    })
            if args.require_pipboy_screen:
                screen_values = spatial_values[eye]["pipboy_screen"]
                visible_indices = [
                    index
                    for index, value in enumerate(screen_values)
                    if value >= args.min_pipboy_screen_ratio
                ]
                internal_missing_fraction = 1.0
                if visible_indices:
                    first_visible = visible_indices[0]
                    last_visible = visible_indices[-1]
                    span = screen_values[first_visible : last_visible + 1]
                    internal_missing_fraction = (
                        sum(
                            value < args.min_pipboy_screen_ratio
                            for value in span
                        )
                        / len(span)
                    )
                tag_report["pipboy_screen"]["visible_frame_count"] = len(
                    visible_indices
                )
                tag_report["pipboy_screen"]["first_visible_frame"] = (
                    visible_indices[0] if visible_indices else None
                )
                tag_report["pipboy_screen"]["last_visible_frame"] = (
                    visible_indices[-1] if visible_indices else None
                )
                tag_report["pipboy_screen"]["internal_missing_fraction"] = (
                    internal_missing_fraction
                )
                visible_nonblack = [
                    screen_content[eye]["nonblack_fraction"][index]
                    for index in visible_indices
                ]
                visible_blue = [
                    screen_content[eye]["blue_dominant_fraction"][index]
                    for index in visible_indices
                ]
                visible_luma = [
                    screen_content[eye]["mean_luma"][index]
                    for index in visible_indices
                ]
                minimum_nonblack = min(visible_nonblack, default=0.0)
                maximum_blue = max(visible_blue, default=1.0)
                maximum_luma = max(visible_luma, default=255.0)
                tag_report["pipboy_screen"]["minimum_nonblack_fraction"] = (
                    minimum_nonblack
                )
                tag_report["pipboy_screen"]["maximum_blue_dominant_fraction"] = (
                    maximum_blue
                )
                tag_report["pipboy_screen"]["maximum_mean_luma"] = maximum_luma
                if len(visible_indices) < args.min_pipboy_screen_frames:
                    failures.append({
                        "eye": eye_name,
                        "metric": "pipboy_screen_visible_frames",
                        "observed": len(visible_indices),
                        "limit": args.min_pipboy_screen_frames,
                        "at": None,
                    })
                if (
                    visible_indices
                    and internal_missing_fraction
                        > args.max_pipboy_screen_internal_missing_fraction
                ):
                    failures.append({
                        "eye": eye_name,
                        "metric": "pipboy_screen_internal_missing_fraction",
                        "observed": internal_missing_fraction,
                        "limit": args.max_pipboy_screen_internal_missing_fraction,
                        "at": None,
                    })
                if (
                    visible_indices
                    and minimum_nonblack
                        < args.min_pipboy_screen_nonblack_fraction
                ):
                    failures.append({
                        "eye": eye_name,
                        "metric": "pipboy_screen_nonblack_fraction",
                        "observed": minimum_nonblack,
                        "limit": args.min_pipboy_screen_nonblack_fraction,
                        "at": None,
                    })
                if (
                    visible_indices
                    and maximum_blue
                        > args.max_pipboy_screen_blue_dominant_fraction
                ):
                    failures.append({
                        "eye": eye_name,
                        "metric": "pipboy_screen_blue_dominant_fraction",
                        "observed": maximum_blue,
                        "limit": args.max_pipboy_screen_blue_dominant_fraction,
                        "at": None,
                    })
                if (
                    visible_indices
                    and maximum_luma > args.max_pipboy_screen_mean_luma
                ):
                    failures.append({
                        "eye": eye_name,
                        "metric": "pipboy_screen_mean_luma",
                        "observed": maximum_luma,
                        "limit": args.max_pipboy_screen_mean_luma,
                        "at": None,
                    })
            spatial_report["eyes"][eye_name] = tag_report
    return {
        "schema": "fnvxr-first-person-visual-quality/v2",
        "source": str(source),
        "fixture_scope": "FNVXR_AutoRetail_L1_Pistol submitted SBS eyes",
        "frame_count": frame_count,
        "morphology_samples": morphology_samples,
        "fps": fps,
        "thresholds": limits,
        "eyes": {
            "left": {"worst": worst[0], "worst_at": worst_at[0]},
            "right": {"worst": worst[1], "worst_at": worst_at[1]},
        },
        "spatial_overlay": spatial_report,
        "failures": failures,
        "accepted": not failures,
        "claim_boundary": (
            "In spatial-overlay mode, exact presentation-inert final-eye alpha "
            "tags prove both hands and the Pip-Boy housing remained in every "
            "captured eye, while untagged final-eye pixels prove a populated "
            "game-world background and strict red-flash continuity stays clean. "
            "Outside that mode, this rejects the historical stretched-arm and "
            "red-flash morphology for the owned pistol fixture."
        ),
    }


def make_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser()
    source = parser.add_mutually_exclusive_group(required=True)
    source.add_argument("--video")
    source.add_argument("--image")
    source.add_argument("--pair-directory")
    parser.add_argument("--report")
    parser.add_argument("--spatial-overlay", action="store_true")
    parser.add_argument("--require-pipboy-screen", action="store_true")
    parser.add_argument("--require-world-background", action="store_true")
    parser.add_argument("--pair-fps", type=float, default=30.0)
    parser.add_argument("--alpha-tag-tolerance", type=int, default=2)
    parser.add_argument("--content-top", type=int, default=0)
    parser.add_argument("--content-height", type=int, default=0)
    parser.add_argument("--roi-top-ratio", type=float, default=0.52)
    parser.add_argument("--morphology-step", type=int, default=5)
    parser.add_argument("--max-skin-component-ratio", type=float, default=0.02)
    parser.add_argument("--max-brown-component-ratio", type=float, default=0.02)
    parser.add_argument("--max-bright-component-ratio", type=float, default=0.008)
    parser.add_argument("--max-red-ratio", type=float, default=0.005)
    parser.add_argument("--max-red-jump", type=float, default=0.003)
    parser.add_argument("--min-left-hand-ratio", type=float, default=0.010)
    parser.add_argument("--min-right-hand-ratio", type=float, default=0.008)
    parser.add_argument("--min-pipboy-housing-ratio", type=float, default=0.001)
    parser.add_argument(
        "--max-left-pipboy-bbox-gap-pixels",
        type=float,
        default=2.0,
    )
    parser.add_argument("--min-spatial-relative-floor", type=float, default=0.10)
    parser.add_argument("--max-spatial-normalized-jump", type=float, default=5.0)
    parser.add_argument("--max-spatial-missing-fraction", type=float, default=0.0)
    parser.add_argument("--min-pipboy-screen-ratio", type=float, default=0.0002)
    parser.add_argument("--min-pipboy-screen-frames", type=int, default=3)
    parser.add_argument(
        "--max-pipboy-screen-internal-missing-fraction",
        type=float,
        default=0.0,
    )
    parser.add_argument(
        "--min-pipboy-screen-nonblack-fraction",
        type=float,
        default=0.30,
    )
    parser.add_argument(
        "--max-pipboy-screen-blue-dominant-fraction",
        type=float,
        default=0.20,
    )
    parser.add_argument(
        "--max-pipboy-screen-mean-luma",
        type=float,
        default=60.0,
    )
    parser.add_argument(
        "--min-world-background-nonblack-fraction",
        type=float,
        default=0.25,
    )
    parser.add_argument(
        "--min-world-background-luma-stddev",
        type=float,
        default=8.0,
    )
    return parser


def main() -> int:
    args = make_parser().parse_args()
    if not 0.0 <= args.roi_top_ratio < 1.0 or args.morphology_step < 1:
        raise SystemExit("invalid ROI or morphology sampling arguments")
    if args.spatial_overlay and not args.pair_directory:
        raise SystemExit("--spatial-overlay requires --pair-directory")
    if args.require_pipboy_screen and not args.spatial_overlay:
        raise SystemExit("--require-pipboy-screen requires --spatial-overlay")
    if args.require_world_background and not args.spatial_overlay:
        raise SystemExit("--require-world-background requires --spatial-overlay")
    if args.pair_fps <= 0.0 or args.alpha_tag_tolerance < 0:
        raise SystemExit("invalid pair FPS or alpha-tag tolerance")
    report = analyze(args)
    encoded = json.dumps(report, indent=2)
    if args.report:
        path = Path(args.report).resolve()
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(encoded + "\n", encoding="utf-8")
    print(encoded)
    return 0 if report["accepted"] else 2


if __name__ == "__main__":
    sys.exit(main())
