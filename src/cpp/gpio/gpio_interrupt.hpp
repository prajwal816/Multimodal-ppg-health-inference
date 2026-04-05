#pragma once

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <mutex>
#include <thread>

namespace ppg::gpio {

/// Software-timed interrupt source for embedded bring-up when hardware IRQ is unavailable.
/// On Raspberry Pi, replace tick generation with wiringPi / pigpio edge callbacks.
class InterruptSource {
public:
    using Callback = std::function<void(std::chrono::steady_clock::time_point)>;

    explicit InterruptSource(double rate_hz);
    ~InterruptSource();

    void set_callback(Callback cb);
    void start();
    void stop();
    [[nodiscard]] bool running() const { return running_.load(std::memory_order_acquire); }

private:
    void run_loop();

    Callback callback_;
    std::thread worker_;
    std::atomic<bool> running_{false};
    std::atomic<bool> stop_requested_{false};
    const std::chrono::nanoseconds period_;
    mutable std::mutex cb_mutex_;
};

}  // namespace ppg::gpio
