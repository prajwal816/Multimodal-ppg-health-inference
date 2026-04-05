#pragma once

#include "signal_processing/bandpass_filter.hpp"
#include "sync/multimodal_sync.hpp"

#include <vector>

namespace ppg::dsp {

struct ProcessedWindow {
    sync::AlignedWindow meta;
    std::vector<float> ir_filtered;
    std::vector<float> red_filtered;
    std::vector<float> fused_channel;
};

class PpgProcessingPipeline {
public:
    explicit PpgProcessingPipeline(double fs_hz, double band_low_hz, double band_high_hz,
                                 std::size_t window_len);

    ProcessedWindow process(sync::AlignedWindow in);

private:
    BandpassChain ir_bp_;
    BandpassChain red_bp_;
    std::size_t window_len_;
};

}  // namespace ppg::dsp
