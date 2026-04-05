#include "signal_processing/bandpass_filter.hpp"

namespace ppg::dsp {

namespace {

constexpr double kPi = 3.14159265358979323846;

void rbj_lowpass(double f0, double fs, double Q, double& b0, double& b1, double& b2, double& a1,
                 double& a2) {
    const double w0 = 2.0 * kPi * f0 / fs;
    const double cosw0 = std::cos(w0);
    const double sinw0 = std::sin(w0);
    const double alpha = sinw0 / (2.0 * Q);
    const double b0l = (1.0 - cosw0) / 2.0;
    const double b1l = 1.0 - cosw0;
    const double b2l = (1.0 - cosw0) / 2.0;
    const double a0l = 1.0 + alpha;
    b0 = b0l / a0l;
    b1 = b1l / a0l;
    b2 = b2l / a0l;
    a1 = -2.0 * cosw0 / a0l;
    a2 = (1.0 - alpha) / a0l;
}

void rbj_highpass(double f0, double fs, double Q, double& b0, double& b1, double& b2, double& a1,
                  double& a2) {
    const double w0 = 2.0 * kPi * f0 / fs;
    const double cosw0 = std::cos(w0);
    const double sinw0 = std::sin(w0);
    const double alpha = sinw0 / (2.0 * Q);
    const double b0h = (1.0 + cosw0) / 2.0;
    const double b1h = -(1.0 + cosw0);
    const double b2h = (1.0 + cosw0) / 2.0;
    const double a0h = 1.0 + alpha;
    b0 = b0h / a0h;
    b1 = b1h / a0h;
    b2 = b2h / a0h;
    a1 = -2.0 * cosw0 / a0h;
    a2 = (1.0 - alpha) / a0h;
}

}  // namespace

Biquad::Biquad(double b0, double b1, double b2, double a1, double a2)
    : b0_(b0), b1_(b1), b2_(b2), a1_(a1), a2_(a2) {}

void Biquad::reset() {
    x1_ = x2_ = y1_ = y2_ = 0.0;
}

float Biquad::process(float x) {
    const double y = b0_ * x + b1_ * x1_ + b2_ * x2_ - a1_ * y1_ - a2_ * y2_;
    x2_ = x1_;
    x1_ = x;
    y2_ = y1_;
    y1_ = y;
    return static_cast<float>(y);
}

BandpassChain::BandpassChain(double fs_hz, double fl_hz, double fh_hz) {
    double b0, b1, b2, a1, a2;
    rbj_highpass(fl_hz, fs_hz, 0.707, b0, b1, b2, a1, a2);
    stages_.emplace_back(b0, b1, b2, a1, a2);
    rbj_lowpass(fh_hz, fs_hz, 0.707, b0, b1, b2, a1, a2);
    stages_.emplace_back(b0, b1, b2, a1, a2);
}

void BandpassChain::reset() {
    for (auto& s : stages_) {
        s.reset();
    }
}

void BandpassChain::process_inplace(std::vector<float>& samples) {
    for (auto& s : samples) {
        float y = s;
        for (auto& stage : stages_) {
            y = stage.process(y);
        }
        s = y;
    }
}

}  // namespace ppg::dsp
