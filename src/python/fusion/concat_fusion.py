"""Late fusion: concatenate normalized PPG window with compact visual descriptors."""

from __future__ import annotations

from dataclasses import dataclass
from typing import Optional

import numpy as np


@dataclass
class FusedFeatures:
    vector: np.ndarray
    ppg_part: np.ndarray
    visual_part: np.ndarray


class FusionEngine:
    def __init__(self, ppg_weight: float, visual_weight: float, visual_dim: int):
        self._wp = float(ppg_weight)
        self._wv = float(visual_weight)
        self._vdim = int(visual_dim)

    def visual_from_frame(self, frame_bgr: Optional[np.ndarray]) -> np.ndarray:
        if frame_bgr is None or frame_bgr.size == 0:
            return np.zeros((self._vdim,), dtype=np.float32)
        # Lightweight embedding: downsampled histogram + spatial pooling (no trainable CNN in box)
        small = frame_bgr[::8, ::8, :].astype(np.float32) / 255.0
        flat = small.reshape(-1, 3)
        hist = np.histogramdd(flat, bins=(4, 4, 4), range=((0, 1),) * 3)[0].ravel()
        hist = hist.astype(np.float32)
        if hist.size >= self._vdim:
            v = hist[: self._vdim]
        else:
            v = np.zeros((self._vdim,), dtype=np.float32)
            v[: hist.size] = hist
        n = float(np.linalg.norm(v) + 1e-6)
        return (v / n).astype(np.float32)

    def fuse(self, ppg_window: np.ndarray, frame_bgr: Optional[np.ndarray]) -> FusedFeatures:
        p = ppg_window.astype(np.float32).ravel()
        v = self.visual_from_frame(frame_bgr)
        p_norm = p / (np.linalg.norm(p) + 1e-6)
        vec = np.concatenate([self._wp * p_norm, self._wv * v]).astype(np.float32)
        return FusedFeatures(vector=vec, ppg_part=p_norm, visual_part=v)
