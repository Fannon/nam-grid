#pragma once
#include <bit>
#include "SlotGuitarEffects.h"
#include "SlotPitchShift.h"

// Original NAM Grid MIT delay designs. The tape recorder uses the attributed
// Airwindows Spiral transfer and the same minimum-phase oversampling as LoFi Tape.
// See docs/module-implementation.md for design and hardware references.
namespace expanded_delay
{
inline double Blend(double from, double to, double amount) { return from + amount * (to - from); }
inline double LowPass(double x, double& state, double pole)
{ return state = grid_fx::Flush(x + pole * (state - x)); }
inline double Pole(double hz, double rate)
{ return std::exp(-2. * grid_fx::pi * std::min(hz, .45 * rate) / rate); }
inline double Limit(double x, double knee = 1.5)
{
  const double excess = std::max(0., std::abs(x) - knee);
  return x - std::copysign(excess * excess / (.45 + excess), x);
}

// Long rings use float storage, with double interpolation and filter states.
using Line = grid_fx::BasicDelayLine<float>;

// Each tap has its own transition. A tempo jump or a large move of a single
// tap crossfades the old and new reads, including the signal returned to the
// loop. Small changes still glide. One pending jump may replace an older one.
class TapChange
{
public:
  void Reset(double rate)
  {
    *this = {};
    mJump = .002 * rate;
    mStep = 1. / (.03 * rate);
    mPole = std::exp(-1. / (.02 * rate));
  }
  void Tick(double target)
  {
    if (!mStarted)
    {
      mTo = mLastTarget = target;
      mStarted = true;
    }
    mPending |= std::abs(target - mLastTarget) > mJump;
    mLastTarget = target;
    if (mFade == 1.)
    {
      if (mPending && std::abs(target - mTo) > mJump)
      {
        mFrom = mTo;
        mTo = target;
        mFade = 0.;
      }
      else
        mTo = target + mPole * (mTo - target);
      mPending = false;
    }
    mFade = std::min(1., mFade + mStep);
  }
  double Read(const Line& line, double modulation = 0.) const
  {
    const double tap = line.Read(mTo + modulation);
    if (mFade == 1.) return tap;
    const double weight = mFade * mFade * (3. - 2. * mFade);
    return Blend(line.Read(mFrom + modulation), tap, weight);
  }
private:
  double mFrom = 0., mTo = 0., mLastTarget = 0., mFade = 1.;
  double mJump = 0., mStep = 0., mPole = 0.;
  bool mStarted = false, mPending = false;
};

// Two overlapping Hann grains. Positions are latched at launch so automation
// cannot step a currently audible read head. Grain starts are deterministic and
// independent of callback partitioning. Every read is from past samples.
class Slices
{
public:
  void Reset(double rate)
  {
    *this = {};
    mRate = rate;
  }
  void Read(const Line& source, double delay,
             double length, double speed, double scatter, bool reverse,
             bool glitch, int channel, double& result)
  {
    if (channel == 0)
    {
      if (!mStarted)
      {
        Launch(mGrains[0], delay, length, speed, scatter, reverse, glitch);
        mGrains[0].phase = .5;
        Launch(mGrains[1], delay, length, speed, scatter, reverse, glitch);
        mStarted = true;
      }
      if (mGrains[0].phase >= 1. || mGrains[1].phase >= 1.)
      {
        const int expired = mGrains[0].phase >= 1. ? 0 : 1;
        mGrains[1 - expired].phase = .5;
        Launch(mGrains[expired], delay, length, speed, scatter, reverse, glitch);
      }
    }
    const double phase = mGrains[0].phase;
    const double window = .5 - .5 * std::cos(2. * grid_fx::pi * phase);
    result = Blend(source.Read(mGrains[1].age + (channel ? mGrains[1].offset : 0.)),
                   source.Read(mGrains[0].age + (channel ? mGrains[0].offset : 0.)), window);
  }
  void Advance()
  {
    for (auto& grain : mGrains)
    {
      grain.age += grain.step;
      grain.phase += grain.increment;
    }
    mPosition += 1.;
  }
private:
  double Random()
  {
    mSeed ^= mSeed << 13; mSeed ^= mSeed >> 17; mSeed ^= mSeed << 5;
    return static_cast<double>(mSeed) * (1. / 4294967295.);
  }
  struct Grain
  {
    double age = 1., offset = 0., step = 0., phase = 0., increment = 1.;
  };
  void Launch(Grain& grain, double delay, double length, double speed, double scatter, bool reverse, bool glitch)
  {
    const double frames = std::max(8., std::round(length * mRate * .001));
    // Normal forward playback at zero scatter reads the ordinary delay tap.
    const double step = reverse ? 1. + speed : 1. - speed;
    const double earliest = reverse ? 0. : std::max(0., -step * frames);
    const double jitter = scatter * std::min(delay, .42 * mRate);
    double age = std::max(2. + earliest, delay + (Random() - .5) * jitter - step * frames * .5);
    if (glitch)
    {
      if (mLatch == 0 && Random() < .2 + .65 * scatter)
      {
        mCaptured = mPosition - age;
        mLatch = 2 + static_cast<int>(5. * scatter);
      }
      if (mLatch > 0)
      {
        age = std::max(2. + earliest, mPosition - mCaptured);
        --mLatch;
      }
    }
    grain = {age, reverse || scatter > 0. ? (Random() - .5) * jitter * .24 : 0., step, 0., 1. / frames};
    grain.offset = std::max(2. + earliest - age, grain.offset);
  }
  std::array<Grain, 2> mGrains{};
  double mRate = 48000., mPosition = 0., mCaptured = 0.;
  uint32_t mSeed = 0xc283ee81u;
  int mLatch = 0;
  bool mStarted = false;
};

// The four-pole input filter suppresses foldback when pitched grains read faster
// than real time. Coefficients update at a fixed cadence independent of blocks.
class PitchLowPass
{
public:
  void Configure(double hz, double rate)
  {
    if (hz == mFrequency) return;
    mFrequency = hz;
    const double g = std::tan(grid_fx::pi * hz / rate);
    for (size_t i = 0; i < mStages.size(); ++i)
    {
      auto& s = mStages[i];
      const double k = i ? .7653668647301795 : 1.8477590650225735;
      s.a1 = 1. / (1. + g * (g + k)); s.a2 = g * s.a1; s.a3 = g * s.a2;
    }
  }
  double Tick(double x)
  {
    for (auto& s : mStages)
    {
      const double v3 = x - s.b;
      const double v1 = s.a1 * s.a + s.a2 * v3;
      const double v2 = s.b + s.a2 * s.a + s.a3 * v3;
      s.a = grid_fx::Flush(2. * v1 - s.a);
      s.b = grid_fx::Flush(2. * v2 - s.b);
      x = v2;
    }
    return x;
  }
private:
  struct Stage { double a = 0., b = 0., a1 = 0., a2 = 0., a3 = 0.; };
  std::array<Stage, 2> mStages{};
  double mFrequency = -1.;
};
} // namespace expanded_delay

class SlotExpandedDelay
{
  using Stereo = std::array<double, 2>;
public:
  void Reset(double rate, int block, SlotEffect effect)
  {
    mEffect = effect;
    mRate = rate;
    const int count = effect == SlotEffect::PatternDelay ? 4 : 1;
    // Host sync reaches eight seconds. Grains can read older audio than Time,
    // up to 1.5 s during a long reverse grain or a latched glitch.
    const double seconds = effect == SlotEffect::GranularDelay || effect == SlotEffect::PitchDelay ? 10. : 8.025;
    for (auto& channel : mChannel)
    {
      channel = {};
      for (int line = 0; line < count; ++line)
        channel.line[line].Reset(static_cast<int>(std::ceil(rate * seconds)));
      if (effect == SlotEffect::SpaceTapeEcho) channel.tape.Reset();
      if (effect == SlotEffect::BucketBrigade) channel.bbd.Reset();
    }
    if (effect == SlotEffect::SpaceTapeEcho)
    {
      mSpring.Reset(rate);
      mSpringInput[0].assign(block, 0.);
      mSpringInput[1].assign(block, 0.);
    }
    // A fresh tank is silent, so a Spring that starts at 0% never runs.
    mSpringDrain = static_cast<int64_t>(4. * rate);
    mSpringIdle = mSpringDrain;
    if (effect == SlotEffect::PitchDelay)
    {
      mPitch.Reset(rate);
      mRightPitch.Reset(rate);
    }
    mSlices.Reset(rate);
    mControls.Reset(rate); mOptions.Reset(rate); mExtras.Reset(rate);
    mMix.Reset(rate); mDrain.Reset(rate);
    mMode = -1; mModeGain = 1.; mCounter = 0;
    mPhase = mFlutter = mCapturePhase = mEnv = 0.;
    for (auto& tap : mTaps) tap.Reset(rate);
    mDC = expanded_delay::Pole(28., rate);
    mAttack = std::exp(-1. / (.003 * rate));
    mCompAttack = std::exp(-1. / (.0015 * rate));
    mCompRelease = std::exp(-1. / (.08 * rate));
  }

  void Process(double* left, double* right, int frames, const UtilityControls& c, bool draining)
  {
    using namespace expanded_delay;
    auto values = c.delay;
    if (c.sync) values[0] = 1000. * grid_tempo::Seconds(c.tempo, c.division);
    mControls.Target(values, uDelayTime, c.sync);
    mOptions.Target(c.expandedDelay, uSpaceHeads);
    mExtras.Target({c.delayMotion, c.delayOutput, c.extra.duckRelease}, {uDelayMotion, uDelayOutput, uDuckRelease});
    const int modeControl = ExpandedDelayModeControl(mEffect);
    const int targetMode = std::clamp(static_cast<int>(c.DelayOption(modeControl)), 0,
                                      static_cast<int>(EffectChoices(modeControl).size()) - 1);
    if (mMode < 0) mMode = targetMode;
    const bool tape = mEffect == SlotEffect::SpaceTapeEcho;
    const bool analog = mEffect == SlotEffect::BucketBrigade;
    const bool pitch = mEffect == SlotEffect::PitchDelay;
    const bool granular = mEffect == SlotEffect::GranularDelay;
    const bool vintage = mEffect == SlotEffect::VintageDigital;
    const bool fx = mEffect == SlotEffect::FeedbackFXDelay;
    const bool pattern = mEffect == SlotEffect::PatternDelay;
    const int channels = right ? 2 : 1;
    // Reuse the classic spring as a wet-only parallel path. Its input comes
    // from the original playing, independently of echo head selection. At 0%
    // the spring stops listening, rings out in silence (over -150 dB at its
    // 1.55 s decay), then parks. Raised again, it resumes from a silent tank.
    const bool listening = tape && c.DelayOption(uEchoSpring) > 0.;
    if (listening) mSpringIdle = 0;
    const bool spring = tape && mSpringIdle < mSpringDrain;
    if (spring)
    {
      if (listening)
      {
        std::copy_n(left, frames, mSpringInput[0].data());
        if (right) std::copy_n(right, frames, mSpringInput[1].data());
      }
      else
      {
        std::fill_n(mSpringInput[0].data(), frames, 0.);
        if (right) std::fill_n(mSpringInput[1].data(), frames, 0.);
        mSpringIdle += frames;
      }
      mSpring.Process(mSpringInput[0].data(), right ? mSpringInput[1].data() : nullptr, frames, mSpringControls);
    }
    for (int i = 0; i < frames; ++i)
    {
      const auto& v = mControls.Tick();
      const auto& options = mOptions.Tick();
      const auto& extra = mExtras.Tick();
      const auto option = [&](int p) { return options[p - uSpaceHeads]; };
      const double live = mDrain.Tick(draining);
      mMix.Tick(c.constantDry);
      // Selector transitions dip the wet signal and feedback. The dry path
      // stays at its current Mix gain and the stored echoes keep their timing.
      const double transition = 1. / (.012 * mRate);
      mModeGain = mMode == targetMode ? std::min(1., mModeGain + transition) : std::max(0., mModeGain - transition);
      if (mModeGain == 0. && mMode != targetMode)
      {
        mMode = targetMode;
        mSlices.Reset(mRate);
        mCounter = 0;
      }
      const double time = v[0] * .001 * mRate;
      if (!tape && !analog && !granular)
      {
        const double target = std::clamp(values[0], 5., c.sync ? 8000. : 4000.) * .001 * mRate;
        if (pattern)
          for (int tap = 0; tap < 4; ++tap)
            mTaps[tap].Tick(target * .01 * std::clamp(c.DelayOption(uTapTime1 + tap), 5., 100.));
        else
          mTaps[0].Tick(target);
      }
      double motionRate = option(uEchoRate);
      if (fx && mMode != 3 && c.DelayOption(uEchoRateSync) >= .5) motionRate *= grid_tempo::Tempo(c.tempo) / 60.;
      if (analog && mMode == 2) motionRate *= 4.;
      if (tape) motionRate = .43 * std::clamp(std::sqrt(420. / v[0]), .55, 2.5) * (mMode == 7 ? 2.7 : 1.);
      if (fx && mMode == 3) motionRate = option(uEchoRingRate);
      if (tape || analog || pitch || fx || vintage || (pattern && mMode == 3))
      {
        mPhase += motionRate / mRate;
        mPhase -= std::floor(mPhase);
      }
      if (tape)
      {
        mFlutter += 6.7 * motionRate / (.43 * mRate);
        mFlutter -= std::floor(mFlutter);
      }
      const bool crushed = vintage && (mMode == 0 || mMode == 3);
      if (mCounter == 0)
      {
        double tone = v[2];
        if (tape) tone *= mMode == 7 ? .60 : std::clamp(std::sqrt(420. / v[0]), .55, 1.45);
        if (analog)
        {
          constexpr std::array<double, 5> widths{.70, 1., .96, 1.5, .84};
          tone = std::min(tone, 4300. * std::clamp(std::sqrt(330. / v[0]), .38, 1.65) * widths[mMode]);
        }
        if (pattern && mMode == 3) tone *= .8;
        mTone = Pole(tone, mRate);
        mLowCut = Pole(vintage ? option(uEchoLowCut) : tape ? 70. : 42., mRate);
        mRatio = std::exp2(option(uEchoInterval) / 12.);
        if (pitch || granular)
        {
          const double cutoff = std::min(tone, .39 * mRate / std::max(1., pitch && mMode == 2 ? std::max(mRatio, 1. / mRatio) : mRatio));
          for (int ch = 0; ch < channels; ++ch) mChannel[ch].pitchFilter.Configure(cutoff, mRate);
        }
        const double sampleRate = mMode == 0 ? 24000. : option(uEchoSampleRate);
        mCaptureIncrement = std::min(sampleRate / mRate, 1.);
        mCrushInput = Pole(.36 * sampleRate, mRate);
        mCrushOutput = Pole(.45 * sampleRate, mRate);
        mLevels = std::exp2((mMode == 0 ? 12. : std::round(option(uEchoBits))) - 1.);
        if (vintage) mRelease = std::exp(-1000. / (extra[2] * mRate));
        if (fx && mMode < 2) ConfigureSweep();
      }
      mCounter = (mCounter + 1) % 16;
      const Stereo dry{left[i], right ? right[i] : left[i]};
      const double headGain = tape ? 1. / std::sqrt(static_cast<double>(std::popcount(spaceHeads[mMode]))) : 1.;
      // Normalize the tape return by the sum of the enabled head gains.
      const double feedback = .01 * v[1] * live * mModeGain * headGain;
      Stereo wet{};
      if (pattern)
      {
        if (mMode == 3) mDrumMotion = std::sin(2. * grid_fx::pi * mPhase);
        wet = Pattern(dry, feedback, option, channels);
      }
      else
      {
        Stereo direct{}, shifted{};
        if (crushed) mCapturePhase += mCaptureIncrement;
        const double wave = tape || analog || pitch || fx || vintage ? std::sin(2. * grid_fx::pi * mPhase) : 0.;
        for (int ch = 0; ch < channels; ++ch)
        {
          auto& channel = mChannel[ch];
          if (tape)
          {
            const double flutter = .8 * std::sin(2. * grid_fx::pi * mFlutter) + .2 * std::sin(4. * grid_fx::pi * mFlutter);
            // The same shared wow and flutter rates as the LoFi Tape transport.
            const double move = extra[0] * .01 * mRate * (.0028 * wave + .00038 * flutter);
            const unsigned mask = spaceHeads[mMode];

            for (int head = 0; head < 3; ++head)
              if (mask & (1u << head))
                direct[ch] += headGain * channel.line[0].Read((time + move) * HeadFraction(mMode, head));
          }
          else if (granular)
          {
            const double length = option(uGrainLength) * (1.25 - .75 * option(uGrainDensity) * .01);
            mSlices.Read(channel.line[0], time, length, mRatio, option(uGrainScatter) * .01, mMode == 1, mMode == 2, ch, direct[ch]);
          }
          else
          {
            const double stereo = ch ? -1. : 1.;
            const double movement = analog ? extra[0] * .01 * (mMode == 0 ? .00035 : .0024) * wave
              : vintage ? option(uEchoDepth) * .01 * .0017 * wave * stereo
              : pitch ? option(uEchoDetune) * .00013 * wave * stereo : 0.;
            direct[ch] = analog ? channel.line[0].Read(time + mRate * movement)
                                : mTaps[0].Read(channel.line[0], mRate * movement);
          }
          if (pitch && mMode == 1)
            mSlices.Read(channel.line[0], time, 60., mRatio, 0., true, false, ch, shifted[ch]);
        }
        if (granular || (pitch && mMode == 1)) mSlices.Advance();
        if (pitch && mMode != 1 && (option(uEchoShiftBlend) > 1.e-10 || mRatio == 1.))
        {
          const double a = mChannel[0].pitchFilter.Tick(direct[0]);
          const double b = right ? mChannel[1].pitchFilter.Tick(direct[1]) : 0.;
          if (mMode == 2)
          {
            shifted[0] = mPitch.Tick(a, 0., mRatio, false)[0];
            if (right) shifted[1] = mRightPitch.Tick(b, 0., 1. / mRatio, false)[0];
          }
          else
            shifted = mPitch.Tick(a, b, mRatio, right != nullptr);
        }
        for (int ch = 0; ch < channels; ++ch)
        {
          auto& channel = mChannel[ch];
          double echo = direct[ch];
          if (pitch)
          {
            // A zero interval in forward playback is direct, with optional
            // detune. At other intervals keep a distinct unshifted voice.
            if (mMode != 1 && mRatio == 1.) shifted[ch] = echo;
            echo = Blend(echo, shifted[ch], option(uEchoShiftBlend) * .01);
          }
          if (analog)
          {
            echo = LowPass(echo, channel.inBand, mTone);
            const double envelope = Envelope(echo, channel.expand);
            echo *= std::max(.045, envelope);
          }
          if (fx) echo = FeedbackEffect(echo, channel, ch, wave, option);
          if (crushed)
          {
            for (auto& s : channel.crushIn) echo = LowPass(echo, s, mCrushInput);
            if (mCapturePhase >= 1.) channel.held = std::trunc(echo * mLevels) / mLevels;
            echo = LowPass(LowPass(channel.held, channel.crushOne, mCrushOutput), channel.crushTwo, mCrushOutput);
          }
          echo = LowPass(echo, channel.tone, mTone);
          const double loop = echo - LowPass(echo, channel.low, mLowCut);
          wet[ch] = vintage ? loop : echo;
          double send = dry[ch] + feedback * Limit(loop);
          if (tape || analog) send -= LowPass(send, channel.dc, mDC);
          if (tape)
          {
            const double drive = 1. + .037 * v[3];
            send = channel.tape.Tick(send, [drive](double x) { return AirwindowsTape::Spiral(x * drive) / drive; });
          }
          if (analog)
          {
            const double envelope = Envelope(send, channel.compress);
            const double compressed = send / std::sqrt(std::max(.002025, envelope));
            const double drive = .7 + .013 * v[3] + (mMode == 4 ? .8 : 0.);
            send = channel.bbd.Tick(compressed, [drive](double x) { return std::tanh(drive * x) / drive; });
          }
          if (granular || (pitch && mMode == 1)) send = channel.pitchFilter.Tick(send);
          channel.line[0].Push(send);
        }
        if (crushed && mCapturePhase >= 1.) mCapturePhase -= 1.;
      }
      if (vintage)
      {
        const double peak = std::max(std::abs(dry[0]), std::abs(dry[1]));
        const double env = LowPass(peak, mEnv, peak > mEnv ? mAttack : mRelease);
        if (mMode == 2)
        {
          const double sensitivity = .2 + .20 * option(uEchoSensitivity);
          const double duck = 1. - option(uEchoDucking) * .01 * std::min(1., env * sensitivity);
          for (auto& sample : wet) sample *= duck;
        }
      }
      const double mix = v[4] * .01;
      const double output = mOutput(extra[1] * .05, [](double x) { return std::pow(10., x); });
      for (int ch = 0; ch < channels; ++ch)
      {
        double sample = wet[ch] * mModeGain;
        if (spring) sample += option(uEchoSpring) * .01 * mSpringInput[ch][i];
        (ch ? right[i] : left[i]) = mMix.Apply((dry[ch] + mix * (sample - dry[ch])) * output, dry[ch], mix, output);
      }
    }
  }

  // Test hook: whether the parallel spring still processes audio.
  bool SpringRunning() const { return mSpringIdle < mSpringDrain; }

private:
  struct Channel
  {
    std::array<expanded_delay::Line, 4> line;
    grid_oversampling::FIR<4, 32> tape;
    grid_oversampling::FIR<2, 32> bbd;
    expanded_delay::PitchLowPass pitchFilter;
    std::array<double, 4> tapTone{}, tapBass{}, phaser{};
    std::array<double, 3> crushIn{};
    double tone = 0., low = 0., dc = 0., inBand = 0., compress = 0., expand = 0.;
    double sweepLow = 0., sweepHigh = 0., held = 0., crushOne = 0., crushTwo = 0.;
  };
  const UtilityControls mSpringControls = [] {
    UtilityControls controls;
    controls.spring = {1.55, 0., 48., 100., 0.};
    controls.reverb[2] = 4700.;
    return controls;
  }();

  double Envelope(double x, double& state) const
  {
    const double peak = std::abs(x);
    return expanded_delay::LowPass(peak, state, peak > state ? mCompAttack : mCompRelease);
  }
  void ConfigureSweep()
  {
    using namespace expanded_delay;
    for (int ch = 0; ch < 2; ++ch)
    {
      const double position = .5 + .5 * std::sin(2. * grid_fx::pi * (mPhase + ch * .17));
      const double frequency = 420. * std::exp2(4. * position);
      mSweepLow[ch] = Pole(frequency * .57, mRate);
      mSweepHigh[ch] = Pole(frequency * 1.8, mRate);
      const double g = std::tan(grid_fx::pi * std::min(.4 * mRate, frequency) / mRate);
      mAllpass[ch] = (1. - g) / (1. + g);
    }
  }
  template<class Option>
  double FeedbackEffect(double x, Channel& c, int ch, double wave, Option option)
  {
    using namespace expanded_delay;
    const double depth = option(uEchoDepth) * .01;
    if (mMode == 3) return x * Blend(1., wave, depth);
    if (mMode == 2) return x * (1. - depth * (.5 + .5 * (ch ? -wave : wave)));
    const double resonance = option(uEchoResonance) * .01;
    if (mMode == 0)
    {
      const double band = LowPass(x, c.sweepHigh, mSweepHigh[ch]) - LowPass(x, c.sweepLow, mSweepLow[ch]);
      return Blend(x, (1.15 + .85 * resonance) * band, depth);
    }
    double phase = x;
    const double a = mAllpass[ch];
    for (auto& state : c.phaser)
    {
      const double next = grid_fx::Flush(phase + a * state);
      phase = state - a * next;
      state = next;
    }
    return Blend(x, phase, depth * (.5 + .5 * resonance));
  }
  template<class Option>
  Stereo Pattern(const Stereo& dry, double feedback, Option option, int channels)
  {
    using namespace expanded_delay;
    std::array<double, 4> gains{};
    double sum = 0., power = 0.;
    for (int tap = 0; tap < 4; ++tap)
    {
      gains[tap] = option(uTapLevel1 + tap) * .01;
      sum += gains[tap];
      power += gains[tap] * gains[tap];
    }
    const double normalization = 1. / std::sqrt(std::max(1., power));
    Stereo wet{};
    for (int ch = 0; ch < channels; ++ch)
    {
      auto& c = mChannel[ch];
      std::array<double, 4> loop{};
      double swell = 0.;
      for (int tap = 0; tap < 4; ++tap)
      {
        const double wobble = mMode == 3 ? mRate * .00018 * mDrumMotion * (tap + 1) : 0.;
        const double value = mTaps[tap].Read(c.line[tap], wobble);
        const double filtered = LowPass(value, c.tapTone[tap], mTone);
        const double sample = mMode == 3 ? Limit(1.5 * filtered, .55) / 1.5 : filtered;
        const double pan = channels == 2 ? option(uTapPan1 + tap) * .0001 * option(uEchoWidth) : 0.;
        wet[ch] += sample * gains[tap] * (1. - std::max(0., ch ? -pan : pan)) * normalization;
        loop[tap] = sample - LowPass(sample, c.tapBass[tap], mLowCut);
        swell += loop[tap] * gains[tap];
      }
      swell /= std::max(1., sum);
      // Orthogonal Householder scattering: -tap plus half of the sum.
      // In Swell and Drum the amplitude weights sum to at most one.
      const double halfSum = .5 * (loop[0] + loop[1] + loop[2] + loop[3]);
      for (int tap = 0; tap < 4; ++tap)
      {
        const double returned = mMode == 0 ? loop[3] : mMode == 2 ? halfSum - loop[tap] : swell;
        c.line[tap].Push(dry[ch] + feedback * Limit(returned));
      }
    }
    return wet;
  }

  SlotEffect mEffect = SlotEffect::SpaceTapeEcho;
  std::array<Channel, 2> mChannel;
  SlotSpring mSpring;
  std::array<std::vector<double>, 2> mSpringInput;
  pitch_shift::Grains mPitch, mRightPitch;
  expanded_delay::Slices mSlices;
  grid_fx::SmoothControls<5> mControls;
  grid_fx::SmoothControls<uTapPan4 - uSpaceHeads + 1> mOptions;
  grid_fx::SmoothControls<3> mExtras;
  grid_fx::MixLaw mMix;
  grid_fx::DrainRamp mDrain;
  grid_fx::Memo mOutput;
  Stereo mSweepLow{}, mSweepHigh{}, mAllpass{};
  double mRate = 48000., mModeGain = 1., mTone = 0., mLowCut = 0., mDC = 0.;
  std::array<expanded_delay::TapChange, 4> mTaps;
  double mAttack = 0., mRelease = 0., mEnv = 0., mCompAttack = 0., mCompRelease = 0.;
  double mPhase = 0., mFlutter = 0., mCapturePhase = 0., mCaptureIncrement = 1., mCrushInput = 0., mCrushOutput = 0.;
  double mRatio = 1., mLevels = 2048., mDrumMotion = 0.;
  int mMode = -1, mCounter = 0;
  int64_t mSpringIdle = 0, mSpringDrain = 0;
};
