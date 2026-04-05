"""ONNX Runtime inference with bounded session options for edge deployment."""

from __future__ import annotations

import logging
import os
import time
from pathlib import Path
from typing import Any, Dict, List, Optional, Tuple

import numpy as np

logger = logging.getLogger(__name__)


class OnnxFusionEngine:
    def __init__(self, model_path: Path, intra_op_threads: int = 1, inter_op_threads: int = 1):
        import onnxruntime as ort

        self._model_path = Path(model_path)
        so = ort.SessionOptions()
        so.intra_op_num_threads = intra_op_threads
        so.inter_op_num_threads = inter_op_threads
        so.graph_optimization_level = ort.GraphOptimizationLevel.ORT_ENABLE_ALL
        providers = ["CPUExecutionProvider"]
        if ort.get_device() == "GPU":
            providers = ["CUDAExecutionProvider", "CPUExecutionProvider"]
        self._session = ort.InferenceSession(
            str(self._model_path), sess_options=so, providers=providers
        )
        self._in_name = self._session.get_inputs()[0].name
        self._out_names = [o.name for o in self._session.get_outputs()]
        logger.info(
            "loaded_onnx model=%s inputs=%s outputs=%s",
            self._model_path.name,
            self._in_name,
            self._out_names,
        )

    @property
    def input_name(self) -> str:
        return self._in_name

    def predict(self, x: np.ndarray) -> Tuple[np.ndarray, float]:
        x = np.asarray(x, dtype=np.float32)
        if x.ndim == 1:
            x = x[np.newaxis, :]
        t0 = time.perf_counter()
        outs = self._session.run(self._out_names, {self._in_name: x})
        dt_ms = (time.perf_counter() - t0) * 1000.0
        return outs[0], dt_ms

    def predict_proba(self, x: np.ndarray) -> Tuple[float, float]:
        raw, dt = self.predict(x)
        if raw.ndim == 2 and raw.shape[1] >= 2:
            ex = np.exp(raw.astype(np.float64) - np.max(raw, axis=1, keepdims=True))
            p = (ex / np.sum(ex, axis=1, keepdims=True))[0, 1]
            return float(p), dt
        if raw.ndim == 2 and raw.shape[1] == 1:
            return float(1.0 / (1.0 + np.exp(-raw[0, 0]))), dt
        return float(raw.ravel()[0]), dt


def ensure_model(path: Path, ppg_len: int = 64, visual_dim: int = 16) -> Path:
    """Create a tiny ONNX MLP if missing (deterministic, for CI and Docker)."""
    if path.exists():
        return path
    path.parent.mkdir(parents=True, exist_ok=True)
    _export_stub_mlp(path, ppg_len, visual_dim)
    return path


def _export_stub_mlp(path: Path, ppg_len: int, visual_dim: int) -> None:
    from onnx import TensorProto, helper

    fused = ppg_len + visual_dim
    hidden = 32
    w1 = np.random.default_rng(42).standard_normal((fused, hidden)).astype(np.float32) * 0.05
    b1 = np.zeros((hidden,), dtype=np.float32)
    w2 = np.random.default_rng(43).standard_normal((hidden, 2)).astype(np.float32) * 0.08
    b2 = np.zeros((2,), dtype=np.float32)

    nodes = [
        helper.make_node("MatMul", ["X", "W1"], ["H0"]),
        helper.make_node("Add", ["H0", "B1"], ["H1"]),
        helper.make_node("Relu", ["H1"], ["H2"]),
        helper.make_node("MatMul", ["H2", "W2"], ["Z0"]),
        helper.make_node("Add", ["Z0", "B2"], ["Y"]),
    ]
    graph = helper.make_graph(
        nodes,
        "fusion_mlp",
        inputs=[helper.make_tensor_value_info("X", TensorProto.FLOAT, [None, fused])],
        outputs=[helper.make_tensor_value_info("Y", TensorProto.FLOAT, [None, 2])],
        initializer=[
            helper.make_tensor("W1", TensorProto.FLOAT, list(w1.shape), w1.tobytes(), raw=True),
            helper.make_tensor("B1", TensorProto.FLOAT, list(b1.shape), b1.tobytes(), raw=True),
            helper.make_tensor("W2", TensorProto.FLOAT, list(w2.shape), w2.tobytes(), raw=True),
            helper.make_tensor("B2", TensorProto.FLOAT, list(b2.shape), b2.tobytes(), raw=True),
        ],
    )
    model = helper.make_model(graph, opset_imports=[helper.make_opsetid("", 17)])
    from onnx import save_model

    save_model(model, str(path))
    logger.warning("wrote_stub_onnx path=%s (train and replace for production)", path)
