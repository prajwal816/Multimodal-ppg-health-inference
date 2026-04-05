from pathlib import Path

import numpy as np

from fusion.concat_fusion import FusionEngine
from inference.onnx_engine import OnnxFusionEngine, ensure_model


def test_onnx_fusion_roundtrip(tmp_path: Path):
    model = tmp_path / "m.onnx"
    win = 64
    vd = 16
    ensure_model(model, ppg_len=win, visual_dim=vd)
    eng = OnnxFusionEngine(model)
    fusion = FusionEngine(0.65, 0.35, vd)
    ppg = np.random.randn(win).astype(np.float32)
    frame = (np.random.rand(32, 32, 3) * 255).astype(np.uint8)
    fused = fusion.fuse(ppg, frame)
    assert fused.vector.shape == (win + vd,)
    out, dt = eng.predict(fused.vector)
    assert out.shape[1] == 2
    assert dt >= 0
