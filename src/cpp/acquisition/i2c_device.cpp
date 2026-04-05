#include "acquisition/i2c_device.hpp"

#include <algorithm>
#include <cstring>

namespace ppg::acquisition {

I2cPpgDevice::I2cPpgDevice(int bus_id, std::uint8_t address)
    : bus_id_(bus_id), address_(address) {}

bool I2cPpgDevice::write_register(std::uint8_t /*reg*/, std::uint8_t /*value*/) {
    (void)bus_id_;
    (void)address_;
    return true;
}

std::vector<std::uint8_t> I2cPpgDevice::read_burst(std::uint8_t /*reg*/, std::size_t len) {
    std::vector<std::uint8_t> out;
    const std::size_t n = std::min(len, fifo_.size());
    out.assign(fifo_.end() - static_cast<std::ptrdiff_t>(n), fifo_.end());
    fifo_.erase(fifo_.end() - static_cast<std::ptrdiff_t>(n), fifo_.end());
    return out;
}

void I2cPpgDevice::inject_sample_for_simulation(float red, float ir) {
    std::uint8_t block[8];
    std::memcpy(block, &red, 4);
    std::memcpy(block + 4, &ir, 4);
    for (std::uint8_t b : block) {
        if (fifo_.size() >= kMaxFifo) {
            fifo_.erase(fifo_.begin());
        }
        fifo_.push_back(b);
    }
}

}  // namespace ppg::acquisition
