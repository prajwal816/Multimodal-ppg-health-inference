# Embedded deployment notes (Raspberry Pi 4)

1. **Isolation:** Run acquisition under `SCHED_FIFO` only after careful cap setup (`CAP_SYS_NICE`); prefer isolcpus for the sampling IRQ thread on dedicated cores.
2. **Memory:** Preallocate ring buffers and ONNX I/O tensors; avoid `std::vector` growth in the 200 Hz callback (use fixed windows).
3. **ONNX Runtime:** Build with `armv7`/`aarch64` ORT 1.16+; enable XNNPACK; consider INT8 quantization of the fusion MLP for sub-10 ms targets.
4. **Sync:** Use a single monotonic clock domain; propagate `frame_id` and `sample_seq` for offline audit when skew exceeds `max_sync_skew_ms` in config.
