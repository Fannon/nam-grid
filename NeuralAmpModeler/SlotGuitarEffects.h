#pragma once
#include "SlotTimeEffects.h"

// Original MIT implementations. Mathematical references/provenance are in
// docs/module-implementation.md; no third-party implementation is copied.
class SlotWah
{
public:
  void Reset(double rate)
  {
    mRate = rate;
    mControls.Reset(rate);
    mS1 = mS2 = 0.;
  }
  void Process(double* samples, int frames, const UtilityControls& controls)
  {
    mControls.Target(controls.wah, uWahPosition);
    for (int i = 0; i < frames; ++i)
    {
      const auto& c = mControls.Tick();
      // Heel/toe endpoints move together with Range; Position follows a log sweep.
      const double heel = 250. * mRange(c[2] * .01, [](double x) { return std::exp2(x); });
      const double hz = std::min(.35 * mRate, heel * mSweep(c[0] * .01, [](double x) { return std::pow(6.8, x); }));
      const double g = mWarp(grid_fx::pi * hz / mRate, [](double x) { return std::tan(x); }), k = 1. / c[1];
      const double x = samples[i];
      // Trapezoidal integration solves the two integrators' feedback together.
      const double band = (mS1 + g * (x - mS2)) / (1. + g * (g + k));
      const double low = mS2 + g * band;
      mS1 = grid_fx::Flush(2. * band - mS1);
      mS2 = grid_fx::Flush(2. * low - mS2);
      // A little broadband body avoids the thinness of an isolated band-pass.
      const double wet = .35 * x + 1.65 * k * band, mix = c[3] * .01;
      samples[i] = ((1. - mix) * x + mix * wet) * mOutput(c[4] * .05, [](double exponent) { return std::pow(10., exponent); });
    }
  }

private:
  grid_fx::SmoothControls<5> mControls;
  grid_fx::Memo mRange, mSweep, mWarp, mOutput;
  double mRate = 48000., mS1 = 0., mS2 = 0.;
};

// Dispersive spring tank after the parametric structure of Valimaki, Parker and Abel,
// "Parametric spring reverberation effect" (JAES 58(7/8), 2010); original implementation,
// no published code or calibration copied. Each of two springs runs a long cascade of
// first-order allpasses stretched by K samples inside a feedback loop. Below the spring's
// cutoff fs/(2K) (~4.4 kHz) its group delay rises with frequency, so every echo is a chirp:
// lows arrive first, highs later (the "drip"). A low-pass removes the cascade's images
// above the cutoff; a shorter plain-allpass loop adds the high-frequency splash.
class SpringChirp
{
public:
  static constexpr int kStages = 100, kSplashStages = 24;
  static constexpr double kCutoff = 4400.;
  static constexpr std::array<double, 2> kEcho{.0563, .0691}; // seconds per round trip
  void Reset(double rate)
  {
    mRate = rate;
    mStretch = std::max(1, static_cast<int>(std::lround(rate / (2. * kCutoff))));
    for (int s = 0; s < 2; ++s)
    {
      mCascade[s].assign(static_cast<size_t>(kStages * mStretch), 0.);
      mLoop[s].Reset(static_cast<int>(rate * kEcho[s]) + 4);
      mSplashLoop[s].Reset(static_cast<int>(rate * kEcho[s] * .9) + 4);
    }
    // RBJ low-pass at the cutoff (Q 0.707) for the chirp output.
    const double w = 2. * grid_fx::pi * std::min(kCutoff, .45 * rate) / rate, alpha = std::sin(w) / std::sqrt(2.);
    const double a0 = 1. + alpha;
    mB = {(1. - std::cos(w)) * .5 / a0, (1. - std::cos(w)) / a0, (1. - std::cos(w)) * .5 / a0};
    mA = {-2. * std::cos(w) / a0, (1. - alpha) / a0};
    mSplashHP = std::exp(-2. * grid_fx::pi * kCutoff / rate);
    Clear();
  }
  // Called on Reset and when the tank is switched in, so it never resumes a stale tail.
  void Clear()
  {
    for (auto& c : mCascade) std::fill(c.begin(), c.end(), 0.);
    for (auto& l : mLoop) l.Clear();
    for (auto& l : mSplashLoop) l.Clear();
    mSplash = {}; mTail = mSplashTail = mDamp = mSplashDamp = mHigh = {}; mBiquad = {}; mIndex = 0;
  }
  // Decay is seconds to -60 dB; drip 0..1 sets the chirp strength; damping is a one-pole coefficient.
  void Configure(double decay, double drip, double damping)
  {
    mA1 = .45 * drip; // 0: plain delays; the audible chirp is strongest near 0.45
    mDamping = damping;
    // Loop time: the echo plus the cascade's group delay at 1 kHz, so the 1 kHz band decays as set.
    const double w = 2. * grid_fx::pi * 1000. / mRate * mStretch;
    const double perStage = mStretch * (1. - mA1 * mA1) / (1. + mA1 * mA1 + 2. * mA1 * std::cos(w));
    for (int s = 0; s < 2; ++s)
    {
      mGain[s] = std::pow(.001, (kEcho[s] + kStages * perStage / mRate) / decay);
      mSplashGain[s] = std::pow(.001, (kEcho[s] * .9 + kSplashStages * 1.5 / mRate) / decay);
    }
  }
  // Mono excitation per spring; returns the two springs' outputs.
  std::array<double, 2> Tick(const std::array<double, 2>& input)
  {
    std::array<double, 2> out{};
    // Chirp loops: stretched allpasses w[n] = x - a w[n-K], y = a w[n] + w[n-K]. Each
    // stage waits for the previous one, so both springs' chains run interleaved.
    double x0 = input[0] + mGain[0] * mDamp[0], x1 = input[1] + mGain[1] * mDamp[1];
    double* w0 = mCascade[0].data() + mIndex;
    double* w1 = mCascade[1].data() + mIndex;
    for (int stage = 0, at = 0; stage < kStages; ++stage, at += mStretch)
    {
      const double old0 = w0[at], next0 = grid_fx::Flush(x0 - mA1 * old0);
      const double old1 = w1[at], next1 = grid_fx::Flush(x1 - mA1 * old1);
      x0 = mA1 * next0 + old0; w0[at] = next0;
      x1 = mA1 * next1 + old1; w1[at] = next1;
    }
    const std::array<double, 2> dispersed{x0, x1};
    for (int s = 0; s < 2; ++s)
    {
      mLoop[s].Push(dispersed[s]);
      mTail[s] = mLoop[s].Read(mRate * kEcho[s]);
      mDamp[s] = grid_fx::Flush(mTail[s] + mDamping * (mDamp[s] - mTail[s]));
      auto& b = mBiquad[s];
      const double chirp = mB[0] * mTail[s] + b[0];
      b[0] = grid_fx::Flush(mB[1] * mTail[s] - mA[0] * chirp + b[1]);
      b[1] = grid_fx::Flush(mB[2] * mTail[s] - mA[1] * chirp);
      // Splash loop: plain first-order allpasses, high-passed at the cutoff.
      double h = .5 * input[s] + mSplashGain[s] * mSplashDamp[s];
      for (auto& state : mSplash[s])
      {
        const double next = grid_fx::Flush(h + .6 * state);
        h = -.6 * next + state;
        state = next;
      }
      mSplashLoop[s].Push(h);
      mSplashTail[s] = mSplashLoop[s].Read(mRate * kEcho[s] * .9);
      mSplashDamp[s] = grid_fx::Flush(mSplashTail[s] + mDamping * (mSplashDamp[s] - mSplashTail[s]));
      mHigh[s] = grid_fx::Flush(mSplashTail[s] + mSplashHP * (mHigh[s] - mSplashTail[s]));
      out[s] = chirp + .35 * (mSplashTail[s] - mHigh[s]);
    }
    if (++mIndex == mStretch) mIndex = 0;
    return out;
  }

private:
  std::array<std::vector<double>, 2> mCascade;
  std::array<grid_fx::DelayLine, 2> mLoop, mSplashLoop;
  std::array<std::array<double, kSplashStages>, 2> mSplash{};
  std::array<std::array<double, 2>, 2> mBiquad{};
  std::array<double, 3> mB{};
  std::array<double, 2> mA{}, mGain{}, mSplashGain{}, mTail{}, mSplashTail{}, mDamp{}, mSplashDamp{}, mHigh{};
  double mRate = 48000., mA1 = .65, mDamping = 0., mSplashHP = 0.;
  int mStretch = 5, mIndex = 0;
};

// Three unequal waveguide-style tanks with stretched allpass dispersion in each
// feedback loop. Drip changes dispersion, not a generic reverb's modulation depth.
// This is a spring-inspired effect, not a calibrated model of a particular tank.
class SlotSpring
{
public:
  void Reset(double rate)
  {
    mMixLaw.Reset(rate);
    mRate = rate;
    mControls.Reset(rate);
    mTone.Reset(rate);
    mStereo.Reset(rate);
    mInputLow = {}; mPredelayControl.Reset(rate);
    for (auto& line : mPredelay) line.Reset(static_cast<int>(rate * .201));
    mDamp = {};
    mCounter = 0;
    mChirp.Reset(rate); mTankWeight = 0.; mFirstTank = true;
    for (int tank = 0; tank < 3; ++tank)
    {
      mTank[tank].Reset(static_cast<int>(.1 * rate));
      for (int section = 0; section < 16; ++section)
      {
        mDispersion[tank][section].Reset(static_cast<int>(.001 * rate));
        mStretch[tank][section] = std::max(1., rate * (.00037 + .000021 * section + .000013 * tank));
      }
    }
  }
  void Process(double* left, double* right, int frames, const UtilityControls& controls)
  {
    mControls.Target(controls.spring, uSpringDecay);
    mPredelayControl.Target({controls.extra.springPredelay}, uSpringPredelay);
    mTone.Target({controls.reverb[2]}, uReverbTone);
    mStereo.Target({controls.stereoReverb ? 1. : 0.}, uReverbStereo);
    constexpr std::array<double, 3> length{.0473, .0617, .0791};
    constexpr std::array<double, 3> pan{.85, .5, .15};
    const double hp = std::exp(-2. * grid_fx::pi * 130. / mRate);
    // Tank: Classic or Dispersive. A switch crossfades over 20 ms; the incoming engine
    // starts silent, and only the selected engine runs once the fade completes.
    const double tank = controls.extra.springTank >= .5 ? 1. : 0.;
    if (mFirstTank) { mTankWeight = tank; mFirstTank = false; }
    else if (tank == 1. && mTankWeight == 0.) mChirp.Clear();
    else if (tank == 0. && mTankWeight == 1.) ClearClassic();
    const double tankStep = 1. / (.02 * mRate);
    for (int i = 0; i < frames; ++i)
    {
      mMixLaw.Tick(controls.constantDry);
      const auto& c = mControls.Tick();
      const double tone = mTone.Tick()[0], width = right ? mStereo.Tick()[0] : 0.;
      if (mCounter++ % 32 == 0)
      {
        mDamping = std::exp(-2. * grid_fx::pi * std::min(tone, .4 * mRate) / mRate);
        for (int tank = 0; tank < 3; ++tank)
          mFeedback[tank] = std::pow(.001, (length[tank] + .009) / c[0]);
        mChirp.Configure(c[0], c[2] * .01, mDamping);
      }
      mCounter %= 32;
      const double dry[2]{left[i], right ? right[i] : left[i]};
      double send[2]{};
      const double predelay = mPredelayControl.Tick()[0];
      const double drive = mDrive(c[1] * .05, [](double x) { return std::pow(10., x); });
      for (int ch = 0; ch < 2; ++ch)
      {
        mInputLow[ch] = grid_fx::Flush(dry[ch] + hp * (mInputLow[ch] - dry[ch]));
        const double excited = std::tanh(drive * (dry[ch] - mInputLow[ch]));
        send[ch] = predelay > 0. ? mPredelay[ch].Read(predelay * .001 * mRate) : excited;
        mPredelay[ch].Push(excited);
      }
      mTankWeight += std::clamp(tank - mTankWeight, -tankStep, tankStep);
      double wetLeft = 0., wetRight = 0.;
      for (int tank = 0; tank < 3 && mTankWeight < 1.; ++tank)
      {
        const double tap = mTank[tank].Read(mRate * length[tank]);
        mDamp[tank] = grid_fx::Flush(tap + mDamping * (mDamp[tank] - tap));
        double wave = .4 * (pan[tank] * send[0] + (1. - pan[tank]) * send[1]) + mFeedback[tank] * mDamp[tank];
        for (int section = 0; section < 16; ++section)
        {
          auto& allpass = mDispersion[tank][section];
          const double a = .15 + .5 * c[2] * .01;
          const double delayed = allpass.Read(mStretch[tank][section]);
          const double dispersed = delayed - a * wave;
          allpass.Push(wave + a * dispersed);
          wave = dispersed;
        }
        mTank[tank].Push(wave);
        wetLeft += pan[tank] * tap;
        wetRight += (1. - pan[tank]) * tap;
      }
      if (mTankWeight > 0.)
      {
        constexpr std::array<double, 2> springPan{.8, .2};
        const auto springs = mChirp.Tick({kChirpInput * (springPan[0] * send[0] + (1. - springPan[0]) * send[1]),
                                          kChirpInput * (springPan[1] * send[0] + (1. - springPan[1]) * send[1])});
        const double chirpLeft = springPan[0] * springs[0] + springPan[1] * springs[1];
        const double chirpRight = (1. - springPan[0]) * springs[0] + (1. - springPan[1]) * springs[1];
        wetLeft += mTankWeight * (chirpLeft - wetLeft);
        wetRight += mTankWeight * (chirpRight - wetRight);
      }
      const double mid = .5 * (wetLeft + wetRight), side = .5 * (wetLeft - wetRight) * width;
      const double mix = c[3] * .01, output = mOutput(c[4] * .05, [](double x) { return std::pow(10., x); });
      left[i] = mMixLaw.Apply(((1. - mix) * dry[0] + mix * (mid + side)) * output, dry[0], mix, output);
      if (right)
        right[i] = mMixLaw.Apply(((1. - mix) * dry[1] + mix * (mid - side)) * output, dry[1], mix, output);
    }
  }

private:
  // Matches the Dispersive tank's wet level to Classic's on played material.
  static constexpr double kChirpInput = .45;
  void ClearClassic()
  {
    for (auto& line : mTank) line.Clear();
    for (auto& tank : mDispersion) for (auto& line : tank) line.Clear();
    mDamp = {};
  }
  SpringChirp mChirp;
  double mTankWeight = 0.;
  bool mFirstTank = true;
  grid_fx::MixLaw mMixLaw;
  grid_fx::SmoothControls<5> mControls;
  grid_fx::SmoothControls<1> mTone, mStereo;
  grid_fx::SmoothControls<1> mPredelayControl;
  std::array<grid_fx::DelayLine, 2> mPredelay;
  std::array<grid_fx::DelayLine, 3> mTank;
  std::array<std::array<grid_fx::DelayLine, 16>, 3> mDispersion;
  std::array<std::array<double, 16>, 3> mStretch{};
  std::array<double, 2> mInputLow{};
  std::array<double, 3> mDamp{}, mFeedback{};
  grid_fx::Memo mDrive, mOutput;
  double mRate = 48000., mDamping = 0.;
  int mCounter = 0;
};
