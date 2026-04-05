# USB / CSI camera

- **Python path:** OpenCV `VideoCapture` (see `scripts/run_pipeline.py` `--mode full_cpp`).
- **C++ path:** Prefer `libcamera` on modern Raspberry Pi OS, or V4L2 for USB UVC devices.

Timestamp frames with `CLOCK_MONOTONIC` (or `steady_clock` in C++) at capture completion to align with PPG IRQ time base.
