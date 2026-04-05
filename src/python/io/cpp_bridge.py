"""Consume `ppg_realtime_demo --jsonl` stdout (non-blocking reader thread)."""

from __future__ import annotations

import json
import logging
import queue
import subprocess
import threading
from dataclasses import dataclass
from pathlib import Path
from typing import List, Optional

import numpy as np

logger = logging.getLogger(__name__)


@dataclass
class CppWindow:
    seq: int
    ppg: np.ndarray
    sync_delay_ns: int
    frame_id: int
    luma: float


class CppJsonlPpgReader:
    def __init__(self, demo_exe: Path, extra_args: Optional[List[str]] = None):
        self._exe = Path(demo_exe)
        self._extra = extra_args or []
        self._proc: Optional[subprocess.Popen] = None
        self._q: queue.Queue[Optional[CppWindow]] = queue.Queue(maxsize=32)
        self._thread: Optional[threading.Thread] = None

    def start(self, seconds: int = 30) -> None:
        cmd = [str(self._exe), "--jsonl", "--seconds", str(seconds), *self._extra]
        logger.info("starting_cpp_bridge cmd=%s", " ".join(cmd))
        self._proc = subprocess.Popen(
            cmd,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
            bufsize=1,
        )
        self._thread = threading.Thread(target=self._read_loop, daemon=True)
        self._thread.start()

    def _read_loop(self) -> None:
        assert self._proc and self._proc.stdout
        for line in self._proc.stdout:
            line = line.strip()
            if not line:
                continue
            try:
                obj = json.loads(line)
                w = CppWindow(
                    seq=int(obj["seq"]),
                    ppg=np.asarray(obj["ppg"], dtype=np.float32),
                    sync_delay_ns=int(obj["sync_delay_ns"]),
                    frame_id=int(obj["frame_id"]),
                    luma=float(obj["luma"]),
                )
                try:
                    self._q.put(w, timeout=0.5)
                except queue.Full:
                    logger.warning("cpp_bridge queue full, dropping window seq=%s", w.seq)
            except (json.JSONDecodeError, KeyError, ValueError) as e:
                logger.debug("skip line parse error: %s", e)

        self._q.put(None)

    def try_get(self, timeout: float = 0.05) -> Optional[CppWindow]:
        try:
            return self._q.get(timeout=timeout)
        except queue.Empty:
            return None

    def wait_done(self) -> None:
        if self._thread:
            self._thread.join(timeout=60)
        if self._proc:
            self._proc.wait(timeout=10)
