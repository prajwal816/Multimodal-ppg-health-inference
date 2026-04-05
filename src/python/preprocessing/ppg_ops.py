"""PPG-oriented preprocessing (Python mirror of C++ DSP path for tests and Pi-side fallback)."""

from __future__ import annotations

import math
from typing import Generator, Iterable, List, Sequence, Tuple

import numpy as np


def _biquad_step(
    x: np.ndarray, b: Tuple[float, float, float], a: Tuple[float, float, float]
) -> np.ndarray:
    b0, b1, b2 = b
    _, a1, a2 = a
    y = np.zeros_like(x, dtype=np.float64)
    x1 = x2 = y1 = y2 = 0.0
    for i, xi in enumerate(x.astype(np.float64)):
        yi = b0 * xi + b1 * x1 + b2 * x2 - a1 * y1 - a2 * y2
        x2, x1 = x1, xi
        y2, y1 = y1, yi
        y[i] = yi
    return y.astype(np.float32)


def bandpass_sos(x: np.ndarray, fs: float, low_hz: float, high_hz: float) -> np.ndarray:
    """Two-stage RBJ high-pass + low-pass (same topology as C++ BandpassChain)."""
    # Coefficients computed offline-style; fs-dependent
    def rbj_hp(f0: float, q: float = 0.707):
        w0 = 2.0 * math.pi * f0 / fs
        c = math.cos(w0)
        s = math.sin(w0)
        alpha = s / (2.0 * q)
        b0 = (1.0 + c) / 2.0
        b1 = -(1.0 + c)
        b2 = (1.0 + c) / 2.0
        a0 = 1.0 + alpha
        return (b0 / a0, b1 / a0, b2 / a0), (1.0, -2.0 * c / a0, (1.0 - alpha) / a0)

    def rbj_lp(f0: float, q: float = 0.707):
        w0 = 2.0 * math.pi * f0 / fs
        c = math.cos(w0)
        s = math.sin(w0)
        alpha = s / (2.0 * q)
        b0 = (1.0 - c) / 2.0
        b1 = 1.0 - c
        b2 = (1.0 - c) / 2.0
        a0 = 1.0 + alpha
        return (b0 / a0, b1 / a0, b2 / a0), (1.0, -2.0 * c / a0, (1.0 - alpha) / a0)

    bh, ah = rbj_hp(low_hz)
    bl, al = rbj_lp(high_hz)
    y = _biquad_step(x, bh, ah)
    y = _biquad_step(y, bl, al)
    return y


def normalize_window(x: np.ndarray, eps: float = 1e-6) -> np.ndarray:
    x = x.astype(np.float32)
    m = float(x.mean())
    s = float(x.std()) + eps
    return ((x - m) / s).astype(np.float32)


def sliding_windows(x: Sequence[float], win: int, hop: int) -> Generator[np.ndarray, None, None]:
    arr = np.asarray(x, dtype=np.float32)
    for i in range(0, max(0, len(arr) - win + 1), hop):
        yield arr[i : i + win].copy()


def stack_ir_red(ir: np.ndarray, red: np.ndarray) -> Tuple[np.ndarray, np.ndarray]:
    return bandpass_sos(ir, 200.0, 0.5, 4.0), bandpass_sos(red, 200.0, 0.5, 4.0)
