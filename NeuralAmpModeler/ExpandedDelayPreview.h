#pragma once
#include <bit>
#include "EffectPreview.h"

// Small, settled-control models for the editor. These describe one repeat
// or follow idealized feedback paths without running the audio processor.
namespace delay_preview
{
inline constexpr int bins = 128;
using Trace = std::array<double, bins>;

struct Tap
{
  double time, level, pan;
};
template<class Value>
std::array<Tap, 4> PatternTaps(Value value)
{
  std::array<Tap, 4> taps{};
  for (int i = 0; i < 4; ++i)
    taps[i] = {.01 * value(uTapTime1 + i), .01 * value(uTapLevel1 + i), .01 * value(uTapPan1 + i)};
  return taps;
}
inline double TapGain(const Tap& tap, int channel, double width)
{
  return tap.level * (1. - std::max(0., (channel ? -tap.pan : tap.pan) * width));
}

inline int DelayBin(double time, double horizon)
{
  return std::clamp(static_cast<int>(std::lround(time / horizon * (bins - 1))), 1, bins - 1);
}

// An impulse through the four nominal tap delays, with the first arrivals removed
// from the drawn trace. Binning is for display, and omits filters, motion and drive.
inline std::array<Trace, 2> PatternReturns(const std::array<Tap, 4>& taps, int mode, double feedback, double width)
{
  std::array<Trace, 4> written{};
  std::array<Trace, 2> output{};
  std::array<int, 4> time{};
  double sum = 0., power = 0.;
  for (int tap = 0; tap < 4; ++tap)
  {
    time[tap] = DelayBin(taps[tap].time, 3.);
    written[tap][0] = 1.;
    sum += taps[tap].level;
    power += taps[tap].level * taps[tap].level;
  }
  const double norm = 1. / std::sqrt(std::max(1., power));
  for (int bin = 1; bin < bins; ++bin)
  {
    std::array<double, 4> reads{};
    double swell = 0., total = 0.;
    for (int tap = 0; tap < 4; ++tap)
    {
      const int source = bin - time[tap];
      if (source >= 0) reads[tap] = written[tap][source];
      swell += reads[tap] * taps[tap].level;
      total += reads[tap];
      if (source > 0)
        for (int ch = 0; ch < 2; ++ch)
          output[ch][bin] += reads[tap] * TapGain(taps[tap], ch, width) * norm;
    }
    for (int tap = 0; tap < 4; ++tap)
    {
      const double returned = mode == 0 ? reads[3] : mode == 2 ? .5 * total - reads[tap] : swell / std::max(1., sum);
      written[tap][bin] = feedback * returned;
    }
  }
  return output;
}

// Space Tape Echo's arrivals split by the head that plays them. Every enabled head
// also plays the earlier heads' feedback, so one head's repeats can land where
// another head sits.
inline std::array<Trace, 3> TapeRepeatsByHead(int mode, double feedback, double time, double horizon)
{
  std::array<Trace, 3> wet{};
  Trace send{}, total{};
  std::array<int, 3> delay{};
  for (int head = 0; head < 3; ++head) delay[head] = DelayBin(time * HeadFraction(head), horizon);
  const int count = std::popcount(spaceHeads[mode]);
  if (count == 0) return wet; // Spring only.
  const double headGain = 1. / std::sqrt(static_cast<double>(count));
  send[0] = 1.;
  for (int bin = 1; bin < bins; ++bin)
  {
    for (int head = 0; head < 3; ++head)
      if ((spaceHeads[mode] & (1u << head)) && bin >= delay[head])
      {
        const double arrival = headGain * send[bin - delay[head]];
        wet[head][bin] += arrival;
        total[bin] += arrival;
      }
    send[bin] = feedback * headGain * total[bin];
  }
  return wet;
}
inline Trace TapeRepeats(int mode, double feedback, double time, double horizon)
{
  const auto heads = TapeRepeatsByHead(mode, feedback, time, horizon);
  Trace wet{};
  for (int bin = 0; bin < bins; ++bin) wet[bin] = heads[0][bin] + heads[1][bin] + heads[2][bin];
  return wet;
}

inline double BBDBandwidth(int mode, double ms, double highCut)
{
  constexpr std::array widths{.70, 1., .96, 1.5, .84};
  return std::min(highCut, 4300. * std::clamp(std::sqrt(330. / ms), .38, 1.65) * widths[mode]);
}

// Bucket Brigade repeats over time. Each pass runs through the clock-limited band twice
// (input and output filters), so the share above 2 kHz shrinks with every repeat and
// with longer Times. Companding and saturation are left out.
struct BBDRepeat { double seconds = 0., level = 0., treble = 0.; };
struct BBDRepeats { std::array<BBDRepeat, 12> repeats{}; int count = 0; double cutoff = 0.; };
inline BBDRepeats BBDRepeatTrain(int mode, double ms, double highCut, double feedback, double gain, double rate, double horizon)
{
  BBDRepeats result;
  result.cutoff = BBDBandwidth(mode, ms, highCut);
  const double treble = effect_preview::LowPass(2000., result.cutoff, rate, 2, .45);
  double level = gain, remaining = treble;
  for (int pass = 1; pass <= 12 && level > 1.e-5 && pass * ms * .001 <= horizon; ++pass)
  {
    result.repeats[result.count++] = {pass * ms * .001, level, remaining};
    level *= feedback;
    remaining *= treble;
  }
  return result;
}

inline double Decibels(double gain) { return 20. * std::log10(std::max(gain, 1.e-6)); }

// Granular Delay: where one played note comes back. Follows the module's grain
// schedule (two Hann grains, a new one every half length, the same random sequence,
// Glitch latches and pitch), then feeds each returned fragment back for later passes.
struct GrainFragment { double start = 0., end = 0., level = 0.; int pass = 0; bool latched = false; };
struct GrainPicture
{
  std::array<GrainFragment, 96> fragments{};
  int count = 0, firstPass = 0;
  double first = 0., last = 0., hop = 0., length = 0.;
};
inline GrainPicture GrainFragments(int mode, double timeMs, double grainMs, double density, double scatter,
                                   double interval, double feedback, double horizon, double note = .2)
{
  GrainPicture picture;
  const double time = timeMs * .001, speed = std::exp2(interval / 12.);
  const bool reverse = mode == 1, glitch = mode == 2;
  picture.length = std::max(.001, grainMs * .001 * (1.25 - .75 * density));
  picture.hop = .5 * picture.length;
  const double length = picture.length, step = reverse ? 1. + speed : 1. - speed;
  const double earliest = reverse ? 0. : std::max(0., -step * length), jitter = scatter * std::min(time, .42);
  const double slope = 1. - step; // Source seconds per output second: +speed or -speed.
  uint32_t seed = 0xc283ee81u;
  const auto random = [&] { seed ^= seed << 13; seed ^= seed >> 17; seed ^= seed << 5; return seed * (1. / 4294967295.); };
  std::array<GrainFragment, 97> sources{};
  sources[0] = {0., note, 1., 0, false};
  int sourceCount = 1, latch = 0;
  double captured = 0.;
  picture.first = horizon;
  for (double launch = 0.; launch < horizon && picture.count < static_cast<int>(picture.fragments.size()); launch += picture.hop)
  {
    double age = std::max(earliest, time + (random() - .5) * jitter - step * length * .5);
    bool latched = false;
    if (glitch)
    {
      if (latch == 0 && random() < .2 + .65 * scatter) { captured = launch - age; latch = 2 + static_cast<int>(5. * scatter); }
      if (latch > 0) { age = std::max(earliest, launch - captured); --latch; latched = true; }
    }
    if (reverse || scatter > 0.) random(); // The stereo offset draw keeps the sequence aligned.
    const double origin = launch - age; // Source time read at the grain's start.
    for (int i = 0; i < sourceCount && picture.count < static_cast<int>(picture.fragments.size()); ++i)
    {
      const auto source = sources[i];
      if (source.pass >= 3 || source.start >= launch) continue;
      double a = (source.start - origin) / slope, b = (source.end - origin) / slope;
      if (a > b) std::swap(a, b);
      a = std::clamp(a, 0., length); b = std::clamp(b, 0., length);
      if (b - a < 1.e-4) continue;
      // Loudest Hann weight inside the overlap.
      const double centre = std::clamp(.5 * length, a, b);
      const double window = .5 - .5 * std::cos(2. * 3.14159265358979323846 * centre / length);
      const double level = source.level * (source.pass == 0 ? 1. : feedback) * window;
      if (level < .02) continue;
      const GrainFragment fragment{launch + a, launch + b, level, source.pass + 1, latched};
      picture.fragments[picture.count++] = fragment;
      if (sourceCount < static_cast<int>(sources.size())) sources[sourceCount++] = fragment;
      if (fragment.pass == 1)
      {
        ++picture.firstPass;
        picture.first = std::min(picture.first, fragment.start);
        picture.last = std::max(picture.last, fragment.end);
      }
    }
  }
  return picture;
}


struct PitchPass
{
  static constexpr int count = 4;
  int repeats;
  std::array<double, count + 1> weights{};
};

inline std::array<PitchPass, PitchPass::count> PitchVoices(double shiftBlend, double feedback)
{
  std::array<PitchPass, PitchPass::count> passes{};
  PitchPass current{};
  current.weights[0] = 1.;
  for (auto& next : passes)
  {
    next.repeats = current.repeats + 1;
    for (int shifted = 0; shifted <= current.repeats; ++shifted)
    {
      const double gain = current.weights[shifted] * (current.repeats == 0 ? 1. : feedback);
      next.weights[shifted] += gain * (1. - shiftBlend);
      next.weights[shifted + 1] += gain * shiftBlend;
    }
    current = next;
  }
  return passes;
}

inline std::complex<double> OnePole(double cutoff, double rate, std::complex<double> z)
{
  const double a = std::exp(-2. * grid_fx::pi * std::min(cutoff, .45 * rate) / rate);
  return (1. - a) / (1. - a * z);
}

inline double FilterEffect(int mode, double frequency, double rate, double center, double depth, double resonance)
{
  const auto z = std::polar(1., -2. * grid_fx::pi * frequency / rate);
  if (mode == 0)
  {
    const auto band = OnePole(center * 1.8, rate, z) - OnePole(center * .57, rate, z);
    return std::abs((1. - depth) + depth * (1.15 + .85 * resonance) * band);
  }
  const double g = std::tan(grid_fx::pi * std::min(.4 * rate, center) / rate), a = (1. - g) / (1. + g);
  const auto phase = (z - a) / (1. - a * z);
  const double blend = depth * (.5 + .5 * resonance);
  return std::abs((1. - blend) + blend * phase * phase * phase * phase);
}

} // namespace delay_preview
