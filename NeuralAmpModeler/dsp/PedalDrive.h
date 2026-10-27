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
// Cerulean and Solaris schematics, and the MXR Timmy manual.
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
  double dc = 0., slew = 0., envelope = 0.;
  void Reset(double rate)
  {
    constexpr std::array<double, Count> hz{35., 80., 250., 300., 720., 1400., 1700., 4200., 5000., 7000., 9000.};
    for (int i = 0; i < Count; ++i)
      poles[i] = std::exp(-2. * pi * std::min(hz[i], .4 * rate) / rate);
    dc = std::exp(-2. * pi * 5. / rate);
    slew = 35000. / rate;
    envelope = std::exp(-1. / (.005 * rate));
  }
};
inline double Biased(double x, double bias)
{
  const double offset = std::tanh(bias);
  return (std::tanh(x + bias) - offset) / (1. - offset * offset);
}

// Bias-dependent normalization is updated with the smoothed character control,
// rather than evaluating the bias tanh for every oversampled audio sample.
struct BiasedStage
{
  double bias = 0., offset = 0., denominator = 1.;
  void Reset(double value)
  {
    bias = value;
    offset = std::tanh(value);
    denominator = 1. - offset * offset;
  }
  double Tick(double x) const { return (std::tanh(x + bias) - offset) / denominator; }
};
struct Character
{
  double value = 0., screamerBass = .8, rodentBite = 1.4, slewMultiplier = 1., bassAttack = .9, scoop = .65;
  double odBlend = 0., hardBlend = 0., bluesHeadroom = 1., transparentHeadroom = 1.2, gateThreshold = 0.;
  BiasedStage preampFirst, preampSecond, transparent, vintage;
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
  }
};

// Each voice keeps its own filters during style crossfades. gain is linear,
// rootGain is its cached square root. amount is Drive / 36 dB, used only by
// Parallel OD's internal dual-gang blend.
struct Voice
{
  std::array<double, Coefficients::Count> low{};
  double slew = 0., envelope = 0.;
  grid_adaa::Average rodentInput;
  grid_adaa::SlewClip rodentSlew;
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
        return lp(.15 * x + .85 * std::tanh(2. * gain * (x - character.screamerBass * lp(x, P::Mid))), P::SoftTop);
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
        return lp(.85 * Biased(1.2 * Biased(2.4 * gain * bite, .12), -.06), P::PreampTop);
      }
      case 3: // Internal clean/clipped blend is distinct from the outer Mix.
      {
        const double clipped = .9 * std::tanh(3. * gain * (x - .65 * lp(x, P::LowerMid)));
        const double blend = std::clamp(amount + character.odBlend, 0., 1.);
        return lp((1. - blend) * x + blend * clipped, P::PreampTop);
      }
      case 4: // Cascaded clipping, coupling loss and a broad passive-style scoop.
      {
        const double tight = x - lp(x, P::Sub);
        const double first = lp(std::tanh(3. * rootGain * tight), P::PreampTop);
        const double second = lp(std::tanh(2.8 * rootGain * first), P::FuzzTop);
        return .9 * (second - character.scoop * (lp(second, P::UpperMid) - lp(second, P::LowerMid)));
      }
      case 5: // Mild, asymmetric preamp stages with interstage coupling/filtering.
      {
        const double first = lp(character.preampFirst.Tick(1.1 * rootGain * x), P::PreampTop);
        const double coupled = first - lp(first, P::Sub);
        return .9 * character.preampSecond.Tick(1.2 * rootGain * coupled);
      }
      case 6: // Colored treble booster; Focus changes the pre-drive bass shaping.
      {
        const double mid = lp(x, P::Mid), upper = lp(x, P::Attack);
        const double focused =
          character.value <= 0. ? x - (1. + .7 * character.value) * mid : (x - mid) + character.value * (mid - upper);
        return lp(Biased(1.7 * gain * focused, .2), P::PreampTop);
      }
      case 7: // Bluesbreaker family: bandwidth-limited gain into feedback-style
              // soft clipping with a clean contribution, then optional hard clipping.
      {
        const double tight = x - .35 * lp(x, P::Bass);
        const double first = lp(.65 * rootGain * tight, P::RodentTop);
        const double soft =
          .12 * first + .88 * character.bluesHeadroom * std::tanh(1.1 * rootGain * first / character.bluesHeadroom);
        return lp(soft + character.hardBlend * (std::clamp(soft, -.4, .4) - soft), P::PreampTop);
      }
      case 8: // Timmy family: restrained bass shaping before clipping, treble
              // filtering after it, with compressed/open/asymmetric clip character.
      {
        const double voiced = x - .4 * lp(x, P::Bass);
        const double driven = (1. + .6 * (gain - 1.)) * voiced;
        const double clipped =
          character.transparentHeadroom * character.transparent.Tick(driven / character.transparentHeadroom);
        return lp(.1 * voiced + .9 * clipped, P::RodentTop);
      }
      case 9: // Fuzz Face family: two inverting stages, interstage coupling
              // and a bias-dependent pinched decay. No analog pickup loading model.
      {
        const double tight = x - .25 * lp(x, P::Sub);
        const double first = -.6 * std::tanh(1.8 * tight);
        const double coupled = first - .7 * lp(first, P::Bass);
        envelope = Flush(std::abs(coupled) + c.envelope * (envelope - std::abs(coupled)));
        const double gate =
          character.gateThreshold == 0. ? 1. : std::clamp((envelope - character.gateThreshold) * 100., 0., 1.);
        return lp(.9 * character.vintage.Tick(-2.8 * gain * coupled) * gate, P::RodentTop);
      }
      default: return x;
    }
  }
};
} // namespace grid_pedal
