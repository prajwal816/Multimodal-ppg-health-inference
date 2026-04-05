#!/usr/bin/env python3
"""
Multi-threaded edge pipeline: acquisition | sync | preprocess | fusion | ONNX.

Modes:
  sim          Pure Python simulated sensors (default)
  full_cpp     C++ `ppg_realtime_demo --jsonl` + OpenCV camera + ONNX
"""

from __future__ import annotations

import argparse
import logging
import queue
import sys
import threading
import time
from pathlib import Path
from typing import Optional, Tuple

import numpy as np
import yaml

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "src" / "python"))

from fusion.concat_fusion import FusionEngine  # noqa: E402
from inference.onnx_engine import OnnxFusionEngine, ensure_model  # noqa: E402
from io.cpp_bridge import CppJsonlPpgReader  # noqa: E402
from preprocessing.ppg_ops import bandpass_sos, normalize_window  # noqa: E402

logging.basicConfig(
    level=logging.INFO,
    format="%(asctime)s.%(msecs)03d %(levelname)s [%(threadName)s] %(message)s",
    datefmt="%H:%M:%S",
)
logger = logging.getLogger("pipeline")


def load_config(path: Path) -> dict:
    with path.open("r", encoding="utf-8") as f:
        return yaml.safe_load(f)


def sim_ppg_sample(tick: int, fs: float) -> Tuple[float, float]:
    phase = (tick / fs) * (2 * np.pi) * (72 / 60.0)
    ir = 0.85 + 0.5 * np.sin(phase) + 0.12 * np.sin(2 * phase + 0.3)
    red = 0.80 + 0.45 * np.sin(phase) + 0.1 * np.sin(2 * phase)
    return float(ir), float(red)


def run_sim(cfg: dict, max_windows: int) -> None:
    pipe = cfg["pipeline"]
    fs = float(pipe["sample_rate_hz"])
    win = int(pipe["ppg_window"])
    fusion_cfg = cfg["fusion"]
    model_path = ROOT / cfg["model"]["onnx_path"]
    ensure_model(model_path, ppg_len=win, visual_dim=int(fusion_cfg["visual_feature_dim"]))
    engine = OnnxFusionEngine(model_path)
    fusion = FusionEngine(
        fusion_cfg["ppg_weight"],
        fusion_cfg["visual_weight"],
        int(fusion_cfg["visual_feature_dim"]),
    )

    raw_q: queue.Queue[Tuple[float, float]] = queue.Queue(maxsize=512)
    win_q: queue.Queue[np.ndarray] = queue.Queue(maxsize=32)
    shutdown = threading.Event()

    def acquire() -> None:
        tick = 0
        period = 1.0 / fs
        while not shutdown.is_set():
            ir, red = sim_ppg_sample(tick, fs)
            try:
                raw_q.put((ir, red), timeout=0.2)
            except queue.Full:
                pass
            tick += 1
            time.sleep(period)

    def process() -> None:
        buf_ir: list[float] = []
        buf_red: list[float] = []
        emitted = 0
        while not shutdown.is_set() and emitted < max_windows:
            try:
                ir, red = raw_q.get(timeout=0.2)
            except queue.Empty:
                continue
            buf_ir.append(ir)
            buf_red.append(red)
            if len(buf_ir) < win:
                continue
            ir_a = np.asarray(buf_ir[-win:], dtype=np.float32)
            red_a = np.asarray(buf_red[-win:], dtype=np.float32)
            ir_f = bandpass_sos(ir_a, fs, cfg["dsp"]["bandpass_low_hz"], cfg["dsp"]["bandpass_high_hz"])
            red_f = bandpass_sos(red_a, fs, cfg["dsp"]["bandpass_low_hz"], cfg["dsp"]["bandpass_high_hz"])
            fused = 0.6 * normalize_window(ir_f) + 0.4 * normalize_window(red_f)
            try:
                win_q.put(fused.astype(np.float32), timeout=0.2)
            except queue.Full:
                pass
            emitted += 1
            buf_ir = buf_ir[-win // 2 :]
            buf_red = buf_red[-win // 2 :]

    def infer() -> None:
        count = 0
        latencies = []
        while count < max_windows and not shutdown.is_set():
            try:
                w = win_q.get(timeout=1.0)
            except queue.Empty:
                continue
            fake_frame = (np.random.rand(48, 48, 3) * 255).astype(np.uint8)
            fused = fusion.fuse(w, fake_frame)
            prob, dt = engine.predict_proba(fused.vector)
            latencies.append(dt)
            logger.info(
                "inference window=%s prob_positive=%.4f onnx_ms=%.3f (budget=%sms)",
                count + 1,
                prob,
                dt,
                cfg["pipeline"]["inference_target_latency_ms"],
            )
            count += 1
        shutdown.set()
        if latencies:
            logger.info(
                "latency_p50_ms=%.3f p95_ms=%.3f",
                float(np.percentile(latencies, 50)),
                float(np.percentile(latencies, 95)),
            )

    t1 = threading.Thread(target=acquire, name="acquire")
    t3 = threading.Thread(target=process, name="process")
    t4 = threading.Thread(target=infer, name="inference")
    t1.start()
    t3.start()
    t4.start()
    t1.join()
    t3.join(timeout=5)
    t4.join(timeout=30)


def run_full_cpp(cfg: dict, demo_exe: Path, seconds: int) -> None:
    pipe = cfg["pipeline"]
    win = int(pipe["ppg_window"])
    fusion_cfg = cfg["fusion"]
    model_path = ROOT / cfg["model"]["onnx_path"]
    ensure_model(model_path, ppg_len=win, visual_dim=int(fusion_cfg["visual_feature_dim"]))
    engine = OnnxFusionEngine(model_path)
    fusion = FusionEngine(
        fusion_cfg["ppg_weight"],
        fusion_cfg["visual_weight"],
        int(fusion_cfg["visual_feature_dim"]),
    )

    reader = CppJsonlPpgReader(demo_exe)
    reader.start(seconds=seconds)

    import cv2

    cap = cv2.VideoCapture(0)
    frame: Optional[np.ndarray] = None

    def cam_loop() -> None:
        nonlocal frame
        fps = float(pipe.get("camera_fps", 30))
        period = 1.0 / max(fps, 1.0)
        while reader._proc and reader._proc.poll() is None:
            ok, f = cap.read()
            if ok:
                frame = f
            time.sleep(period)
        cap.release()

    ct = threading.Thread(target=cam_loop, name="camera", daemon=True)
    ct.start()

    n = 0
    latencies = []
    while True:
        w = reader.try_get(timeout=0.2)
        if w is None:
            if reader._proc and reader._proc.poll() is not None:
                break
            continue
        ppg = normalize_window(w.ppg.astype(np.float32))
        fused = fusion.fuse(ppg, frame)
        prob, dt = engine.predict_proba(fused.vector)
        latencies.append(dt)
        n += 1
        logger.info(
            "cpp+jsonl window=%s prob=%.4f onnx_ms=%.3f sync_delay_ns=%s frame_id=%s",
            n,
            prob,
            dt,
            w.sync_delay_ns,
            w.frame_id,
        )

    reader.wait_done()
    if latencies:
        logger.info(
            "latency_p50_ms=%.3f p95_ms=%.3f",
            float(np.percentile(latencies, 50)),
            float(np.percentile(latencies, 95)),
        )


def main() -> None:
    ap = argparse.ArgumentParser(description="PPG multimodal ONNX pipeline")
    ap.add_argument("--config", type=Path, default=ROOT / "configs" / "default.yaml")
    ap.add_argument("--mode", choices=("sim", "full_cpp"), default="sim")
    ap.add_argument("--windows", type=int, default=25, help="Sim mode: number of inference windows")
    ap.add_argument("--seconds", type=int, default=8, help="full_cpp: C++ demo duration")
    ap.add_argument(
        "--demo-exe",
        type=Path,
        default=ROOT / "build" / "Release" / "ppg_realtime_demo.exe",
        help="Windows default; on Linux use build/ppg_realtime_demo",
    )
    args = ap.parse_args()
    cfg = load_config(args.config)
    log_level = cfg.get("logging", {}).get("level", "INFO")
    logging.getLogger().setLevel(getattr(logging, log_level, logging.INFO))

    if args.mode == "sim":
        run_sim(cfg, max_windows=args.windows)
    else:
        exe = args.demo_exe
        if not exe.exists():
            alt = ROOT / "build" / "ppg_realtime_demo"
            if alt.exists():
                exe = alt
        if not exe.exists():
            logger.error("C++ demo not found at %s — build with CMake first.", args.demo_exe)
            sys.exit(1)
        run_full_cpp(cfg, exe, seconds=args.seconds)


if __name__ == "__main__":
    main()
