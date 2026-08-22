#!/usr/bin/env python3
"""Fixture-specific pixel gate for FNVXR submitted side-by-side eyes."""

from __future__ import annotations

import argparse
import json
import math
import sys
from pathlib import Path

try:
    import cv2
    import numpy as np
except ImportError as exc:
    raise SystemExit("requires OpenCV (cv2) and numpy") from exc


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
    source = Path(args.video or args.image).resolve()
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

    def consume(frame: "np.ndarray", index: int, seconds: float) -> None:
        nonlocal frame_count, morphology_samples
        rois = eye_rois(frame, args)
        for eye, roi in enumerate(rois):
            selected = masks(roi)
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
        if index % args.morphology_step == 0:
            morphology_samples += 1
        frame_count += 1

    if args.image:
        frame = cv2.imread(str(source), cv2.IMREAD_COLOR)
        if frame is None:
            raise ValueError(f"could not decode image: {source}")
        consume(frame, 0, 0.0)
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
            consume(frame, index, index / fps)
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
    failures = []
    for eye, metrics in enumerate(worst):
        for key, limit in limits.items():
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
    return {
        "schema": "fnvxr-first-person-visual-quality/v1",
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
        "failures": failures,
        "accepted": not failures,
        "claim_boundary": (
            "Rejects the known stretched-arm/red-flash morphology for the owned "
            "pistol fixture; it does not identify hands or authorize the product."
        ),
    }


def make_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser()
    source = parser.add_mutually_exclusive_group(required=True)
    source.add_argument("--video")
    source.add_argument("--image")
    parser.add_argument("--report")
    parser.add_argument("--content-top", type=int, default=0)
    parser.add_argument("--content-height", type=int, default=0)
    parser.add_argument("--roi-top-ratio", type=float, default=0.52)
    parser.add_argument("--morphology-step", type=int, default=5)
    parser.add_argument("--max-skin-component-ratio", type=float, default=0.02)
    parser.add_argument("--max-brown-component-ratio", type=float, default=0.02)
    parser.add_argument("--max-bright-component-ratio", type=float, default=0.008)
    parser.add_argument("--max-red-ratio", type=float, default=0.005)
    parser.add_argument("--max-red-jump", type=float, default=0.003)
    return parser


def main() -> int:
    args = make_parser().parse_args()
    if not 0.0 <= args.roi_top_ratio < 1.0 or args.morphology_step < 1:
        raise SystemExit("invalid ROI or morphology sampling arguments")
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
