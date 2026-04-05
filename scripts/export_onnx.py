#!/usr/bin/env python3
"""Export (or refresh) the stub fusion MLP used for development and CI."""

from __future__ import annotations

import argparse
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "src" / "python"))

from inference.onnx_engine import ensure_model  # noqa: E402


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", type=Path, default=ROOT / "models" / "fusion_mlp.onnx")
    ap.add_argument("--ppg-len", type=int, default=64)
    ap.add_argument("--visual-dim", type=int, default=16)
    args = ap.parse_args()
    path = ensure_model(args.out, ppg_len=args.ppg_len, visual_dim=args.visual_dim)
    print(f"model_ready path={path}")


if __name__ == "__main__":
    main()
