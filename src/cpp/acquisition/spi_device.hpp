#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace ppg::acquisition {

/// Simulated SPI frame reader for alternate PPG ADCs.
/// Production: spidev ioctl SPI_IOC_MESSAGE.
class SpiPpgDevice {
public:
    SpiPpgDevice(int bus, int cs);

    [[nodiscard]] std::vector<std::uint8_t> transfer(const std::vector<std::uint8_t>& tx);

    void push_adc_sample(std::int16_t value);

private:
    int bus_;
    int cs_;
    std::vector<std::uint8_t> rx_queue_;
};

}  // namespace ppg::acquisition
