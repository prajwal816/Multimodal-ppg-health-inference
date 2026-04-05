#include "sync/multimodal_sync.hpp"

#include <algorithm>
#include <cmath>

namespace ppg::sync {

MultimodalSynchronizer::MultimodalSynchronizer(std::size_t ppg_window_len,
                                               std::chrono::nanoseconds max_skew)
    : window_len_(ppg_window_len), max_skew_(max_skew) {}

void MultimodalSynchronizer::push_ppg(const acquisition::PpgSample& s) {
    std::lock_guard<std::mutex> lock(mutex_);
    ppg_buf_.push_back(s);
    while (ppg_buf_.size() > window_len_ * 4) {
        ppg_buf_.pop_front();
    }

    if (ppg_buf_.size() < window_len_) {
        return;
    }

    std::vector<float> ir;
    std::vector<float> red;
    ir.reserve(window_len_);
    red.reserve(window_len_);
    const auto end_it = ppg_buf_.begin() + static_cast<std::ptrdiff_t>(window_len_);
    for (auto it = ppg_buf_.begin(); it != end_it; ++it) {
        ir.push_back(it->ir);
        red.push_back(it->red);
    }
    const auto center_t = ppg_buf_[window_len_ / 2].t;
    const auto nf = nearest_frame(center_t);
    if (!nf.has_value()) {
        return;
    }
    const std::int64_t skew_ns =
        std::chrono::duration_cast<std::chrono::nanoseconds>(center_t - nf->t).count();

    if (std::chrono::nanoseconds(std::llabs(skew_ns)) > max_skew_) {
        return;
    }

    AlignedWindow w;
    w.window_end = ppg_buf_[window_len_ - 1].t;
    w.ppg_ir = std::move(ir);
    w.ppg_red = std::move(red);
    w.nearest_frame = *nf;
    w.sync_delay_ns = skew_ns;
    ready_.push_back(std::move(w));

    ppg_buf_.erase(ppg_buf_.begin(), end_it);
}

void MultimodalSynchronizer::push_camera_frame(CameraFrameMeta frame) {
    std::lock_guard<std::mutex> lock(mutex_);
    cam_buf_.push_back(frame);
    while (cam_buf_.size() > 64) {
        cam_buf_.pop_front();
    }
}

std::optional<AlignedWindow> MultimodalSynchronizer::try_pop_ready_window() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (ready_.empty()) {
        return std::nullopt;
    }
    AlignedWindow w = std::move(ready_.front());
    ready_.pop_front();
    return w;
}

std::size_t MultimodalSynchronizer::ppg_backlog() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return ppg_buf_.size();
}

std::optional<CameraFrameMeta> MultimodalSynchronizer::nearest_frame(
    std::chrono::steady_clock::time_point t) const {
    if (cam_buf_.empty()) {
        return std::nullopt;
    }
    CameraFrameMeta best = cam_buf_.front();
    auto best_dt = std::chrono::abs(t - best.t);
    for (const auto& c : cam_buf_) {
        const auto dt = std::chrono::abs(t - c.t);
        if (dt < best_dt) {
            best_dt = dt;
            best = c;
        }
    }
    return best;
}

}  // namespace ppg::sync
