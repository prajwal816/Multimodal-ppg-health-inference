#pragma once

#include <cmath>
#include <vector>

namespace ppg::dsp {

/// Direct Form I biquad IIR; cascade for bandpass (0.5–4 Hz typical for PPG pulse rate at 200 Hz).
class Biquad {
public:
    Biquad(double b0, double b1, double b2, double a0, double a1, double a2);

    float process(float x);

private:
    double b0_, b1_, b2_;
    double a1_, a2_;
    double x1_{0}, x2_{0};
    double y1_{0}, y2_{0};
};

class BandpassChain {
public:
    /// fs_hz sample rate, fl_hz/fh_hz band edges (approximate Butterworth via bilinear + biquad).
    BandpassChain(double fs_hz, double fl_hz, double fh_hz);

    void reset();
    void process_inplace(std::vector<float>& samples);

private:
    std::vector<Biquad> stages_;
};

}  // namespace ppg::dsp
