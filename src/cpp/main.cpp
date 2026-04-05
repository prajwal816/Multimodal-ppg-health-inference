#include "acquisition/sensor_simulator.hpp"
#include "gpio/gpio_interrupt.hpp"
#include "signal_processing/ppg_pipeline.hpp"
#include "sync/multimodal_sync.hpp"
#include "sync/ring_buffer.hpp"

#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

namespace {

using Clock = std::chrono::steady_clock;

struct Config {
    double sample_hz{200.0};
    double camera_fps{30.0};
    std::size_t window_len{64};
    int duration_sec{5};
    bool jsonl{false};
    double hr_bpm{72.0};
};

Config parse_args(int argc, char** argv) {
    Config c;
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--jsonl") == 0) {
            c.jsonl = true;
        } else if (std::strcmp(argv[i], "--seconds") == 0 && i + 1 < argc) {
            c.duration_sec = std::atoi(argv[++i]);
        } else if (std::strcmp(argv[i], "--help") == 0) {
            std::cout
                << "ppg_realtime_demo [options]\n"
                << "  --jsonl       Line-delimited JSON windows to stdout\n"
                << "  --seconds N   Run duration (default 5)\n"
                << "  --help\n";
            std::exit(0);
        }
    }
    return c;
}

std::string format_json_window(const ppg::dsp::ProcessedWindow& w, std::uint64_t seq) {
    std::ostringstream os;
    os << "{\"seq\":" << seq << ",\"sync_delay_ns\":" << w.meta.sync_delay_ns
       << ",\"frame_id\":" << w.meta.nearest_frame.frame_id << ",\"ppg\":[";
    for (std::size_t i = 0; i < w.fused_channel.size(); ++i) {
        if (i) {
            os << ',';
        }
        os << std::setprecision(6) << w.fused_channel[i];
    }
    os << "],\"luma\":" << std::setprecision(5) << w.meta.nearest_frame.mean_luma << "}";
    return os.str();
}

}  // namespace

int main(int argc, char** argv) {
    const Config cfg = parse_args(argc, argv);

    ppg::sync::SpscRingBuffer<ppg::acquisition::PpgSample> ppg_queue(2048);
    ppg::sync::MultimodalSynchronizer synchronizer(
        cfg.window_len, std::chrono::milliseconds(80));
    ppg::dsp::PpgProcessingPipeline dsp(cfg.sample_hz, 0.5, 4.0, cfg.window_len);
    ppg::acquisition::SensorSimulator sensor(cfg.hr_bpm, cfg.sample_hz);

    std::atomic<std::uint64_t> frame_id{0};
    std::atomic<bool> shutdown{false};
    std::atomic<std::uint64_t> windows_out{0};

    std::mutex log_mutex;

    ppg::gpio::InterruptSource irq(cfg.sample_hz);
    irq.set_callback([&](Clock::time_point t) {
        if (shutdown.load(std::memory_order_acquire)) {
            return;
        }
        auto s = sensor.next_sample(t);
        while (!ppg_queue.try_push(std::move(s))) {
            (void)ppg_queue.try_pop();
        }
    });
    irq.start();

    std::thread cam_thread([&] {
        using namespace std::chrono_literals;
        const auto period = std::chrono::duration<double>(1.0 / cfg.camera_fps);
        auto next = Clock::now();
        while (!shutdown.load(std::memory_order_acquire)) {
            next += std::chrono::duration_cast<Clock::duration>(period);
            std::this_thread::sleep_until(next);
            ppg::sync::CameraFrameMeta m;
            m.t = Clock::now();
            m.frame_id = frame_id.fetch_add(1, std::memory_order_relaxed);
            m.mean_luma = 0.42 + 0.01 * std::sin(static_cast<double>(m.frame_id) * 0.07);
            synchronizer.push_camera_frame(m);
        }
    });

    std::thread proc_thread([&] {
        using namespace std::chrono_literals;
        while (!shutdown.load(std::memory_order_acquire)) {
            auto s = ppg_queue.try_pop();
            if (!s.has_value()) {
                std::this_thread::sleep_for(250us);
                continue;
            }
            synchronizer.push_ppg(*s);
            auto w = synchronizer.try_pop_ready_window();
            if (!w.has_value()) {
                continue;
            }
            const auto t0 = Clock::now();
            auto pw = dsp.process(std::move(*w));
            const auto dt_ns =
                std::chrono::duration_cast<std::chrono::nanoseconds>(Clock::now() - t0).count();

            windows_out.fetch_add(1, std::memory_order_relaxed);

            if (cfg.jsonl) {
                std::lock_guard<std::mutex> lk(log_mutex);
                std::cout << format_json_window(pw, windows_out.load(std::memory_order_relaxed))
                          << '\n';
            } else {
                std::lock_guard<std::mutex> lk(log_mutex);
                std::cout << "[proc] window=" << windows_out.load(std::memory_order_relaxed)
                          << " dsp_ns=" << dt_ns << " sync_ns=" << pw.meta.sync_delay_ns << "\n";
            }
        }
    });

    std::thread infer_thread([&] {
        using namespace std::chrono_literals;
        const auto deadline = Clock::now() + std::chrono::seconds(cfg.duration_sec);
        while (Clock::now() < deadline && !shutdown.load(std::memory_order_acquire)) {
            std::this_thread::sleep_for(100ms);
        }
        shutdown.store(true, std::memory_order_release);
    });

    infer_thread.join();
    irq.stop();
    cam_thread.join();
    proc_thread.join();

    if (!cfg.jsonl) {
        std::cout << "[summary] windows_emitted=" << windows_out.load() << " target_latency_budget_ms=50\n";
    }
    return 0;
}
