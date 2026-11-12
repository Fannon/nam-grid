#pragma once
#include "Antiderivative.h"
#include "MinimumPhaseOversampling.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <vector>
#if defined(__aarch64__) && !defined(NAM_PEDAL_DRIVE_SCALAR)
  #include <arm_neon.h>
#endif

// Original MIT DSP for NAM Grid. Design references (no circuit-model code copied):
// Aion Stratus, Helios, Refractor and Halo schematics; Darkglass B3K/X Ultra
// manuals; Keeley Katana, BOSS BD-2 and Analog Man Beano descriptions; Aion
// Cerulean and Solaris schematics, the MXR Timmy manual, published Tone Bender
// MkII and Tycobrahe Octavia schematics, and descriptions of starved-bias gated
// fuzzes and flip-flop octave dividers.
// See docs/modules/pedal-drive.md and ThirdPartyNotices.txt. These are inspired voices,
// not calibrated emulations of the named pedals.
namespace grid_pedal
{
inline constexpr double pi = 3.14159265358979323846;
inline double Flush(double x)
{ return std::abs(x) < 1.e-30 ? 0. : x; }

// Preserve the pedal's fixed 4x processing and coefficient design.
using Oversampler4x = grid_oversampling::FIR<4>;

struct Coefficients
{
  enum Pole
  {
    Sub,
    Bass,
    Body,
    LowerMid,
    Mid,
    UpperMid,
    Attack,
    SoftTop,
    FuzzTop,
    PreampTop,
    RodentTop,
    Count
  };
  std::array<double, Count> poles{};
  double dc = 0., slew = 0., envelope = 0., peak = 0.;
  void Reset(double rate)
  {
    constexpr std::array<double, Count> hz{35., 80., 250., 300., 720., 1400., 1700., 4200., 5000., 7000., 9000.};
    for (int i = 0; i < Count; ++i)
      poles[i] = std::exp(-2. * pi * std::min(hz[i], .4 * rate) / rate);
    dc = std::exp(-2. * pi * 5. / rate);
    slew = 35000. / rate;
    envelope = std::exp(-1. / (.005 * rate));
    peak = std::exp(-1. / (.03 * rate));
  }
};
// Every clipping stage except Rodent's is averaged over the line between adjacent
// oversampled inputs (first-order antiderivative antialiasing), so the curves need
// antiderivatives that cost less than the exp and log of tanh's.
// Soft, x / sqrt(1 + x^2), is the curve of the fuzzes added after 0.1.
inline double Soft(double x) { return x / std::sqrt(1. + x * x); }
inline double SoftIntegral(double x) { return std::sqrt(1. + x * x) - 1.; }
// Warm stands in for the tanh of the voices released in 0.1: 57% Soft at 1.2 times the
// drive plus 43% of a cubic soft clip. It has unity slope at zero, saturates at +/-1 and
// stays within 0.013 of tanh everywhere, so those voices keep their drive and level
// calibration. Its antiderivative is one square root and a polynomial.
namespace warm
{
inline constexpr double share = .5703, rate = 1.1958;
inline constexpr double knee = 1.5 * (1. - share / rate) / (1. - share); // unity asymptote
} // namespace warm
inline double Warm(double x)
{
  const double u = std::clamp(x, -warm::knee, warm::knee);
  return warm::share * x / std::sqrt(1. + warm::rate * warm::rate * x * x)
         + (1. - warm::share) * (u - u * u * u / (3. * warm::knee * warm::knee));
}
inline double WarmIntegral(double x)
{
  // Branch-free: the cubic part is integrated up to the knee and continues as a line.
  constexpr double softScale = warm::share / (warm::rate * warm::rate), quartic = 1. / (12. * warm::knee * warm::knee);
  const double m = std::abs(x), u = std::min(m, warm::knee), u2 = u * u;
  return softScale * (std::sqrt(1. + warm::rate * warm::rate * x * x) - 1.)
         + (1. - warm::share) * (u2 * (.5 - quartic * u2) + (2. * warm::knee / 3.) * (m - u));
}
inline double WarmSlope(double x)
{
  const double u = std::min(std::abs(x), warm::knee) / warm::knee;
  return warm::share * std::pow(1. + warm::rate * warm::rate * x * x, -1.5) + (1. - warm::share) * (1. - u * u);
}
// The antiderivative must be cheap: it is evaluated for every changing sample. The
// first sample starts at its own input, so a biased stage is silent from a reset,
// and an unchanged input (silence) reuses its value without evaluating anything.
struct Averaged
{
  double previous = 0., integral = 0., held = 0.;
  bool primed = false, holding = false;
  template <class Curve, class Integral>
  double Tick(double x, Curve&& curve, Integral&& primitive)
  {
    if (x == previous && primed)
    {
      if (!holding) { held = curve(x); holding = true; }
      return held;
    }
    holding = false;
    const double current = primitive(x);
    if (!primed) { primed = true; previous = x; integral = current; }
    const double delta = x - previous;
    const double scale = std::max(1., std::max(std::abs(x), std::abs(previous)));
    const double y = std::abs(delta) > 1.e-5 * scale ? (current - integral) / delta : curve(.5 * (x + previous));
    previous = x;
    integral = current;
    return y;
  }
  double Soft(double x) { return Tick(x, grid_pedal::Soft, SoftIntegral); }
  double Warm(double x) { return Tick(x, grid_pedal::Warm, WarmIntegral); }
};

// An asymmetric Warm stage: the bias moves the operating point, and the output is
// re-centred and scaled back to unity small-signal gain. That normalization is
// updated with the smoothed character control, not for every oversampled sample.
struct BiasedStage
{
  double bias = 0., offset = 0., slope = 1.;
  BiasedStage() = default;
  explicit BiasedStage(double value) { Reset(value); }
  void Reset(double value)
  {
    bias = value;
    offset = Warm(value);
    slope = WarmSlope(value);
  }
  double Tick(Averaged& stage, double x) const { return (stage.Warm(x + bias) - offset) / slope; }
};
struct Character
{
  double value = 0., screamerBass = .8, rodentBite = 1.4, slewMultiplier = 1., bassAttack = .9, scoop = .65;
  double odBlend = 0., hardBlend = 0., bluesHeadroom = 1., transparentHeadroom = 1.2, gateThreshold = 0.;
  double octave = .85, benderGate = 0., gateOpen = .3, gateClose = .48, sub = .6;
  double benderBias = .45, benderOffset = 0., benderSlope = 1.;
  BiasedStage preampFirst, preampSecond, transparent, vintage;
  // Fixed biases: Bass Edge's two stages and Treble Drive.
  BiasedStage edgeFirst{.12}, edgeSecond{-.06}, treble{.2};
  void Reset(double control)
  {
    value = std::clamp(control, -1., 1.);
    screamerBass = .8 + (value < 0. ? .4 : .18) * value;
    rodentBite = 1.4 * (1. + .5 * value);
    slewMultiplier = 1. + .6 * value;
    bassAttack = .9 + .6 * value;
    scoop = .65 * (1. - value);
    odBlend = .45 * value;
    hardBlend = std::max(0., value);
    bluesHeadroom = 1. - .5 * std::min(0., value);
    transparentHeadroom = 1.2 + .65 * std::min(0., value);
    gateThreshold = .015 * std::max(0., value);
    preampFirst.Reset(.1 * (1. + value));
    preampSecond.Reset(-.08 * (1. + value));
    transparent.Reset(.22 * std::max(0., value));
    vintage.Reset(.28 + .25 * value);
    // Soft's bias, its output at rest and its small-signal slope there.
    benderBias = .45 + .25 * value;
    benderOffset = Soft(benderBias);
    benderSlope = std::pow(1. + benderBias * benderBias, -1.5);
    benderGate = .006 * std::max(0., value);
    octave = std::clamp(.85 + (value < 0. ? .5 : .15) * value, 0., 1.);
    gateOpen = .3 + (value < 0. ? .25 : .4) * value;
    gateClose = 1.6 * gateOpen;
    sub = std::clamp(.6 + (value < 0. ? .5 : .4) * value, 0., 1.);
  }
};

// Each voice keeps its own filters during style crossfades. gain is linear,
// rootGain is its cached square root. amount is Drive / 36 dB, used only by
// Parallel OD's internal dual-gang blend.
struct Voice
{
  std::array<double, Coefficients::Count> low{};
  // slew is the legacy Rodent state, used only by the offline comparison path
  // (antialiasRodent == false); the shipped path keeps its state in rodentSlew.
  double slew = 0., envelope = 0.;
  grid_adaa::Average rodentInput;
  grid_adaa::SlewClip rodentSlew;
  // Up to three averaged nonlinear stages per voice.
  Averaged fuzzFirst, fuzzSecond, fuzzRectifier;
  // Sub Fuzz's octave divider.
  double peakHigh = 0., peakLow = 0., tracked = 0., polarity = 1.;
  bool high = false, pending = false;
  double Tick(double x, int style, double gain, double rootGain, double amount, const Character& character,
              const Coefficients& c, bool antialiasRodent = true)
  {
    using P = Coefficients;
    const auto lp = [&](double input, int pole) {
      return low[pole] = Flush(input + c.poles[pole] * (low[pole] - input));
    };
    switch (style)
    {
      case 0: // Mid-focused soft feedback-clip character, with a small clean feed.
        return lp(.15 * x + .85 * fuzzFirst.Warm(2. * gain * (x - character.screamerBass * lp(x, P::Mid))), P::SoftTop);
      case 1: // Shaped op-amp gain, limited slew, then harder diode-like clipping.
      {
        const double high = x - lp(x, P::Bass);
        const double voiced = high + character.rodentBite * (high - lp(high, P::UpperMid));
        const double input = voiced * gain * 1.6 / 3.;
        const double driven = 3. * (antialiasRodent ? rodentInput.Tanh(input) : std::tanh(input));
        if (antialiasRodent) return lp(rodentSlew.Tick(driven, c.slew * character.slewMultiplier, .8), P::RodentTop);
        slew = Flush(
          slew + std::clamp(driven - slew, -c.slew * character.slewMultiplier, c.slew * character.slewMultiplier));
        return lp(std::clamp(slew, -.8, .8), P::RodentTop);
      }
      case 2: // Defined upper mids with asymmetric two-stage bass drive.
      {
        const double tight = x - .65 * lp(x, P::Bass);
        const double bite = tight + character.bassAttack * (tight - lp(tight, P::Attack));
        const double first = character.edgeFirst.Tick(fuzzFirst, 2.4 * gain * bite);
        return lp(.85 * character.edgeSecond.Tick(fuzzSecond, 1.2 * first), P::PreampTop);
      }
      case 3: // Internal clean/clipped blend is distinct from the outer Mix.
      {
        const double clipped = .9 * fuzzFirst.Warm(3. * gain * (x - .65 * lp(x, P::LowerMid)));
        const double blend = std::clamp(amount + character.odBlend, 0., 1.);
        return lp((1. - blend) * x + blend * clipped, P::PreampTop);
      }
      case 4: // Cascaded clipping, coupling loss and a broad passive-style scoop.
      {
        const double tight = x - lp(x, P::Sub);
        const double first = lp(fuzzFirst.Warm(3. * rootGain * tight), P::PreampTop);
        const double second = lp(fuzzSecond.Warm(2.8 * rootGain * first), P::FuzzTop);
        return .9 * (second - character.scoop * (lp(second, P::UpperMid) - lp(second, P::LowerMid)));
      }
      case 5: // Mild, asymmetric preamp stages with interstage coupling/filtering.
      {
        const double first = lp(character.preampFirst.Tick(fuzzFirst, 1.1 * rootGain * x), P::PreampTop);
        const double coupled = first - lp(first, P::Sub);
        return .9 * character.preampSecond.Tick(fuzzSecond, 1.2 * rootGain * coupled);
      }
      case 6: // Colored treble booster; Focus changes the pre-drive bass shaping.
      {
        const double mid = lp(x, P::Mid), upper = lp(x, P::Attack);
        const double focused =
          character.value <= 0. ? x - (1. + .7 * character.value) * mid : (x - mid) + character.value * (mid - upper);
        return lp(character.treble.Tick(fuzzFirst, 1.7 * gain * focused), P::PreampTop);
      }
      case 7: // Bluesbreaker family: bandwidth-limited gain into feedback-style
              // soft clipping with a clean contribution, then optional hard clipping.
      {
        const double tight = x - .35 * lp(x, P::Bass);
        const double first = lp(.65 * rootGain * tight, P::RodentTop);
        const double soft =
          .12 * first + .88 * character.bluesHeadroom * fuzzFirst.Warm(1.1 * rootGain * first / character.bluesHeadroom);
        const double hard = fuzzSecond.Tick(soft, [](double v) { return std::clamp(v, -.4, .4); },
                                            [](double v) { const double m = std::abs(v); return m <= .4 ? .5 * v * v : .4 * m - .08; });
        return lp(soft + character.hardBlend * (hard - soft), P::PreampTop);
      }
      case 8: // Timmy family: restrained bass shaping before clipping, treble
              // filtering after it, with compressed/open/asymmetric clip character.
      {
        const double voiced = x - .4 * lp(x, P::Bass);
        const double driven = (1. + .6 * (gain - 1.)) * voiced;
        const double clipped =
          character.transparentHeadroom * character.transparent.Tick(fuzzFirst, driven / character.transparentHeadroom);
        return lp(.1 * voiced + .9 * clipped, P::RodentTop);
      }
      case 9: // Fuzz Face family: two inverting stages, interstage coupling
              // and a bias-dependent pinched decay. No analog pickup loading model.
      {
        const double tight = x - .25 * lp(x, P::Sub);
        const double first = -.6 * fuzzFirst.Warm(1.8 * tight);
        const double coupled = first - .7 * lp(first, P::Bass);
        envelope = Flush(std::abs(coupled) + c.envelope * (envelope - std::abs(coupled)));
        const double gate =
          character.gateThreshold == 0. ? 1. : std::clamp((envelope - character.gateThreshold) * 100., 0., 1.);
        return lp(.9 * character.vintage.Tick(fuzzSecond, -2.8 * gain * coupled) * gate, P::RodentTop);
      }
      case 10: // Tone Bender family: bass-cut first stage, coupling, then a high-gain
               // asymmetric pair. More gain and mids than case 9; Bias adds sputter.
      {
        const double tight = x - lp(x, P::Body);
        const double first = -.5 * fuzzFirst.Soft(3. * rootGain * tight);
        const double coupled = lp(first - lp(first, P::LowerMid), P::UpperMid);
        envelope = Flush(std::abs(coupled) + c.envelope * (envelope - std::abs(coupled)));
        const double gate =
          character.benderGate == 0. ? 1. : std::clamp((envelope - character.benderGate) * 200., 0., 1.);
        const double second =
          (fuzzSecond.Soft(-9.6 * rootGain * coupled + character.benderBias) - character.benderOffset) / character.benderSlope;
        return lp(.7 * second * gate, P::PreampTop);
      }
      case 11: // Octavia family: a milder gain stage, full-wave rectification
               // that doubles the fundamental, then fuzz.
      {
        const double tight = x - lp(x, P::Body);
        const double pre = fuzzFirst.Soft(1.5 * gain * tight);
        // Full-wave rectifier, averaged like the clipping stages: |v| integrates to
        // v |v| / 2, so its corner is resolved exactly without a transcendental.
        const double rectified = fuzzRectifier.Tick(pre, [](double v) { return std::abs(v); },
                                                    [](double v) { return .5 * v * std::abs(v); });
        const double octave = rectified - lp(rectified, P::Sub);
        const double blended = character.octave * 2. * octave + (1. - character.octave) * pre;
        return lp(.85 * fuzzSecond.Soft(1.8 * rootGain * blended), P::RodentTop);
      }
      case 12: // Starved, misbiased fuzz: the second stage conducts only above a
               // threshold, sooner on one polarity. Loud notes are a full fuzz; as a note
               // decays its pulses narrow, sputter and cut off. Silence stays silent.
      {
        const double tight = x - lp(x, P::Bass);
        const double first = fuzzFirst.Soft(4. * gain * tight);
        const double coupled = first - lp(first, P::Sub);
        constexpr double steep = 24.;
        const double open = character.gateOpen, close = character.gateClose;
        const double starved = fuzzSecond.Tick(coupled,
          [=](double v) { return v > open ? Soft(steep * (v - open)) : v < -close ? Soft(steep * (v + close)) : 0.; },
          [=](double v) { return v > open ? SoftIntegral(steep * (v - open)) / steep : v < -close ? SoftIntegral(steep * (v + close)) / steep : 0.; });
        return lp(.8 * starved, P::RodentTop);
      }
      case 13: // Octave divider into fuzz. A peak-relative trigger finds one event per
               // cycle of the low-passed input; the following zero crossing flips the
               // polarity, so every other cycle is inverted and the result repeats at
               // half the pitch without a step. Single notes track; chords glitch.
      {
        const double body = lp(lp(x, P::LowerMid), P::Mid);
        peakHigh = std::max(body, Flush(peakHigh * c.peak));
        peakLow = std::min(body, Flush(peakLow * c.peak));
        if (!high && body > .6 * peakHigh && body > 1.e-5) { high = true; pending = true; }
        else if (high && body < .6 * peakLow) high = false;
        if (pending && body <= 0. && tracked > 0.) { polarity = -polarity; pending = false; }
        tracked = body;
        const double tight = x - lp(x, P::Bass);
        const double blended = character.sub * 2.5 * polarity * body + (1. - character.sub) * tight;
        return lp(.85 * fuzzFirst.Soft(3. * gain * blended), P::PreampTop);
      }
      default: return x;
    }
  }
};
} // namespace grid_pedal
