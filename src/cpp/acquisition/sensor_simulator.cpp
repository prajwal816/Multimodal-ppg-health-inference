#include "acquisition/sensor_simulator.hpp"

#include <cmath>

namespace ppg::acquisition {

namespace {
constexpr float kPi = 3.14159265358979323846f;
}

SensorSimulator::SensorSimulator(double hr_bpm, double sample_rate_hz)
    : i2c_(1, 0x57),
      spi_(0, 0),
      phase_(0.0),
      hr_rad_per_s_((hr_bpm / 60.0) * 2.0 * kPi),
      dt_s_(1.0 / std::max(sample_rate_hz, 1.0)) {}

PpgSample SensorSimulator::next_sample(std::chrono::steady_clock::time_point irq_time) {
    std::lock_guard<std::mutex> lock(mutex_);
    phase_ += hr_rad_per_s_ * dt_s_;
    if (phase_ > 2.0 * kPi) {
        phase_ -= 2.0 * kPi;
    }
    const float fundamental = 0.5f * std::sin(static_cast<float>(phase_));
    const float harmonic = 0.12f * std::sin(2.0f * static_cast<float>(phase_) + 0.3f);
    const float resp = 0.04f * std::sin(0.2f * static_cast<float>(phase_));
    const float ir = 0.85f + fundamental + harmonic + resp;
    const float red = 0.80f + 0.9f * fundamental + 0.7f * harmonic + resp;
    const float adc = 2048.0f + 400.0f * fundamental;

    i2c_.inject_sample_for_simulation(red, ir);
    spi_.push_adc_sample(static_cast<std::int16_t>(std::lround(adc)));

    PpgSample s;
    s.t = irq_time;
    s.ir = ir;
    s.red = red;
    s.adc = adc;
    return s;
}

}  // namespace ppg::acquisition
