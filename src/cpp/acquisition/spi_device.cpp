#include "acquisition/spi_device.hpp"

namespace ppg::acquisition {

SpiPpgDevice::SpiPpgDevice(int bus, int cs) : bus_(bus), cs_(cs) {}

std::vector<std::uint8_t> SpiPpgDevice::transfer(const std::vector<std::uint8_t>& tx) {
    (void)tx;
    (void)bus_;
    (void)cs_;
    std::vector<std::uint8_t> out;
    if (rx_queue_.size() >= 2) {
        out.push_back(rx_queue_.front());
        rx_queue_.erase(rx_queue_.begin());
        out.push_back(rx_queue_.front());
        rx_queue_.erase(rx_queue_.begin());
    }
    return out;
}

void SpiPpgDevice::push_adc_sample(std::int16_t value) {
    rx_queue_.push_back(static_cast<std::uint8_t>(value & 0xFF));
    rx_queue_.push_back(static_cast<std::uint8_t>((value >> 8) & 0xFF));
}

}  // namespace ppg::acquisition
