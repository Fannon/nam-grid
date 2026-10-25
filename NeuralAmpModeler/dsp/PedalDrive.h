#pragma once
#include <algorithm>
#include <array>
#include <cmath>
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

// Windowed-sinc interpolation and decimation, both at 4x. Polyphase interpolation
// skips zero-stuffed samples; mirrored rings make each dot product contiguous.
// The two 257-tap linear-phase filters add exactly 64 host samples of delay.
// Fixed storage; coefficient design and clearing only happen during preparation.
class Oversampler4x
{
public:
  static constexpr int factor = 4, taps = 257, latency = (taps - 1) / factor;
  void Reset()
  {
    mInput = {};
    mHigh = {};
    mInputIndex = mHighIndex = 0;
    double sum = 0.;
    for (int i = 0; i < taps; ++i)
    {
      const double x = i - (taps - 1) * .5;
      const double sinc = x == 0. ? .225 : std::sin(2. * pi * .1125 * x) / (pi * x);
      const double window = .42 - .5 * std::cos(2. * pi * i / (taps - 1)) + .08 * std::cos(4. * pi * i / (taps - 1));
      mKernel[i] = sinc * window;
      sum += mKernel[i];
    }
    for (double& x : mKernel)
      x /= sum;
  }
  template <class Nonlinear>
  double Tick(double input, Nonlinear&& nonlinear)
  {
    mInputIndex = (mInputIndex - 1) & 127;
    mInput[mInputIndex] = mInput[mInputIndex + 128] = input;
    // Accumulate the four interpolation phases together: each input load feeds
    // four independent sums, allowing SIMD without changing their tap order.
    std::array<double, factor> up{};
    Interpolate(up.data());
    up[0] += mKernel[taps - 1] * mInput[mInputIndex + (taps - 1) / factor];
    double result = 0.;
    for (int phase = 0; phase < factor; ++phase)
    {
      const double high = Flush(nonlinear(up[phase] * factor));
      mHighIndex = (mHighIndex - 1) & 511;
      mHigh[mHighIndex] = mHigh[mHighIndex + 512] = high;
      // Output is sampled at phase zero, matching the stated integer latency.
      if (phase == 0)
      {
        // Independent sums remove the serial 257-add dependency. The kernel
        // and phase are unchanged; only roundoff from the final reduction differs.
        std::array<double, factor> down{};
        Decimate(down.data());
        result = ((down[0] + down[1]) + (down[2] + down[3])) + mKernel[taps - 1] * mHigh[mHighIndex + taps - 1];
      }
    }
    return Flush(result);
  }

private:
  // Each phase accumulates its own taps in the scalar order the reference uses,
  // so pairing two phases in one SIMD register keeps the output bit-identical.
  // On arm64 this halves the instructions per tap of the four dot products that
  // dominate Pedal Drive's cost; other targets keep the portable loop.
  void Interpolate(double* up) const
  {
#if defined(__aarch64__) && !defined(NAM_PEDAL_DRIVE_SCALAR)
    float64x2_t low = vdupq_n_f64(0.), high = vdupq_n_f64(0.);
    const double* in = &mInput[mInputIndex];
    const double* kernel = mKernel.data();
    for (int group = 0; group < (taps - 1) / (factor * 2); ++group)
    {
      const float64x2_t first = vdupq_n_f64(in[0]), second = vdupq_n_f64(in[1]);
      low = vaddq_f64(low, vmulq_f64(vld1q_f64(kernel), first));
      high = vaddq_f64(high, vmulq_f64(vld1q_f64(kernel + 2), first));
      low = vaddq_f64(low, vmulq_f64(vld1q_f64(kernel + 4), second));
      high = vaddq_f64(high, vmulq_f64(vld1q_f64(kernel + 6), second));
      kernel += 2 * factor;
      in += 2;
    }
    vst1q_f64(up, low);
    vst1q_f64(up + 2, high);
#else
    for (int sample = 0; sample < (taps - 1) / factor; ++sample)
      for (int phase = 0; phase < factor; ++phase)
        up[phase] += mKernel[factor * sample + phase] * mInput[mInputIndex + sample];
#endif
  }
  void Decimate(double* down) const
  {
#if defined(__aarch64__) && !defined(NAM_PEDAL_DRIVE_SCALAR)
    float64x2_t low = vdupq_n_f64(0.), high = vdupq_n_f64(0.);
    const double* ring = &mHigh[mHighIndex];
    const double* kernel = mKernel.data();
    for (int group = 0; group < (taps - 1) / (factor * 2); ++group)
    {
      low = vaddq_f64(low, vmulq_f64(vld1q_f64(kernel), vld1q_f64(ring)));
      high = vaddq_f64(high, vmulq_f64(vld1q_f64(kernel + 2), vld1q_f64(ring + 2)));
      low = vaddq_f64(low, vmulq_f64(vld1q_f64(kernel + 4), vld1q_f64(ring + 4)));
      high = vaddq_f64(high, vmulq_f64(vld1q_f64(kernel + 6), vld1q_f64(ring + 6)));
      kernel += 2 * factor;
      ring += 2 * factor;
    }
    vst1q_f64(down, low);
    vst1q_f64(down + 2, high);
#else
    for (int tap = 0; tap < taps - 1; tap += factor)
      for (int j = 0; j < factor; ++j)
        down[j] += mKernel[tap + j] * mHigh[mHighIndex + tap + j];
#endif
  }

  std::array<double, taps> mKernel{};
  std::array<double, 256> mInput{};
  std::array<double, 1024> mHigh{};
  int mInputIndex = 0, mHighIndex = 0;
};

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
  double Tick(double x, int style, double gain, double rootGain, double amount, const Character& character,
              const Coefficients& c)
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
        const double driven = 3. * std::tanh(voiced * gain * 1.6 / 3.);
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
