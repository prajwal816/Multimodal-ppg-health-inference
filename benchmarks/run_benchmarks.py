#!/usr/bin/env python3
"""Emit reproducible latency / throughput numbers for README tables."""

from __future__ import annotations

import json
import sys
import time
from pathlib import Path

import numpy as np
import yaml

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "src" / "python"))

from inference.onnx_engine import ensure_model  # noqa: E402


def main() -> None:
    cfg_path = ROOT / "configs" / "default.yaml"
    with cfg_path.open("r", encoding="utf-8") as f:
        cfg = yaml.safe_load(f)
    win = int(cfg["pipeline"]["ppg_window"])
    vd = int(cfg["fusion"]["visual_feature_dim"])
    model = ROOT / "models" / "fusion_mlp.onnx"
    ensure_model(model, ppg_len=win, visual_dim=vd)
    try:
        from inference.onnx_engine import OnnxFusionEngine
    except ImportError as e:
        print(
            json.dumps(
                {
                    "error": str(e),
                    "note": "Install onnxruntime; on Windows ensure VC++ 2019–2022 runtime is present.",
                },
                indent=2,
            )
        )
        raise SystemExit(1) from e
    try:
        eng = OnnxFusionEngine(model)
    except ImportError as e:
        print(
            json.dumps(
                {
                    "error": str(e),
                    "note": "ONNX Runtime native library failed to load.",
                },
                indent=2,
            )
        )
        raise SystemExit(1) from e

    warm = int(cfg.get("benchmark", {}).get("warmup_iterations", 5))
    n = int(cfg.get("benchmark", {}).get("timed_iterations", 200))
    x = np.random.randn(1, win + vd).astype(np.float32)
    for _ in range(warm):
        eng.predict(x)

    times = []
    t0 = time.perf_counter()
    for _ in range(n):
        _, dt = eng.predict(x)
        times.append(dt)
    total = time.perf_counter() - t0

    out = {
        "onnx_only_median_ms": float(np.median(times)),
        "onnx_only_p95_ms": float(np.percentile(times, 95)),
        "onnx_only_throughput_hz": n / total,
        "iterations": n,
        "note": "CPU ONNXRuntime; Pi 4 will be higher — tune ORT threads and quantize.",
    }
    print(json.dumps(out, indent=2))


if __name__ == "__main__":
    main()
