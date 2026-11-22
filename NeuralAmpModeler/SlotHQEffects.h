#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <vector>
#include "SlotTimeEffects.h"

// Original high-quality grid effects. The algorithms are informed by the MIT
// Shimwash (shimmer reverb), Junologue Chorus (Juno BBD ensemble) and Ruminant
// (frequency-shifting delay) projects, but this is a rate-scaled, double
// precision implementation with NAM Grid's control smoothing and lifecycle.
// Reference revisions consulted (MIT at each revision; see ThirdPartyNotices.txt):
//   twinpowers/schwung-shimwash             a27d7a215d9cb112d27c5eeb34d31c9e3304087f
//   charlesvestal/schwung-junologue-chorus  0b1ecd9d09daad91b5726899bd5e4cfe3f97cf06
//   mestela/schwung-ruminant                d6cdedcc1febc426bb3a06eeca74b768198f446c
namespace grid_hq
{
inline double SoftLimit(double x)
{
  const double x2 = x * x;
  return x * (27. + x2) / (27. + 9. * x2);
}
inline double Tri(double phase)
{ return phase < .5 ? phase * 2. : 2. - phase * 2.; }

// Vintage I+II: Juno-60 recordings measured at 9.75 Hz and 3.3-3.7 ms:
// https://github.com/pendragon-andyh/Juno60/blob/master/Chorus/README.md
// Sine-like fast LFO, with the second BBD no longer inverted:
// https://www.florian-anwander.de/roland_string_choruses/
// The pinned Junologue reference above instead blends I and II; retained as mode 1.
class JunoChorus
{
public:
  void Reset(double rate)
  {
    mRate = std::max(1., rate);
    mLine.Reset(static_cast<int>(mRate * .012) + 8);
    mControls.Reset(mRate);
    mPre = mPost[0] = mPost[1] = 0.;
    mPhase[0] = mPhase[1] = 0.;
    mVintagePhase = mVintageGain = 0.;
    mWidth.Reset(rate);
    mGainA = .70710678;
    mGainB = .70710678;
    mMixSmoothing = std::exp(-1. / (.02 * mRate));
    mFirst = true;
  }
  void Process(double* l, double* r, int n, const UtilityControls& c)
  {
    mControls.Target(c.juno, uJunoMode);
    mWidth.Target({c.extra.junoWidth}, uJunoWidth);
    const double gainCoefficient = std::exp(-1. / (.01 * mRate));
    // Smooth the equal-power gains themselves: sqrt of a smoothed mix rising from 0%
    // has an infinite initial slope, so the wet voice jumped in.
    const double mixTarget = std::clamp(c.juno[1], kEffectParameters[uJunoMix].minimum, kEffectParameters[uJunoMix].maximum) * .01;
    const double dryTarget = std::sqrt(std::max(0., 1. - mixTarget)), wetTarget = std::sqrt(mixTarget);
    for (int i = 0; i < n; ++i)
    {
      const auto& v = mControls.Tick();
      const int mode = std::clamp(static_cast<int>(std::lround(v[0])), 0, 3);
      constexpr double modeGains[4][2]{{1., 0.}, {.70710678, .70710678}, {0., 1.}, {0., 0.}};
      const double vintageTarget = mode == 3 ? 1. : 0.;
      if (mFirst)
      {
        mVintageGain = vintageTarget;
        mGainA = modeGains[mode][0];
        mGainB = modeGains[mode][1];
        mDryGain = dryTarget;
        mWetGain = wetTarget;
        mFirst = false;
      }
      mGainA = modeGains[mode][0] + gainCoefficient * (mGainA - modeGains[mode][0]);
      mGainB = modeGains[mode][1] + gainCoefficient * (mGainB - modeGains[mode][1]);
      mVintageGain = vintageTarget + gainCoefficient * (mVintageGain - vintageTarget);
      if (std::abs(mVintageGain - vintageTarget) < 1.e-12) mVintageGain = vintageTarget;
      const double brightness = v[2] * .01;
      const double preCutoff = std::min(.45 * mRate, 2000. + brightness * brightness * 18000.);
      const double postCutoff = std::min(.45 * mRate, 6000. + brightness * brightness * 14000.);
      const double preCoefficient =
        mPreCoefficient(-2. * grid_fx::pi * preCutoff / mRate, [](double x) { return std::exp(x); });
      const double postCoefficient =
        mPostCoefficient(-2. * grid_fx::pi * postCutoff / mRate, [](double x) { return std::exp(x); });
      const double dryL = l[i], dryR = r ? r[i] : dryL;
      const double mono = SoftLimit(.5 * (dryL + dryR));
      mPre = grid_fx::Flush(mono + preCoefficient * (mPre - mono));
      mLine.Push(mPre);
      mPhase[0] += .513 / mRate;
      mPhase[1] += .863 / mRate;
      if (mPhase[0] >= 1.)
        mPhase[0] -= 1.;
      if (mPhase[1] >= 1.)
        mPhase[1] -= 1.;
      constexpr double minimum = .00166, range = .00369;
      const double tap1L = mLine.Read(mRate * (minimum + range * Tri(mPhase[0])));
      const double tap1R = mLine.Read(mRate * (minimum + range * (1. - Tri(mPhase[0]))));
      const double tap2L = mLine.Read(mRate * (minimum + range * Tri(mPhase[1])));
      const double tap2R = mLine.Read(mRate * (minimum + range * (1. - Tri(mPhase[1]))));
      double wetL = tap1L * mGainA + tap2L * mGainB;
      double wetR = tap1R * mGainA + tap2R * mGainB;
      // Crossfade a separate tap; changing mode never jumps a running delay.
      // Skip the extra oscillator/read entirely for rigs using the original modes.
      if (mVintageGain != 0.)
      {
        mVintagePhase += 9.75 / mRate;
        if (mVintagePhase >= 1.) mVintagePhase -= 1.;
        const double delay = .0035 + .0002 * std::sin(2. * grid_fx::pi * mVintagePhase);
        const double vintage = mLine.Read(mRate * delay) * mVintageGain;
        wetL += vintage;
        wetR += vintage;
      }
      mPost[0] = grid_fx::Flush(wetL + postCoefficient * (mPost[0] - wetL));
      mPost[1] = grid_fx::Flush(wetR + postCoefficient * (mPost[1] - wetR));
      wetL = mPost[0];
      wetR = mPost[1];
      const double width = mWidth.Tick()[0] * .01;
      if (width != 1.)
      {
        const double mid = .5 * (wetL + wetR), side = .5 * (wetL - wetR) * width;
        wetL = mid + side; wetR = mid - side;
      }
      // Snap the last rounding step so Mix 0% stays exactly dry after automation.
      const auto approach = [&](double gain, double target) {
        const double next = target + mMixSmoothing * (gain - target);
        return std::abs(next - target) < 1.e-12 ? target : next;
      };
      mDryGain = approach(mDryGain, dryTarget);
      mWetGain = approach(mWetGain, wetTarget);
      l[i] = dryL * mDryGain + wetL * mWetGain;
      if (r)
        r[i] = dryR * mDryGain + wetR * mWetGain;
    }
  }

private:
  grid_fx::DelayLine mLine;
  grid_fx::SmoothControls<3> mControls;
  grid_fx::SmoothControls<1> mWidth;
  grid_fx::Memo mPreCoefficient, mPostCoefficient;
  double mRate = 48000., mPre = 0., mPost[2]{}, mPhase[2]{}, mGainA = 1., mGainB = 0.;
  double mDryGain = .70710678, mWetGain = .70710678, mMixSmoothing = 0.;
  double mVintagePhase = 0., mVintageGain = 0.;
  bool mFirst = true;
};

// Four overlapping read grains under sin^2 windows a quarter apart. Grains that
// keep a fixed spacing cancel tones whose phase difference between grains is
// half a cycle (220 Hz at 25 ms), so each grain restarts at a random extra
// delay. The grains then sum incoherently: sin^2 power over four phases is a
// constant 1.5, so the output keeps unity power without flutter at the grain
// rate, and a power guard keeps it from exceeding the input. Each channel keeps
// its own ratio, grain phase, offsets and power estimate.
class PitchShifter
{
public:
  static constexpr int kGrains = 4;
  void Reset(double rate, double windowSeconds = .1, double rightOffset = .125, double jitterSeconds = .02)
  {
    mRate = std::max(1., rate);
    mWindow = std::max(64, static_cast<int>(mRate * windowSeconds));
    mJitter = std::max(0., mRate * jitterSeconds);
    for (auto& line : mLine)
      line.Reset(mWindow + static_cast<int>(mJitter) + 8);
    mPhase[0] = 0.;
    mPhase[1] = rightOffset - std::floor(rightOffset);
    mRatio = {1., 1.};
    mSeed = {0x2545f491u, 0x9e3779b9u};
    for (int ch = 0; ch < 2; ++ch)
      for (int g = 0; g < kGrains; ++g)
      {
        mOffset[ch][g] = Random(ch) * mJitter;
        mLast[ch][g] = GrainPhase(ch, g);
      }
    mSmoothing = std::exp(-1. / (.01 * mRate));
    mPowerSmoothing = std::exp(-1. / (.05 * mRate));
    mInPower = mOutPower = {};
    mGain = {kIncoherent, kIncoherent};
    mGainCounter = {};
  }
  // Moves both channels' grain phases by `offset` of a window, for several shifters side by side.
  void Stagger(double offset)
  {
    for (int ch = 0; ch < 2; ++ch)
    {
      mPhase[ch] += offset;
      mPhase[ch] -= std::floor(mPhase[ch]);
      for (int g = 0; g < kGrains; ++g) mLast[ch][g] = GrainPhase(ch, g);
    }
  }
  // Records input without producing output, so a muted shifter keeps a current history.
  void Feed(double input, int channel) { mLine[channel != 0 ? 1 : 0].Push(input); }
  double ProcessSample(double input, int channel, double semitones)
  {
    const int ch = channel != 0 ? 1 : 0;
    const double target = mTarget[ch](semitones / 12., [](double x) { return std::exp2(x); });
    // Each channel advances once per sample, including stereo Shimmer's two calls.
    double& ratio = mRatio[ch];
    ratio = target + mSmoothing * (ratio - target);
    mLine[ch].Push(input);
    if (std::abs(ratio - 1.) < 1.e-5)
      return input;
    mPhase[ch] += (1. - ratio) / mWindow;
    mPhase[ch] -= std::floor(mPhase[ch]);
    // sin^2 windows a quarter apart from one sine/cosine pair: sin^2(t), sin^2(t + pi/4)
    // = (s + c)^2 / 2, cos^2(t) and cos^2(t + pi/4) = (c - s)^2 / 2.
    const double t = grid_fx::pi * mPhase[ch], s = std::sin(t), c = std::cos(t);
    const std::array<double, kGrains> window{s * s, .5 * (s + c) * (s + c), c * c, .5 * (c - s) * (c - s)};
    double sum = 0.;
    for (int g = 0; g < kGrains; ++g)
    {
      const double phase = GrainPhase(ch, g);
      // A wrap moves the read across the whole window, where the window is silent.
      if (std::abs(phase - mLast[ch][g]) > .5) mOffset[ch][g] = Random(ch) * mJitter;
      mLast[ch][g] = phase;
      sum += window[g] * mLine[ch].Read(std::max(1., phase * mWindow + mOffset[ch][g]));
    }
    // Grains that happen to read coherent audio sum louder, by up to 1.63x. Inside
    // a feedback loop that would grow, so the output power follows the input
    // power over a few grains and never exceeds it.
    // Compare with the input the grains read on average, half a window plus half the jitter ago.
    const double read = mLine[ch].Read(.5 * (mWindow + mJitter));
    mInPower[ch] = grid_fx::Flush(read * read + mPowerSmoothing * (mInPower[ch] - read * read));
    mOutPower[ch] = grid_fx::Flush(sum * sum + mPowerSmoothing * (mOutPower[ch] - sum * sum));
    // The estimates follow power over 50 ms; refreshing the gain every 16 samples is enough.
    if ((++mGainCounter[ch] & 15) == 0)
      mGain[ch] = mOutPower[ch] > 1.e-30 ? std::min(kIncoherent, std::sqrt(mInPower[ch] / mOutPower[ch])) : kIncoherent;
    return sum * mGain[ch];
  }

private:
  double GrainPhase(int ch, int g) const
  {
    const double phase = mPhase[ch] + g * (1. / kGrains);
    return phase >= 1. ? phase - 1. : phase;
  }
  double Random(int ch)
  {
    auto& seed = mSeed[ch];
    seed ^= seed << 13; seed ^= seed >> 17; seed ^= seed << 5;
    return (seed >> 8) * (1. / 16777216.);
  }
  std::array<grid_fx::DelayLine, 2> mLine;
  std::array<grid_fx::Memo, 2> mTarget;
  double mRate = 48000., mSmoothing = 0., mJitter = 0., mPhase[2]{};
  std::array<std::array<double, kGrains>, 2> mOffset{}, mLast{};
  // 1/sqrt(1.5): unity power for incoherent grains.
  static constexpr double kIncoherent = .816496580927726;
  std::array<double, 2> mRatio{1., 1.}, mInPower{}, mOutPower{}, mGain{kIncoherent, kIncoherent};
  std::array<uint32_t, 2> mGainCounter{};
  double mPowerSmoothing = 0.;
  std::array<uint32_t, 2> mSeed{0x2545f491u, 0x9e3779b9u};
  int mWindow = 4800;
};

// Shimmer reverb: two decorrelated eight-line tanks. In each tank, Amount
// crossfades the recirculation of four lines between their own signal and a
// pitch-shifted copy, so every pass moves part of the tail by Pitch while the
// loop gain never exceeds the Decay gain: layers climb for as long as the tail
// lasts and the tank stays stable at any Pitch. The shifted lines are band-
// limited before shifting up. This follows the principle of a pitch shifter
// inside the reverb feedback, as in classic studio shimmer designs.
class Shimmer
{
public:
  // Share of each shifted line's recirculation that is pitch-shifted, for
  // Amount 0-1. Exposed for the display.
  static double ShiftShare(double amount) { return kMaxShare * std::clamp(amount, 0., 1.); }
  void Reset(double rate)
  {
    mMixLaw.Reset(rate);
    mRate = std::max(1., rate);
    mControls.Reset(mRate);
    mTank[0].Reset(mRate, 1., 0.);
    mTank[1].Reset(mRate, 1.037, .5);
    for (int ch = 0; ch < 2; ++ch)
      mInputLow[ch] = 0.;
    mFreeze = 0.;
    mCounter = 0;
    mShare = mShareTarget = 0.;
    mShareSmoothing = std::exp(-1. / (.02 * mRate));
    mDrain.Reset(mRate);
  }
  void Process(double* l, double* r, int n, const UtilityControls& c, bool draining = false)
  {
    mControls.Target(c.shimmer, uShimmerMix);
    const double freezeTarget = (!draining && c.shimmer[6] >= .5) ? 1. : 0.;
    const double coefficient = std::exp(-2. * grid_fx::pi * 100. / mRate);
    for (int i = 0; i < n; ++i)
    {
      mMixLaw.Tick(c.constantDry);
      const auto& v = mControls.Tick();
      mFreeze += std::clamp(freezeTarget - mFreeze, -1. / (.01 * mRate), 1. / (.01 * mRate));
      if (mCounter++ % 32 == 0)
      {
        const double live = 1. - mFreeze;
        // Shifting up moves content above Nyquist: band-limit the shifted lines first.
        const double ratio = std::exp2(std::max(0., v[3]) / 12.);
        const double g = std::tan(grid_fx::pi * std::min(16000., .42 * mRate / ratio) / mRate);
        for (int ch = 0; ch < 2; ++ch)
        {
          auto& tank = mTank[ch];
          for (int j = 0; j < 8; ++j)
          {
            const double normal = std::pow(10., -3. * tank.length[j] / std::max(.25, v[1]));
            tank.feedback[j] = std::min(.9995, normal + mFreeze * (.9995 - normal));
          }
          tank.damping = std::exp(-2. * grid_fx::pi * std::min(.45 * mRate, v[2] + mFreeze * (12000. - v[2])) / mRate);
          tank.modDepth = v[5] * .01 * .000625 * mRate;
          tank.live = live;
          tank.aliasG = g / (1. + g);
        }
        // Freeze holds the tail without adding new pitch layers.
        mShareTarget = ShiftShare(v[4] * .01) * live;
      }
      mCounter %= 32;
      mShare = mShareTarget + mShareSmoothing * (mShare - mShareTarget);
      const double dryL = l[i], dryR = r ? r[i] : dryL;
      // Draining fades the pitched share with the input; the tail then decays as a plain reverb.
      const double share = mShare * mDrain.Tick(draining);
      mInputLow[0] = grid_fx::Flush(dryL + coefficient * (mInputLow[0] - dryL));
      mInputLow[1] = grid_fx::Flush(dryR + coefficient * (mInputLow[1] - dryR));
      const double inputL = (.75 * (dryL - mInputLow[0]) + .25 * (dryR - mInputLow[1])) * (1. - mFreeze);
      const double inputR = (.25 * (dryL - mInputLow[0]) + .75 * (dryR - mInputLow[1])) * (1. - mFreeze);
      const double wetL = kOutput * mTank[0].Process(inputL, share, v[3]);
      const double wetR = kOutput * mTank[1].Process(inputR, share, v[3]);
      const double mix = v[0] * .01;
      l[i] = mMixLaw.Apply(dryL + mix * (wetL - dryL), dryL, mix, 1.);
      if (r)
        r[i] = mMixLaw.Apply(dryR + mix * (wetR - dryR), dryR, mix, 1.);
    }
  }

private:
  // Output scaling puts the wet level with Room and Hall.
  static constexpr double kOutput = 1.8, kMaxShare = .5;
  grid_fx::MixLaw mMixLaw;
  struct Tank
  {
    static constexpr std::array<int, 4> kShifted{0, 3, 5, 6};
    std::array<grid_fx::DelayLine, 8> line;
    std::array<grid_fx::DelayLine, 4> diffuser;
    std::array<double, 8> length{}, damp{}, feedback{}, phase{};
    std::array<double, 4> diffuserDelay{};
    std::array<PitchShifter, 2> shifter;
    std::array<std::array<double, 2>, 4> alias{}, crossPower{};
    std::array<double, 4> crossGain{1., 1., 1., 1.};
    uint32_t counter = 0;
    double damping = 0., modDepth = 0., live = 1., aliasG = 1., powerSmoothing = 0., mRate = 48000.;
    void Reset(double rate, double scale, double grainOffset)
    {
      mRate = std::max(1., rate);
      constexpr std::array<double, 8> base{.0297, .0371, .0411, .0437, .0531, .0617, .0719, .0797};
      constexpr std::array<double, 4> diffusion{.0043, .0067, .0091, .0137};
      for (int j = 0; j < 8; ++j)
      {
        length[j] = base[j] * scale;
        line[j].Reset(static_cast<int>(mRate * (.09 * scale + .002)) + 8);
        damp[j] = 0.;
        phase[j] = j * .137;
      }
      for (int j = 0; j < 4; ++j)
      {
        diffuserDelay[j] = diffusion[j] * scale;
        diffuser[j].Reset(static_cast<int>(mRate * diffuserDelay[j]) + 8);
      }
      // Staggered grain phases decorrelate the four shifted lines.
      shifter[0].Reset(mRate, .1, .5, .02);
      shifter[1].Reset(mRate, .1, .5, .02);
      shifter[0].Stagger(grainOffset + .125);
      shifter[1].Stagger(grainOffset + .375);
      alias = {};
      crossPower = {};
      crossGain = {1., 1., 1., 1.};
      counter = 0;
      powerSmoothing = std::exp(-1. / (.05 * mRate));
    }
    double Process(double input, double share, double semitones)
    {
      for (int j = 0; j < 4; ++j)
      {
        const double delayed = diffuser[j].Read(mRate * diffuserDelay[j]);
        const double y = delayed - .6 * input;
        diffuser[j].Push(input + .6 * y);
        input = y;
      }
      std::array<double, 8> taps{};
      double wet = 0.;
      for (int j = 0; j < 8; ++j)
      {
        phase[j] += (.13 + .043 * j) / mRate;
        if (phase[j] >= 1.)
          phase[j] -= 1.;
        const double motion = modDepth * std::sin(2. * grid_fx::pi * phase[j]);
        const double tap = line[j].Read(mRate * length[j] + motion);
        damp[j] = grid_fx::Flush(tap + damping * (damp[j] - tap));
        taps[j] = damp[j] * feedback[j];
        wet += (j % 2 ? -1. : 1.) * tap * .125;
      }
      // The shifters keep their history at zero share, so Amount changes glide in.
      const double pitch = semitones * live;
      ++counter;
      for (int k = 0; k < 4; ++k)
      {
        double x = taps[kShifted[k]];
        for (auto& state : alias[k])
        {
          const double low = state + aliasG * (x - state);
          state = grid_fx::Flush(low + aliasG * (x - state));
          x = low;
        }
        if (share < 1.e-6)
        {
          shifter[k / 2].Feed(x, k % 2);
          continue;
        }
        const double dry = taps[kShifted[k]];
        const double shifted = shifter[k / 2].ProcessSample(x, k % 2, pitch);
        // A linear crossfade of two unrelated signals loses up to 3 dB per pass, which
        // would shorten the tail as Amount rises. Restore the line's power, but never
        // beyond it, so the shifted lines still decay at the Decay rate.
        const double y = dry + share * (shifted - dry);
        auto& power = crossPower[k];
        power[0] = grid_fx::Flush(dry * dry + powerSmoothing * (power[0] - dry * dry));
        power[1] = grid_fx::Flush(y * y + powerSmoothing * (power[1] - y * y));
        if ((counter & 15) == 0)
        {
          const double cap = 1. / std::sqrt((1. - share) * (1. - share) + share * share);
          crossGain[k] = power[1] > 1.e-30 ? std::min(cap, std::sqrt(power[0] / power[1])) : 1.;
        }
        taps[kShifted[k]] = y * crossGain[k];
      }
      for (int span = 1; span < 8; span *= 2)
        for (int base = 0; base < 8; base += span * 2)
          for (int j = 0; j < span; ++j)
          {
            const double a = taps[base + j], b = taps[base + j + span];
            taps[base + j] = a + b;
            taps[base + j + span] = a - b;
          }
      for (int j = 0; j < 8; ++j)
        line[j].Push(taps[j] * .3535533905932738 + input * (j % 3 == 0 ? -.2 : .2));
      return wet * 1.8;
    }
  };
  Tank mTank[2];
  grid_fx::SmoothControls<7> mControls;
  grid_fx::DrainRamp mDrain;
  double mRate = 48000., mInputLow[2]{}, mFreeze = 0., mShare = 0., mShareTarget = 0., mShareSmoothing = 0.;
  int mCounter = 0;
};

// Analytic-signal pair from two chains of second-order allpass sections, using
// Olli Niemitalo's published polyphase IIR Hilbert transformer coefficients
// (https://yehar.com/blog/?p=368). The quadrature output lags the in-phase output
// by 90 degrees within about 0.7 degrees from 30 Hz to 20 kHz at 44.1/48 kHz
// (60 Hz at 96 kHz), with unity magnitude and no lookahead.

class HilbertPair
{
public:
  void Reset() { mInPhase = {}; mQuadrature = {}; mDelayed = 0.; }
  // Returns {in-phase, quadrature}.
  std::array<double, 2> Process(double x)
  {
    const double quadrature = mDelayed;
    mDelayed = Chain(mQuadrature, quadratureCoefficients, x);
    return {Chain(mInPhase, inPhaseCoefficients, x), quadrature};
  }

private:
  struct Section { double x1 = 0., x2 = 0., y1 = 0., y2 = 0.; };
  using Sections = std::array<Section, 4>;
  static constexpr std::array<double, 4> inPhaseCoefficients{.4021921162426, .8561710882420, .9722909545651,
                                                             .9952884791278};
  static constexpr std::array<double, 4> quadratureCoefficients{.6923878, .9360654322959, .9882295226860,
                                                                .9987488452737};
  static double Chain(Sections& sections, const std::array<double, 4>& a, double x)
  {
    for (size_t j = 0; j < sections.size(); ++j)
    {
      auto& s = sections[j];
      const double y = grid_fx::Flush(a[j] * a[j] * (x + s.y2) - s.x2);
      s.x2 = s.x1; s.x1 = x;
      s.y2 = s.y1; s.y1 = y;
      x = y;
    }
    return x;
  }
  Sections mInPhase{}, mQuadrature{};
  double mDelayed = 0.;
};

class FrequencyShifter
{
public:
  // The single-sideband shift is unity gain, so the loop gain at Feedback 100% used to be
  // about one, bounded only by the soft limiter. The top of the invariant 0-100% range now
  // maps just below unity so a settled tail always decays. Repeated downward shifts fold
  // partials toward 0 Hz, where a loop gain of one would leave a standing near-DC tone and
  // offset; the feedback path therefore loses its low end, while the first, directly shifted
  // pass keeps it.
  static constexpr double kMaximumFeedback = .97;
  void Reset(double rate)
  {
    mRate = std::max(1., rate);
    mControls.Reset(mRate);
    for (int ch = 0; ch < 2; ++ch)
    {
      mLine[ch].Reset(static_cast<int>(mRate * 1.05) + 8);
      mHilbert[ch].Reset();
      mLow[ch] = 0.;
      mFeedbackIn[ch] = mFeedbackOut[ch] = 0.;
    }
    mFeedbackPole = std::exp(-6.283185307179586 * kFeedbackHighPass / mRate);
    mCosine = 1.;
    mSine = 0.;
    mCosOffset = 1.;
    mSinOffset = 0.;
    mCounter = 0;
    mDrain.Reset(mRate);
  }
  void Process(double* l, double* r, int n, const UtilityControls& c, bool draining = false)
  {
    mControls.Target(c.shift, uShiftFrequency);
    for (int i = 0; i < n; ++i)
    {
      const auto& v = mControls.Tick();
      if (mCounter++ % 32 == 0)
      {
        const double offset = std::clamp(v[5], 0., 100.) * .01 * grid_fx::pi;
        mCosOffset = std::cos(offset);
        mSinOffset = std::sin(offset);
      }
      mCounter %= 32;
      const double radians = 2. * grid_fx::pi * std::clamp(v[0], -2000., 2000.) / mRate;
      const double stepCos = std::cos(radians), stepSin = std::sin(radians);
      const double rightCosine = mCosine * mCosOffset - mSine * mSinOffset;
      const double rightSine = mSine * mCosOffset + mCosine * mSinOffset;
      const double delaySamples = std::max(1., v[2] * .001 * mRate);
      const double side = v[3] * .01;
      const double feedback = std::clamp(v[1], 0., 100.) * .01 * kMaximumFeedback * mDrain.Tick(draining);
      const double dryL = l[i], dryR = r ? r[i] : dryL;
      const double lowPole = mLowPole(-2. * grid_fx::pi * std::min(.45 * mRate, v[4]) / mRate,
                                     [](double x) { return std::exp(x); });
      double wet[2]{};
      for (int ch = 0; ch < 2; ++ch)
      {
        const double dry = ch ? dryR : dryL;
        const auto [inPhase, quadrature] = mHilbert[ch].Process(mLine[ch].Read(delaySamples));
        const double cosine = ch ? rightCosine : mCosine, sine = ch ? rightSine : mSine;
        const double upper = inPhase * cosine - quadrature * sine;
        const double lower = inPhase * cosine + quadrature * sine;
        const double weight = .5 * (side + 1.);
        double shifted = lower + (upper - lower) * weight;
        mLow[ch] = grid_fx::Flush(
          shifted + lowPole * (mLow[ch] - shifted));
        shifted = mLow[ch];
        mLine[ch].Push(dry + feedback * SoftLimit(FeedbackHighPass(shifted, ch)));
        wet[ch] = shifted;
      }
      const double mix = v[6] * .01;
      l[i] = dryL + mix * (wet[0] - dryL);
      if (r)
        r[i] = dryR + mix * (wet[1] - dryR);
      const double nextCosine = mCosine * stepCos - mSine * stepSin;
      mSine = mSine * stepCos + mCosine * stepSin;
      mCosine = nextCosine;
    }
  }

private:
  // One-pole DC blocker in the feedback path; the wet output keeps the shifted low end.
  static constexpr double kFeedbackHighPass = 20.;
  double FeedbackHighPass(double x, int ch)
  {
    const double y = x - mFeedbackIn[ch] + mFeedbackPole * mFeedbackOut[ch];
    mFeedbackIn[ch] = x;
    mFeedbackOut[ch] = grid_fx::Flush(y);
    return mFeedbackOut[ch];
  }
  std::array<grid_fx::DelayLine, 2> mLine;
  std::array<HilbertPair, 2> mHilbert;
  grid_fx::SmoothControls<7> mControls;
  grid_fx::Memo mLowPole;
  grid_fx::DrainRamp mDrain;
  double mRate = 48000., mCosine = 1., mSine = 0., mCosOffset = 1., mSinOffset = 0.;
  double mFeedbackPole = 1.;
  std::array<double, 2> mFeedbackIn{}, mFeedbackOut{};
  double mLow[2]{};
  int mCounter = 0;
};
} // namespace grid_hq
