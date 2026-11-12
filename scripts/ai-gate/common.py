"""Shared Noise Gate (AI) training code: the plugin's band split, noise synthesis and model.

The band split mirrors NeuralAmpModeler/AIGate.h: 96 constant-Q (Q=4) band-passes,
5 Hz .. 20 kHz on a log grid. Training evaluates the causal filters in the frequency
domain, which is exact up to the circular wrap of the zero-padded tail.
"""
import math

import numpy as np
import torch
import torch.nn as nn

BANDS = 96
RATE = 48000
HOP = 32  # control tick in samples at 48 kHz (1500 Hz)
Q = 4.0
STEP = math.log(4000.0) / (BANDS - 1)
_ROOT = math.sqrt(4 * Q * Q - 1)
SYNTHESIS = STEP * _ROOT / (2 * math.atan(_ROOT))


def band_coefficients(rate=RATE):
    i = np.arange(BANDS)
    frequency = np.minimum(5.0 * np.exp(STEP * i), 0.45 * rate)
    omega = 2 * np.pi * frequency / rate
    alpha = omega / (2 * Q)
    inverse = 1 / (1 + alpha)
    return alpha * inverse, -2 * np.cos(omega) * inverse, (1 - alpha) * inverse  # b0, a1, a2


def band_response(n_fft, rate=RATE, device="cpu"):
    """Complex response of every band at the rfft bins: (BANDS, n_fft//2+1)."""
    b0, a1, a2 = (torch.tensor(x, dtype=torch.float64, device=device)[:, None] for x in band_coefficients(rate))
    w = torch.arange(n_fft // 2 + 1, dtype=torch.float64, device=device)[None] * (2 * math.pi / n_fft)
    z1 = torch.polar(torch.ones_like(w), -w)
    z2 = z1 * z1
    return (b0 * (1 - z2) / (1 + a1 * z1 + a2 * z2)).to(torch.complex64)


def split_bands(x, response):
    """x: (B, T) with zero-padded tail -> (B, BANDS, T)."""
    return torch.fft.irfft(torch.fft.rfft(x)[:, None] * response[None], n=x.shape[-1])


def tick_power(bands, hop=HOP):
    """Mean band power over each hop: (B, BANDS, T) -> (B, J, BANDS)."""
    b, k, t = bands.shape
    return bands.reshape(b, k, t // hop, hop).square().mean(-1).transpose(1, 2)


def features(power):
    return (10 * torch.log10(power + 1e-12) + 60) / 30


def upsample_gains(g, hop=HOP):
    """Tick gains (B, J, K) -> per-sample (B, K, J*hop).

    Tick j is computed from samples [(j-1)hop, j*hop) and steers block [j*hop, (j+1)hop)
    with a linear ramp from the previous tick. Index 0 of g is tick 1. As in the plugin,
    the gate starts closed: block 0 is silent and the first ramp rises from zero.
    """
    b, j, k = g.shape
    ramp = torch.arange(1, hop + 1, device=g.device, dtype=g.dtype) / hop
    previous = torch.cat([torch.zeros_like(g[:, :1]), g[:, :-1]], 1)
    up = previous[..., None] + (g - previous)[..., None] * ramp  # (B, J, K, hop): blocks 1..J
    up = up.permute(0, 2, 1, 3).reshape(b, k, j * hop)
    return torch.cat([torch.zeros(b, k, hop, device=g.device, dtype=g.dtype), up[..., :-hop]], -1)


def synthesize(x, bands, g_up):
    """NoisePrint reconstruction: equal gains give the scaled input exactly."""
    reference = g_up.amin(1)
    return reference * x + SYNTHESIS * (bands * (g_up - reference[:, None])).sum(1)


class GateNet(nn.Module):
    def __init__(self, hidden=64, inner=64):
        super().__init__()
        self.inp = nn.Linear(BANDS, inner)
        self.gru = nn.GRU(inner, hidden, batch_first=True)
        self.out = nn.Linear(hidden, BANDS)

    def forward(self, f, state=None):
        h, state = self.gru(torch.tanh(self.inp(f)), state)
        return torch.sigmoid(self.out(h)), state


# ---------------------------------------------------------------- noise synthesis
def _shaped_noise(batch, n, shape_fn, device, generator):
    spectrum = torch.fft.rfft(torch.randn(batch, n, device=device, generator=generator))
    f = torch.fft.rfftfreq(n, 1 / RATE).to(device)[None]
    x = torch.fft.irfft(spectrum * shape_fn(f.clamp(min=1.0)), n=n)
    return x / x.square().mean(-1, keepdim=True).sqrt().clamp(min=1e-9)


def _smooth_curve(batch, n, device, generator, depth_db, points=6):
    knots = (torch.rand(batch, 1, points, device=device, generator=generator) * 2 - 1) * depth_db
    curve = torch.nn.functional.interpolate(knots, size=n, mode="linear", align_corners=True)[:, 0]
    return 10 ** (curve / 20)


def synth_noise(batch, n, device, generator=None):
    """Hiss, mains hum and buzz at random levels with slow drift. Returns (B, n)."""
    rand = lambda *s: torch.rand(*s, device=device, generator=generator)
    # Hiss: white/pink blend through a pickup-like resonant low-pass.
    alpha = rand(batch, 1) * 1.2
    f0 = 1500 * (8000 / 1500) ** rand(batch, 1)
    q = 0.6 + rand(batch, 1) * 4
    flat = rand(batch, 1) ** 2  # some flat preamp hiss above the resonance
    highpass = 20 * 10 ** (rand(batch, 1) * 0.9)

    def shape(f):
        r = f / f0
        lowpass = 1 / torch.sqrt((1 - r * r) ** 2 + (r / q) ** 2)
        return f ** (-alpha / 2) * (lowpass + flat * 0.3) * (f / torch.sqrt(f * f + highpass**2))

    hiss = _shaped_noise(batch, n, shape, device, generator)
    hiss = hiss * 10 ** ((-100 + rand(batch, 1) * 62) / 20) * _smooth_curve(batch, n, device, generator, 3)
    # Hum and buzz: harmonics of 50/60 Hz.
    t = torch.arange(n, device=device)[None, None] / RATE
    mains = torch.where(rand(batch, 1, 1) < 0.5, 50.0, 60.0) * (1 + (rand(batch, 1, 1) - 0.5) * 0.02)
    k = torch.arange(1, 81, device=device)[None, :, None]
    beta = torch.where(rand(batch, 1, 1) < 0.35, rand(batch, 1, 1) * 0.8, 0.8 + rand(batch, 1, 1) * 2)
    amplitude = rand(batch, 80, 1) ** 2 * k ** (-beta)
    amplitude = amplitude * torch.where(k % 2 == 0, rand(batch, 1, 1), torch.ones(1, device=device))
    amplitude = amplitude * (k * mains < 0.4 * RATE)
    hum = torch.zeros(batch, n, device=device)
    for start in range(0, 80, 16):  # bound memory
        kk = k[:, start : start + 16]
        phase = rand(batch, 16, 1) * 2 * math.pi
        hum = hum + (amplitude[:, start : start + 16] * torch.sin(2 * math.pi * kk * mains * t + phase)).sum(1)
    hum = hum / hum.square().mean(-1, keepdim=True).sqrt().clamp(min=1e-9)
    hum_level = 10 ** ((-98 + rand(batch, 1) * 64) / 20) * (rand(batch, 1) < 0.7)
    hum = hum * hum_level * _smooth_curve(batch, n, device, generator, 4)
    # Occasional step, as when the guitar turns towards a transformer.
    step_at = (rand(batch, 1) * n).long()
    step = torch.where(rand(batch, 1) < 0.25, 10 ** ((rand(batch, 1) * 24 - 12) / 20), torch.ones(batch, 1, device=device))
    position = torch.arange(n, device=device)[None]
    hum = hum * torch.where(position > step_at, step, torch.ones(1, device=device))
    return hiss + hum
