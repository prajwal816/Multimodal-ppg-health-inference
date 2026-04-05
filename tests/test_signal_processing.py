import numpy as np

from preprocessing.ppg_ops import bandpass_sos, normalize_window


def test_bandpass_finite():
    x = np.sin(np.linspace(0, 6.28, 200, dtype=np.float32)) * 0.1 + 0.9
    y = bandpass_sos(x, 200.0, 0.5, 4.0)
    assert np.all(np.isfinite(y))
    assert y.shape == x.shape


def test_normalize_zero_mean_unit_std():
    x = np.linspace(0, 1, 64, dtype=np.float32)
    z = normalize_window(x)
    assert abs(float(z.mean())) < 0.05
    assert 0.8 < float(z.std()) < 1.2
