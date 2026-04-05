import os
from pathlib import Path

import numpy as np
import pytest

from fusion.concat_fusion import FusionEngine
from inference.onnx_engine import ensure_model


def test_fusion_vector_shape_and_normalization():
    """End-to-end fusion dimensions without ONNX Runtime (CI-safe)."""
    fusion = FusionEngine(0.65, 0.35, 16)
    win = 64
    ppg = np.random.randn(win).astype(np.float32)
    frame = (np.random.rand(32, 32, 3) * 255).astype(np.uint8)
    fused = fusion.fuse(ppg, frame)
    assert fused.vector.shape == (win + 16,)
    assert np.isfinite(fused.vector).all()
    n = float(np.linalg.norm(fused.vector))
    assert n > 0.01


@pytest.mark.skipif(
    os.environ.get("PPG_RUN_ONNX_TESTS", "").strip() != "1",
    reason="Set PPG_RUN_ONNX_TESTS=1 to run ONNX Runtime import (avoids broken native DLL noise on some Windows setups).",
)
def test_onnx_fusion_roundtrip(tmp_path: Path):
    from inference.onnx_engine import OnnxFusionEngine

    model = tmp_path / "m.onnx"
    win = 64
    vd = 16
    ensure_model(model, ppg_len=win, visual_dim=vd)
    try:
        eng = OnnxFusionEngine(model)
    except ImportError as e:
        pytest.fail(f"ONNX Runtime not loadable: {e}")

    fusion = FusionEngine(0.65, 0.35, vd)
    ppg = np.random.randn(win).astype(np.float32)
    frame = (np.random.rand(32, 32, 3) * 255).astype(np.uint8)
    fused = fusion.fuse(ppg, frame)
    assert fused.vector.shape == (win + vd,)
    out, dt = eng.predict(fused.vector)
    assert out.shape[1] == 2
    assert dt >= 0
