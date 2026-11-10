"""Train the Noise Gate (AI) network end to end through the plugin's band split.

Clean guitar (48 kHz arrays from prep.py, optionally a small extra DI set) is mixed with
synthetic and recorded noise. The gains predicted from the mixture are applied to the clean
and noise parts separately, so damage to notes and leftover noise are scored independently.
"""
import argparse
import time
from pathlib import Path

import numpy as np
import torch
import torch.nn.functional as F

from common import HOP, RATE, GateNet, band_response, features, split_bands, synth_noise, synthesize, tick_power, upsample_gains

PAD = 24576  # zero tail: bounds the circular wrap of the lowest resonators
T = 245760  # 5.12 s: long enough to hear the noise alone, then a note, then the noise again
N_FFT = T + PAD
TERMS = ("mix", "distortion", "wave", "gap", "retention", "attack")


def crops(data, batch, rng, low=0.88, high=1.15):
    out = np.zeros((batch, T), np.float32)
    for b in range(batch):
        speed = rng.uniform(low, high)
        length = int(T * speed) + 2
        start = rng.integers(0, len(data) - length)
        out[b] = np.interp(np.arange(T) * speed, np.arange(length), data[start : start + length])
    return out


def real_noise(data, batch, device, rng, generator):
    """Crops of recorded noise at random level with slow drift -> (B, T)."""
    n = torch.from_numpy(crops(data, batch, rng, 0.92, 1.08)).to(device)
    rms = n.square().mean(-1, keepdim=True).sqrt().clamp(min=1e-9)
    level = 10 ** ((-100 + torch.rand(batch, 1, device=device, generator=generator) * 58) / 20)
    knots = (torch.rand(batch, 1, 5, device=device, generator=generator) * 2 - 1) * 4
    drift = 10 ** (F.interpolate(knots, size=T, mode="linear", align_corners=True)[:, 0] / 20)
    return n / rms * level * drift


def make_noise(batch, device, rng, generator, recorded=None):
    n = synth_noise(batch, T, device, generator)
    if recorded is not None:  # 40% recorded noise, 15% recorded plus synthetic, rest synthetic
        choice = torch.rand(batch, 1, device=device, generator=generator)
        real = real_noise(recorded, batch, device, rng, generator)
        n = torch.where(choice < 0.4, real, torch.where(choice < 0.55, real + n, n))
    return n * (torch.rand(batch, 1, device=device, generator=generator) > 0.08)  # some clean-input examples


def make_clean(data, batch, device, rng, generator, extra=None):
    """Random crops with speed, level, tone, mute and ring-out augmentation -> (B, T)."""
    out = crops(data, batch, rng)
    if extra is not None:  # a share of clips from the small electric DI set
        pick = rng.random(batch) < 0.2
        if pick.any():
            out[pick] = crops(extra, int(pick.sum()), rng, 0.94, 1.06)
    c = torch.from_numpy(out).to(device)
    rand = lambda *s: torch.rand(*s, device=device, generator=generator)
    one = torch.ones(1, device=device)
    peak = c.abs().amax(-1, keepdim=True)
    target = 10 ** ((-36 + rand(batch, 1) * 35) / 20)
    c = torch.where(peak > 3e-3, c * target / peak.clamp(min=1e-6), c)
    # Tone knob / cable roll-off.
    f = torch.fft.rfftfreq(T, 1 / RATE).to(device)[None]
    cutoff = torch.where(rand(batch, 1) < 0.4, 1200 * (16000 / 1200) ** rand(batch, 1), torch.full((batch, 1), 1e6, device=device))
    c = torch.fft.irfft(torch.fft.rfft(c) / torch.sqrt(1 + (f / cutoff) ** 2), n=T)
    # Mutes: decay to exact silence, then resume with a short fade.
    envelope = torch.ones(batch, T, device=device)
    position = torch.arange(T, device=device)[None] / RATE
    for _ in range(3):
        active = rand(batch, 1) < 0.5
        at = rand(batch, 1) * 4.9
        tau = 0.004 * (0.4 / 0.004) ** rand(batch, 1)
        length = 0.15 + rand(batch, 1) * 1.5
        fade_out = torch.exp(-(position - at).clamp(min=0) / tau)
        fade_out = torch.where(fade_out < 1e-3, torch.zeros(1, device=device), fade_out)
        fade_in = ((position - at - length) / (0.001 + rand(batch, 1) * 0.004)).clamp(0, 1)
        envelope = envelope * torch.where(active, torch.maximum(fade_out, fade_in), one)
    # Slow ring-outs that sink through the noise floor without a cut-off.
    ring = rand(batch, 1) < 0.5
    tau = 0.15 * (1.2 / 0.15) ** rand(batch, 1)
    envelope = envelope * torch.where(ring, torch.exp(-(position - rand(batch, 1) * 4.0).clamp(min=0) / tau), one)
    # A player rarely starts mid-note: most clips open with the noise alone, then an abrupt entry.
    # The gate can learn what the idle noise is and still has to open at once.
    lead = torch.where(rand(batch, 1) < 0.7, 0.3 + rand(batch, 1) * 1.2, torch.zeros(1, device=device))
    envelope = envelope * ((position - lead) / (0.001 + rand(batch, 1) * 0.004)).clamp(0, 1)
    return c * envelope * (rand(batch, 1) > 0.08)  # some noise-only examples


def _pool():
    """rfft bins of a 2048 frame -> 32 log-spaced bands, 60 Hz .. 16 kHz."""
    f = torch.fft.rfftfreq(2048, 1 / RATE)
    edges = 60 * (16000 / 60) ** (torch.arange(33) / 32)
    return ((f[:, None] >= edges[None, :-1]) & (f[:, None] < edges[None, 1:])).float()


def stft_compressed(x, n_fft):
    window = torch.hann_window(n_fft, device=x.device)
    s = torch.stft(x, n_fft, n_fft // 4, window=window, return_complex=True)
    return (s.abs() + 1e-7) ** 0.3


def run(model, c, n, response, pool, hop=HOP):
    """Returns the loss terms (tensors) and plain statistics for one batch."""
    with torch.no_grad():
        bc = split_bands(F.pad(c, (0, PAD)), response)[..., :T]
        bn = split_bands(F.pad(n, (0, PAD)), response)[..., :T]
        f = features(tick_power(bc + bn, hop))
    g, _ = model(f)
    g_up = upsample_gains(g, hop)
    out_c, out_n = synthesize(c, bc, g_up), synthesize(n, bn, g_up)
    terms = {"mix": 0.0, "distortion": 0.0}
    for n_fft in (512, 2048):
        target = stft_compressed(c, n_fft)
        terms["mix"] = terms["mix"] + (stft_compressed(out_c + out_n, n_fft) - target).square().mean()
        terms["distortion"] = terms["distortion"] + (stft_compressed(out_c, n_fft) - target).square().mean()
    # Level-invariant waveform error of the wanted signal: keeps clearly present notes transparent.
    floor = 1e-8 * T
    terms["wave"] = torch.log10(((out_c - c).square().sum(-1) + floor) / (c.square().sum(-1) + floor)).mean()
    # Gate behaviour: where the wanted signal is absent, reward every dB of closing (to -60 dB).
    with torch.no_grad():
        gap = (F.avg_pool1d(c.abs()[:, None], 961, 1, 480)[:, 0] < 1e-5).float()
    gap_noise = (n.square() * gap).sum(-1)
    terms["gap"] = torch.log10(((out_n.square() * gap).sum(-1) + 1e-6 * gap_noise + 1e-12) / (gap_noise + 1e-12)).mean()
    # Sustain: wherever a band of the wanted signal exceeds the noise in that band, losing it costs.
    window = torch.hann_window(2048, device=c.device)
    energy = lambda x: torch.stft(x, 2048, 512, window=window, return_complex=True).abs().square().transpose(1, 2) @ pool
    e_c, e_out, e_n = energy(c), energy(out_c), energy(n)
    keep = ((e_c > e_n) & (e_c > 1e-12)).float()
    terms["retention"] = (keep * torch.relu(torch.log10(e_c + 1e-14) - torch.log10(e_out + 1e-14))).sum() / keep.sum().clamp(min=1)
    # Attack: in the 20 ms after the wanted signal jumps up, every dB lost costs, at 2 ms resolution.
    short = lambda x: x.reshape(x.shape[0], -1, 96).square().sum(-1)
    s_c, s_out = short(c), short(out_c)
    with torch.no_grad():
        earlier = F.avg_pool1d(F.pad(s_c, (10, 0))[:, None], 8, 1)[:, 0, : s_c.shape[-1]]
        onset = ((s_c > 8 * earlier + 1e-9) & (s_c > short(n))).float()
        opening = F.max_pool1d(F.pad(onset, (9, 0))[:, None], 10, 1)[:, 0]
    terms["attack"] = (opening * torch.relu(torch.log10(s_c + 1e-12) - torch.log10(s_out + 1e-12))).sum() / opening.sum().clamp(min=1)
    with torch.no_grad():
        db = lambda a, b: 10 * torch.log10(a.square().sum(-1).clamp(min=1e-20) / b.square().sum(-1).clamp(min=1e-20))
        has_c, has_n = c.abs().amax(-1) > 0, n.abs().amax(-1) > 0
        stats = {"noise_reduction_db": db(n, out_n)[has_n].mean().item() if has_n.any() else float("nan"),
                 "clean_sdr_db": db(c, out_c - c)[has_c].mean().item() if has_c.any() else float("nan")}
    return terms, stats


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--data", required=True, help="48 kHz clean training array (prep.py)")
    p.add_argument("--val", required=True, help="48 kHz clean array of a held-out player")
    p.add_argument("--out", required=True)
    p.add_argument("--extra-clean", default="", help="optional small 48 kHz electric DI array, cleaned of its floor")
    p.add_argument("--noise", default="", help="optional 48 kHz array of recorded noise-only takes")
    p.add_argument("--val-noise", default="", help="optional held-out recorded noise for validation")
    p.add_argument("--init", default="", help="checkpoint to continue from")
    p.add_argument("--steps", type=int, default=20000)
    p.add_argument("--batch", type=int, default=5)
    p.add_argument("--hidden", type=int, default=64)
    p.add_argument("--inner", type=int, default=64)
    p.add_argument("--hop", type=int, default=HOP)
    p.add_argument("--lr", type=float, default=1e-3)
    p.add_argument("--seed", type=int, default=1)
    for name, weight in (("distortion", 2.0), ("wave", 0.01), ("gap", 0.01), ("retention", 0.1), ("attack", 0.1)):
        p.add_argument(f"--{name}", type=float, default=weight, help="loss weight")
    args = p.parse_args()
    weights = {"mix": 1.0, **{name: getattr(args, name) for name in TERMS[1:]}}
    device = "cuda" if torch.cuda.is_available() else "cpu"
    out = Path(args.out)
    out.mkdir(parents=True, exist_ok=True)
    load = lambda path: np.load(path) if path else None
    data, val = np.load(args.data), np.load(args.val)
    extra, noise, val_noise = load(args.extra_clean), load(args.noise), load(args.val_noise)
    torch.manual_seed(args.seed)
    rng = np.random.default_rng(args.seed)
    generator = torch.Generator(device).manual_seed(args.seed)
    response, pool = band_response(N_FFT, device=device), _pool().to(device)
    model = GateNet(args.hidden, args.inner).to(device)
    if args.init:
        model.load_state_dict(torch.load(args.init, map_location=device, weights_only=False)["model"])
    print("parameters", sum(x.numel() for x in model.parameters()), "weights", weights, flush=True)
    optimizer = torch.optim.Adam(model.parameters(), lr=args.lr)
    schedule = torch.optim.lr_scheduler.OneCycleLR(optimizer, args.lr, total_steps=args.steps, pct_start=0.03)
    total = lambda terms: sum(weights[name] * terms[name] for name in TERMS)

    def validate():
        model.eval()
        vr, vg = np.random.default_rng(7), torch.Generator(device).manual_seed(7)
        sums = np.zeros(len(TERMS) + 2)
        with torch.no_grad():
            for _ in range(12):
                terms, stats = run(model, make_clean(val, args.batch, device, vr, vg),
                                   make_noise(args.batch, device, vr, vg, val_noise), response, pool, args.hop)
                sums += [float(terms[name]) for name in TERMS] + [stats["noise_reduction_db"], stats["clean_sdr_db"]]
        model.train()
        return sums / 12

    started = time.time()
    running = np.zeros(len(TERMS))
    for step in range(1, args.steps + 1):
        terms, _ = run(model, make_clean(data, args.batch, device, rng, generator, extra),
                       make_noise(args.batch, device, rng, generator, noise), response, pool, args.hop)
        optimizer.zero_grad(set_to_none=True)
        total(terms).backward()
        torch.nn.utils.clip_grad_norm_(model.parameters(), 1.0)
        optimizer.step()
        schedule.step()
        running += [float(terms[name]) for name in TERMS]
        if step % 100 == 0:
            print(step, " ".join(f"{name} {value / 100:.4f}" for name, value in zip(TERMS, running)), f"{time.time() - started:.0f}s", flush=True)
            running[:] = 0
        if step % 1000 == 0 or step == args.steps:
            v = validate()
            print("  val", " ".join(f"{name} {value:.4f}" for name, value in zip(TERMS, v)),
                  f"| noise reduction {v[-2]:.1f} dB clean SDR {v[-1]:.1f} dB", flush=True)
            checkpoint = {"model": model.state_dict(), "args": vars(args), "step": step}
            torch.save(checkpoint, out / f"step{step}.pt")
            torch.save(checkpoint, out / "model.pt")


if __name__ == "__main__":
    main()
