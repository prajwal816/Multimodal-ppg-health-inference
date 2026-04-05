#pragma once

#include "acquisition/sensor_simulator.hpp"

#include <chrono>
#include <cstdint>
#include <deque>
#include <mutex>
#include <optional>
#include <vector>

namespace ppg::sync {

struct CameraFrameMeta {
    std::chrono::steady_clock::time_point t;
    std::uint64_t frame_id;
    double mean_luma;
};

struct AlignedWindow {
    std::chrono::steady_clock::time_point window_end;
    std::vector<float> ppg_ir;
    std::vector<float> ppg_red;
    CameraFrameMeta nearest_frame;
    std::int64_t sync_delay_ns;
};

/// Buffers PPG samples and camera metadata; emits windows when enough PPG samples collected.
class MultimodalSynchronizer {
public:
    explicit MultimodalSynchronizer(std::size_t ppg_window_len, std::chrono::nanoseconds max_skew);

    void push_ppg(const acquisition::PpgSample& s);
    void push_camera_frame(CameraFrameMeta frame);

    std::optional<AlignedWindow> try_pop_ready_window();

    [[nodiscard]] std::size_t ppg_backlog() const;

private:
    std::optional<CameraFrameMeta> nearest_frame(std::chrono::steady_clock::time_point t) const;

    const std::size_t window_len_;
    const std::chrono::nanoseconds max_skew_;

    mutable std::mutex mutex_;
    std::deque<acquisition::PpgSample> ppg_buf_;
    std::deque<CameraFrameMeta> cam_buf_;
    std::deque<AlignedWindow> ready_;
};

}  // namespace ppg::sync
