"""Unit tests for timestamp-nearest multimodal alignment (matches C++ policy)."""

from __future__ import annotations

from typing import List, Optional, Tuple


def nearest_frame(cam_ts: List[int], center_ns: int) -> Optional[Tuple[int, int]]:
    if not cam_ts:
        return None
    best = cam_ts[0]
    best_abs = abs(center_ns - best)
    for c in cam_ts:
        d = abs(center_ns - c)
        if d < best_abs:
            best_abs = d
            best = c
    return best, center_ns - best


def test_nearest_frame_picks_minimum_delta():
    cams = [1_000_000, 1_050_000, 2_000_000]
    center = 1_048_000
    nf = nearest_frame(cams, center)
    assert nf is not None
    assert nf[0] == 1_050_000
    assert nf[1] == -2000  # center - cam
