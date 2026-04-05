#include "sync/multimodal_sync.hpp"

#include <cassert>
#include <chrono>
#include <cmath>

int main() {
    using namespace std::chrono_literals;
    ppg::sync::MultimodalSynchronizer sync(4, 50ms);
    const auto t0 = std::chrono::steady_clock::now();

    ppg::sync::CameraFrameMeta cam;
    cam.t = t0 + 2ms;
    cam.frame_id = 1;
    cam.mean_luma = 0.5;
    sync.push_camera_frame(cam);

    for (int i = 0; i < 4; ++i) {
        ppg::acquisition::PpgSample s;
        s.t = t0 + std::chrono::milliseconds(i * 5);
        s.ir = 1.0f + 0.1f * static_cast<float>(i);
        s.red = 0.9f + 0.1f * static_cast<float>(i);
        s.adc = 100.0f;
        sync.push_ppg(s);
    }

    auto w = sync.try_pop_ready_window();
    assert(w.has_value());
    assert(w->ppg_ir.size() == 4);
    assert(std::llabs(w->sync_delay_ns) < 50'000'000);
    return 0;
}
