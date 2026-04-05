#include "gpio/gpio_interrupt.hpp"

#include <thread>

namespace ppg::gpio {

InterruptSource::InterruptSource(double rate_hz)
    : period_(static_cast<std::chrono::nanoseconds::rep>(
          1e9 / std::max(rate_hz, 1.0))) {}

InterruptSource::~InterruptSource() { stop(); }

void InterruptSource::set_callback(Callback cb) {
    std::lock_guard<std::mutex> lock(cb_mutex_);
    callback_ = std::move(cb);
}

void InterruptSource::start() {
    if (running_.exchange(true, std::memory_order_acq_rel)) {
        return;
    }
    stop_requested_.store(false, std::memory_order_release);
    worker_ = std::thread([this] { run_loop(); });
}

void InterruptSource::stop() {
    stop_requested_.store(true, std::memory_order_release);
    if (worker_.joinable()) {
        worker_.join();
    }
    running_.store(false, std::memory_order_release);
}

void InterruptSource::run_loop() {
    auto next = std::chrono::steady_clock::now();
    while (!stop_requested_.load(std::memory_order_acquire)) {
        next += period_;
        std::this_thread::sleep_until(next);
        Callback local;
        {
            std::lock_guard<std::mutex> lock(cb_mutex_);
            local = callback_;
        }
        if (local) {
            local(std::chrono::steady_clock::now());
        }
    }
}

}  // namespace ppg::gpio
