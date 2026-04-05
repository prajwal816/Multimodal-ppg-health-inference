#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace ppg::acquisition {

/// Simulated I2C register map for a PPG front-end (e.g. MAX30102-style).
/// Production: ioctl I2C_RDWR on /dev/i2c-*.
class I2cPpgDevice {
public:
    explicit I2cPpgDevice(int bus_id, std::uint8_t address);

    [[nodiscard]] bool write_register(std::uint8_t reg, std::uint8_t value);
    [[nodiscard]] std::vector<std::uint8_t> read_burst(std::uint8_t reg, std::size_t len);

    void inject_sample_for_simulation(float red, float ir);

private:
    int bus_id_;
    std::uint8_t address_;
    std::vector<std::uint8_t> fifo_;
    static constexpr std::size_t kMaxFifo = 512;
};

}  // namespace ppg::acquisition
