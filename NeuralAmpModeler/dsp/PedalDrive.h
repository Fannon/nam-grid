#pragma once
#include <algorithm>
#include <array>
#include <cmath>

// Original MIT DSP for NAM Grid. Design references (no circuit-model code copied):
// Aion Stratus, Helios, Refractor and Halo schematics; Darkglass B3K/X Ultra
// manuals; Keeley Katana, BOSS BD-2 and Analog Man Beano descriptions.
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
    double result = 0.;
    for (int phase = 0; phase < factor; ++phase)
    {
      double up = 0.;
      for (int tap = phase, sample = 0; tap < taps; tap += factor, ++sample)
        up += mKernel[tap] * mInput[mInputIndex + sample];
      const double high = Flush(nonlinear(up * factor));
      mHighIndex = (mHighIndex - 1) & 511;
      mHigh[mHighIndex] = mHigh[mHighIndex + 512] = high;
      // Output is sampled at phase zero, matching the stated integer latency.
      if (phase == 0)
        for (int tap = 0; tap < taps; ++tap)
          result += mKernel[tap] * mHigh[mHighIndex + tap];
    }
    return Flush(result);
  }

private:
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
  double dc = 0., slew = 0.;
  void Reset(double rate)
  {
    constexpr std::array<double, Count> hz{35., 80., 250., 300., 720., 1400., 1700., 4200., 5000., 7000., 9000.};
    for (int i = 0; i < Count; ++i)
      poles[i] = std::exp(-2. * pi * std::min(hz[i], .4 * rate) / rate);
    dc = std::exp(-2. * pi * 5. / rate);
    slew = 35000. / rate;
  }
};
inline double Biased(double x, double bias)
{
  const double offset = std::tanh(bias);
  return (std::tanh(x + bias) - offset) / (1. - offset * offset);
}

// Each voice keeps its own filters during style crossfades. gain is linear;
// amount is Drive / 36 dB, used only by Parallel OD's internal dual-gang blend.
struct Voice
{
  std::array<double, Coefficients::Count> low{};
  double slew = 0.;
  double Tick(double x, int style, double gain, double amount, const Coefficients& c)
  {
    using P = Coefficients;
    const auto lp = [&](double input, int pole) {
      return low[pole] = Flush(input + c.poles[pole] * (low[pole] - input));
    };
    switch (style)
    {
      case 0: // Mid-focused soft feedback-clip character, with a small clean feed.
        return lp(.15 * x + .85 * std::tanh(2. * gain * (x - .8 * lp(x, P::Mid))), P::SoftTop);
      case 1: // Shaped op-amp gain, limited slew, then harder diode-like clipping.
      {
        const double high = x - lp(x, P::Bass);
        const double voiced = high + 1.4 * (high - lp(high, P::UpperMid));
        const double driven = 3. * std::tanh(voiced * gain * 1.6 / 3.);
        slew = Flush(slew + std::clamp(driven - slew, -c.slew, c.slew));
        return lp(std::clamp(slew, -.8, .8), P::RodentTop);
      }
      case 2: // Defined upper mids with asymmetric two-stage bass drive.
      {
        const double tight = x - .65 * lp(x, P::Bass);
        const double bite = tight + .9 * (tight - lp(tight, P::Attack));
        return lp(.85 * Biased(1.2 * Biased(2.4 * gain * bite, .12), -.06), P::PreampTop);
      }
      case 3: // Internal clean/clipped blend is distinct from the outer Mix.
      {
        const double clipped = .9 * std::tanh(3. * gain * (x - .65 * lp(x, P::LowerMid)));
        return lp((1. - amount) * x + amount * clipped, P::PreampTop);
      }
      case 4: // Cascaded clipping, coupling loss and a broad passive-style scoop.
      {
        const double tight = x - lp(x, P::Sub);
        const double first = lp(std::tanh(3. * std::sqrt(gain) * tight), P::PreampTop);
        const double second = lp(std::tanh(2.8 * std::sqrt(gain) * first), P::FuzzTop);
        return .9 * (second - .65 * (lp(second, P::UpperMid) - lp(second, P::LowerMid)));
      }
      case 5: // Mild, asymmetric preamp stages with interstage coupling/filtering.
      {
        const double first = lp(Biased(1.1 * std::sqrt(gain) * x, .1), P::PreampTop);
        const double coupled = first - lp(first, P::Sub);
        return .9 * Biased(1.2 * std::sqrt(gain) * coupled, -.08);
      }
      case 6: // Colored treble booster into a following amp or capture.
        return lp(Biased(1.7 * gain * (x - lp(x, P::Mid)), .2), P::PreampTop);
      default: return x;
    }
  }
};
} // namespace grid_pedal
