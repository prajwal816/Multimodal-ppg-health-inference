#include "signal_processing/ppg_pipeline.hpp"

#include <algorithm>
#include <cmath>
#include <numeric>

namespace ppg::dsp {

namespace {

void zscore_inplace(std::vector<float>& v) {
    if (v.empty()) {
        return;
    }
    double sum = 0.0;
    for (float x : v) {
        sum += static_cast<double>(x);
    }
    const double mean = sum / static_cast<double>(v.size());
    double var = 0.0;
    for (float x : v) {
        const double d = static_cast<double>(x) - mean;
        var += d * d;
    }
    var /= static_cast<double>(v.size());
    const double stddev = std::sqrt(std::max(var, 1e-12));
    for (float& x : v) {
        x = static_cast<float>((static_cast<double>(x) - mean) / stddev);
    }
}

}  // namespace

PpgProcessingPipeline::PpgProcessingPipeline(double fs_hz, double band_low_hz, double band_high_hz,
                                             std::size_t window_len)
    : ir_bp_(fs_hz, band_low_hz, band_high_hz),
      red_bp_(fs_hz, band_low_hz, band_high_hz),
      window_len_(window_len) {}

ProcessedWindow PpgProcessingPipeline::process(sync::AlignedWindow in) {
    ProcessedWindow out;
    out.meta = std::move(in);

    out.ir_filtered = out.meta.ppg_ir;
    out.red_filtered = out.meta.ppg_red;
    ir_bp_.process_inplace(out.ir_filtered);
    red_bp_.process_inplace(out.red_filtered);

    zscore_inplace(out.ir_filtered);
    zscore_inplace(out.red_filtered);

    out.fused_channel.resize(window_len_);
    for (std::size_t i = 0; i < window_len_; ++i) {
        out.fused_channel[i] = 0.6f * out.ir_filtered[i] + 0.4f * out.red_filtered[i];
    }
    return out;
}

}  // namespace ppg::dsp
