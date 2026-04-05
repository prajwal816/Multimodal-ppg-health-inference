#pragma once

#include "acquisition/i2c_device.hpp"
#include "acquisition/spi_device.hpp"

#include <chrono>
#include <cstdint>
#include <mutex>

namespace ppg::acquisition {

struct PpgSample {
    std::chrono::steady_clock::time_point t;
    float ir;
    float red;
    float adc;
};

/// Generates physiologically plausible PPG waveform + drives simulated I2C/SPI FIFOs.
class SensorSimulator {
public:
    SensorSimulator(double hr_bpm, double sample_rate_hz);

    PpgSample next_sample(std::chrono::steady_clock::time_point irq_time);

    I2cPpgDevice& i2c() { return i2c_; }
    SpiPpgDevice& spi() { return spi_; }

private:
    I2cPpgDevice i2c_;
    SpiPpgDevice spi_;
    double phase_;
    double hr_rad_per_s_;
    double dt_s_;
    std::mutex mutex_;
};

}  // namespace ppg::acquisition
