#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <vector>
#include "../EffectModes.h"
#include "GranularReadFilter.h"
#include "reverb/ReverbCores.h"

// Granular texture processor with playing modes. The grain engine follows the granular
// mode of Emilie Gillet's Mutable Instruments Clouds (MIT; also packaged in the Parasites
// firmware and the Overcast MPC plugin, both MIT): a record buffer, grains placed so the
// play and record heads never cross, overlap-based scheduling, feedback and a plate.
// Per-grain randomness follows Beads (Clouds' successor). The modes and the shared
// Activity / Shape / Filter / Repeats / Space controls are inspired by the Hologram
// Microcosm pedal and Lightlike's Grainbloom (MIT). Independent double-precision
// implementation; see docs/module-implementation.md and ThirdPartyNotices.txt.
namespace grid_granular
{
using Stereo = std::array<double, 2>;
using Mode = effect_modes::GranulatorMode;
inline constexpr double kBufferSeconds = 4.;
inline constexpr int kMaxGrains = 32;
inline constexpr int kBurstGrains = 3;
inline constexpr double kMaxLoop = .95; // Repeats gain at 100%; the loop gain is its square.
inline constexpr int kGuard = 8;        // Samples kept between grain reads and the record head.

// Deterministic, so a settled rig renders identically for the same input.
class Random
{
public:
  void Seed(uint32_t seed) { mState = seed ? seed : 0x9e3779b9u; }
  double Uniform()
  {
    mState ^= mState << 13; mState ^= mState >> 17; mState ^= mState << 5;
    return static_cast<double>(mState) * (1. / 4294967296.);
  }
private:
  uint32_t mState = 0x9e3779b9u;
};

// What a mode does with the shared controls. Intervals are octaves and fifths only, so
// pitched grains suit any key.
struct Voicing
{
  bool regular = false;                      // Evenly spaced grains instead of random times.
  double minOverlap = .15, maxOverlap = 24.; // Activity 0% and 100%: grains sounding at once.
  double sizeScale = 1.;                     // Multiplies Size.
  double scatter = 0.;                       // Seconds of random extra reach back.
  double reach = 0.;                         // Share of the remaining buffer reached at random.
  double sizeRandom = 0.;                    // Octaves of random grain length, either way.
  double drift = 0.;                         // Cents of random detune, either way.
  double reverse = 0.;                       // Chance a grain plays backwards.
  double harmony = 0.;                       // Chance a grain takes one of the intervals.
  std::array<double, 8> intervals{};
  int intervalCount = 0;
  bool sequence = false;                     // Step through the intervals in order instead.
  double spread = .6;                        // Random pan width.
  double diffusion = 0.;                     // Allpass wash on the grain sum.
  double lowCut = 20., highCut = 20000.;
  double spaceRepeats = 0.;                  // Share of the Space plate that Repeats records.
};
inline Voicing Voice(int mode)
{
  Voicing v;
  switch (static_cast<Mode>(std::clamp(mode, 0, effect_modes::granulatorCount - 1)))
  {
    case Mode::Cloud:
      v.scatter = .05; v.sizeRandom = .15; v.drift = 3.; v.diffusion = .15; break;
    case Mode::Haze:
      v.minOverlap = 2.; v.maxOverlap = 28.; v.sizeScale = 1.6; v.scatter = .4; v.reach = .6; v.sizeRandom = .5;
      v.drift = 14.; v.reverse = .1; v.harmony = .15; v.intervals = {12., -12.}; v.intervalCount = 2;
      v.spread = .9; v.diffusion = .6; v.lowCut = 60.; v.highCut = 9000.; break;
    case Mode::Shimmer:
      // Octave grains only: Repeats records the plate as well, so each pass through
      // the buffer shifts the tail up again and the layers climb through the space.
      v.minOverlap = 2.; v.maxOverlap = 20.; v.sizeScale = 1.6; v.scatter = .1; v.sizeRandom = .25; v.drift = 4.;
      v.harmony = .6; v.intervals = {12.}; v.intervalCount = 1;
      v.spread = .85; v.diffusion = .45; v.lowCut = 150.; v.highCut = 8000.; v.spaceRepeats = 1.; break;
    case Mode::Tunnel:
      v.minOverlap = 2.; v.maxOverlap = 20.; v.sizeScale = 2.; v.scatter = .2; v.sizeRandom = .3; v.drift = 6.;
      v.harmony = .7; v.intervals = {-12.}; v.intervalCount = 1;
      v.spread = .7; v.diffusion = .55; v.lowCut = 30.; v.highCut = 5000.; break;
    case Mode::Mosaic:
      v.regular = true; v.minOverlap = .5; v.maxOverlap = 6.; v.scatter = .3; v.sizeRandom = .4; v.reverse = .35;
      v.harmony = .5; v.intervals = {-12., 12.}; v.intervalCount = 2; v.spread = .8; v.diffusion = .1; break;
    case Mode::Reverse:
      v.minOverlap = .4; v.maxOverlap = 8.; v.sizeScale = 1.5; v.scatter = .08; v.sizeRandom = .2; v.drift = 2.;
      v.reverse = 1.; v.diffusion = .2; break;
    case Mode::Blocks:
      v.regular = true; v.minOverlap = .25; v.maxOverlap = 2.5; v.spread = .3; break;
    case Mode::Interrupt:
      v.regular = true; v.minOverlap = .25; v.maxOverlap = 3.; v.scatter = 1.; v.sizeRandom = .6; v.reverse = .3;
      v.harmony = .25; v.intervals = {-12., 12.}; v.intervalCount = 2; v.spread = .8; break;
    case Mode::Arp:
      v.regular = true; v.minOverlap = .3; v.maxOverlap = 2.5; v.scatter = .02;
      v.intervals = {0., 7., 12., 19., 24., 19., 12., 7.}; v.intervalCount = 8; v.sequence = true; v.spread = .7; break;
    case Mode::PickBurst:
      v.regular = true; v.maxOverlap = 3.; v.spread = .45; break;
    case Mode::Count: break;
  }
  return v;
}
// Grains sounding at once for Activity 0..1.
inline double Overlap(const Voicing& v, double activity)
{
  const double a = std::clamp(activity, 0., 1.);
  return v.minOverlap + (v.maxOverlap - v.minOverlap) * a * a;
}
// How far the mode's grains drift apart: 0 when they all read the same audio in phase.
inline double Decorrelation(const Voicing& v)
{
  return std::clamp(std::max({v.scatter * 20., v.reach * 2., v.sizeRandom * 4., v.drift * .1, v.reverse * 2.,
                              v.harmony * 2., v.sequence ? 1. : 0.}), 0., 1.);
}
// sin^2(pi t / 2), t in 0..1: each half of the window.
inline const std::array<double, 257> kHann = [] {
  std::array<double, 257> table{};
  for (size_t i = 0; i < table.size(); ++i)
  {
    const double s = std::sin(1.5707963267948966 * static_cast<double>(i) / 256.);
    table[i] = s * s;
  }
  return table;
}();
// Shape 0..1 moves the window's peak from 3% (percussive) through 50% (Hann) to 97% (swell).
inline double Peak(double shape) { return .03 + .94 * std::clamp(shape, 0., 1.); }
inline double Window(double phase, double peak)
{
  const double t = phase < peak ? phase / peak : (1. - phase) / (1. - peak);
  const double x = std::clamp(t, 0., 1.) * 256.;
  const int i = std::min(255, static_cast<int>(x));
  return kHann[i] + (x - i) * (kHann[i + 1] - kHann[i]);
}
// Gain for a channel whose overlapping windows (times pan gains) sum to `sum`. Grains that
// read the same audio in phase add in amplitude (exponent 1); grains that drift apart add
// closer to power (exponent 1/2). Single, sparse grains pass at unity.
inline double Normalization(double sum, double decorrelation)
{
  if (sum <= 1.) return 1.;
  return std::exp(-(1. - .5 * std::clamp(decorrelation, 0., 1.)) * std::log(sum));
}
// Grain length in samples after the limit that keeps the play and record heads apart:
// a grain at ratio r reads r times its length from the buffer.
inline double GrainFrames(double milliseconds, double ratio, double rate, double bufferFrames)
{
  const double limit = .95 * (bufferFrames - 4. * kGuard) / (std::max(1., ratio) + 1.);
  return std::clamp(milliseconds * .001 * rate, 16., limit);
}

// A pick restarts the three pulses. Grains already sounding keep their envelopes.
class BurstClock
{
public:
  bool Tick(bool attack, double increment)
  {
    if (attack) { mRemaining = kBurstGrains; mPhase = 1.; }
    if (!mRemaining) return false;
    const bool due = mPhase >= 1.;
    if (due) { --mRemaining; mPhase -= 1.; }
    mPhase += increment;
    return due;
  }
private:
  int mRemaining = 0;
  double mPhase = 0.;
};

// Detect attacks against the recent input level, including while recording is frozen.
class PickDetector
{
public:
  void Reset(double rate)
  {
    mRelease = std::exp(-1. / (.015 * rate));
    mAverage = 1. - std::exp(-1. / (.04 * rate));
    mHold = static_cast<int>(.075 * rate);
    Clear();
  }
  void Clear(bool settling = false) { mPeak = mLevel = 0.; mCooldown = settling ? mHold : 0; mArmed = !settling; }
  bool Tick(double input)
  {
    mPeak = grid_fx::Flush(std::max(input, mPeak * mRelease));
    mLevel = grid_fx::Flush(mLevel + mAverage * (mPeak - mLevel));
    if (mCooldown > 0) --mCooldown;
    if (mPeak < 1.2 * mLevel + .0005) mArmed = true;
    if (!mArmed || mCooldown || mPeak <= 1.8 * mLevel + .002) return false;
    mArmed = false; mCooldown = mHold;
    return true;
  }
private:
  double mPeak = 0., mLevel = 0., mRelease = 0., mAverage = 0.;
  int mHold = 0, mCooldown = 0;
  bool mArmed = true;
};

class Scheduler
{
public:
  void Reset(uint32_t seed) { mRandom.Seed(seed); mPhase = 0.; mClockPhase = 1.; }
  // True when a grain should start this sample. Call Started() when one was launched.
  bool Tick(double overlap, bool regular, double frames, int active, double clockIncrement)
  {
    if (clockIncrement > 0.)
    {
      const bool due = mClockPhase >= 1.;
      if (due) mClockPhase -= 1.;
      mClockPhase += clockIncrement;
      mPhase = 0.;
      return due;
    }
    mClockPhase = 1.;
    if (overlap <= 0.) { mPhase = 0.; return false; }
    if (regular)
    {
      mPhase += 1.;
      return mPhase >= frames / overlap;
    }
    mPhase = 0.;
    return active < overlap && mRandom.Uniform() * frames < overlap;
  }
  void Started() { mPhase = 0.; }
  Random& Dice() { return mRandom; }
private:
  Random mRandom;
  double mPhase = 0., mClockPhase = 1.;
};
// One grain's decisions. Shared by the processor and the editor's display.
struct GrainChoice
{
  double pitch = 0., sizeFactor = 1., reachU = 0., jitterU = 0., pan = .5;
  bool reverse = false;
};
inline GrainChoice Choose(const Voicing& v, Random& r, int& step)
{
  GrainChoice c;
  double interval = 0.;
  if (v.sequence && v.intervalCount) interval = v.intervals[static_cast<size_t>(step++ % v.intervalCount)];
  else if (v.intervalCount && r.Uniform() < v.harmony)
    interval = v.intervals[static_cast<size_t>(std::min(v.intervalCount - 1, static_cast<int>(r.Uniform() * v.intervalCount)))];
  c.pitch = interval + v.drift * .01 * (2. * r.Uniform() - 1.);
  c.sizeFactor = std::exp2(v.sizeRandom * (2. * r.Uniform() - 1.));
  c.reachU = r.Uniform();
  c.jitterU = r.Uniform();
  c.reverse = r.Uniform() < v.reverse;
  c.pan = .5 + v.spread * (r.Uniform() - .5);
  return c;
}

struct Settings
{
  int mode = 0;
  double position = .15, size = 180., activity = .5, shape = .5, filter = 0., repeats = .2, space = .3;
  double periodSeconds = 0.; // Zero selects the mode's free-running scheduler.
  bool IsBurst() const { return mode == static_cast<int>(Mode::PickBurst); }
  double BurstPeriod() const
  {
    const double slow = 1. - std::clamp(activity, 0., 1.);
    return periodSeconds > 0. ? periodSeconds : .04 + .36 * slow * slow;
  }
  double ClockIncrement(double rate) const { return periodSeconds > 0. ? 1. / std::max(1., periodSeconds * rate) : 0.; }
};

// Editor sketch: the grains the scheduler would start over `seconds`, from a fixed seed.
// It runs the processor's scheduler and choices at a reduced rate.
struct PreviewGrain
{
  double start = 0., length = 0., pan = .5, pitch = 0.;
  bool reverse = false;
};
// One gesture relative to its first grain. The editor can draw even a whole-note
// cycle at slow tempo without running the sample clock for a minute of audio.
inline std::vector<PreviewGrain> PreviewPhrase(const Settings& s)
{
  const auto v = Voice(s.mode);
  const double gap = s.IsBurst() ? s.BurstPeriod() : s.periodSeconds > 0. ? s.periodSeconds
                                      : s.size * .001 / Overlap(v, s.activity);
  std::vector<PreviewGrain> grains;
  for (int i = 0; i < (s.IsBurst() ? kBurstGrains : v.intervalCount); ++i)
  {
    const double pitch = s.IsBurst() ? 0. : v.intervals[i];
    constexpr double rate = 48000.;
    const double length = GrainFrames(s.size, std::exp2(pitch / 12.), rate, kBufferSeconds * rate + 4. * kGuard);
    grains.push_back({i * gap, length / rate, .5, pitch, false});
  }
  return grains;
}
inline std::vector<PreviewGrain> PreviewCloud(const Settings& s, double seconds)
{
  constexpr double rate = 4000.;
  const auto voicing = Voice(s.mode);
  const double overlap = Overlap(voicing, s.activity), buffer = kBufferSeconds * rate + 4. * kGuard;
  const double hint = GrainFrames(s.size * voicing.sizeScale, 1., rate, buffer);
  const double clockIncrement = s.ClockIncrement(rate);
  std::vector<PreviewGrain> grains;
  std::vector<double> ends;
  Scheduler scheduler;
  scheduler.Reset(0x6a09e667u);
  int step = 0;
  const int total = static_cast<int>(seconds * rate);
  for (int t = 0; t < total; ++t)
  {
    std::erase_if(ends, [&](double end) { return end <= t; });
    const bool due = scheduler.Tick(overlap, voicing.regular, hint, static_cast<int>(ends.size()), clockIncrement);
    if (!due || ends.size() >= static_cast<size_t>(kMaxGrains))
      continue;
    scheduler.Started();
    const auto c = Choose(voicing, scheduler.Dice(), step);
    const double frames = GrainFrames(s.size * voicing.sizeScale * c.sizeFactor, std::exp2(c.pitch / 12.), rate, buffer);
    grains.push_back({t / rate, frames / rate, c.pan, c.pitch, c.reverse});
    ends.push_back(t + frames);
  }
  return grains;
}
// Seconds before the record head that grains read from at Position (newest, oldest), and
// how far back the mode's random reach can take them.
inline std::array<double, 3> ReadSpan(const Settings& s)
{
  const auto v = Voice(s.mode);
  const double rate = 48000., buffer = kBufferSeconds * rate + 4. * kGuard;
  const double frames = GrainFrames(s.size * v.sizeScale, 1., rate, buffer);
  const double available = std::max(0., buffer - 4. * kGuard - 2. * frames);
  const double offset = std::clamp(s.position, 0., 1.) * available, remaining = available - offset;
  const double extra = std::min(remaining, v.scatter * rate + v.reach * remaining);
  return {offset / rate, (offset + frames) / rate, (offset + frames + extra) / rate};
}

class Engine
{
public:
  void Reset(double rate)
  {
    mRate = std::max(1., rate);
    mFrames = static_cast<int>(std::ceil(kBufferSeconds * mRate)) + 4 * kGuard;
    static const ReadFilter readFilter;
    mReadFilter = &readFilter;
    for (auto& channel : mBuffer) channel.assign(static_cast<size_t>(mFrames) + ReadFilter::taps - 1, 0.f);
    mWrite = 0;
    mGrains = {};
    mActive = 0;
    mStep = 0;
    mScheduler.Reset(0x6a09e667u);
    mProcessed = false;
    mDetector.Reset(mRate);
    mBurst = {};
    mGain = {1., 1.};
    mGainCoefficient = std::exp(-1. / (.03 * mRate));
    mMixCoefficient = std::exp(-1. / (.01 * mRate));
    mFeedback = {}; mFeedbackHP = {}; mLowPass = {}; mHighPass = {};
    for (int ch = 0; ch < 2; ++ch)
      for (int i = 0; i < 4; ++i) mDiffuser[ch][i].Reset(mRate, kDiffusion[ch][i] + .002);
    mReverb.Reset(mRate);
    mFirst = true;
    Configure({});
  }
  // Called every few samples from smoothed controls; Tick reads the cached values.
  void Configure(const Settings& s)
  {
    if (s.IsBurst() && !mSettings.IsBurst()) { mDetector.Clear(mProcessed); mBurst = {}; }
    mSettings = s;
    mVoicing = Voice(s.mode);
    mOverlap = Overlap(mVoicing, s.activity);
    mHintFrames = GrainFrames(s.size * mVoicing.sizeScale, 1., mRate, mFrames);
    mClockIncrement = s.IsBurst() ? 1. / (s.BurstPeriod() * mRate) : s.ClockIncrement(mRate);
    mDecorrelation = Decorrelation(mVoicing);
    mPeak = Peak(s.shape);
    const double f = std::clamp(s.repeats, 0., 1.);
    mFeedbackGain = kMaxLoop * f * (2. - f);
    mFeedbackCoefficient = 1. / (1. + 2. * grid_fx::pi * (20. + 100. * f * f) / mRate);
    // Filter: left of center lowers a low-pass to 200 Hz, right raises a high-pass to 4 kHz.
    const double knob = std::clamp(s.filter, -1., 1.), top = std::min(20000., .45 * mRate);
    const double lowPass = std::min({top, mVoicing.highCut, knob < 0. ? top * std::pow(200. / top, -knob) : top});
    const double highPass = std::max(mVoicing.lowCut, knob > 0. ? 20. * std::pow(200., knob) : 20.);
    // Glide the cutoffs in octaves, so mode changes do not step the filters.
    const double glide = mFirst ? 0. : .85;
    mLowPassHz = std::exp(std::log(lowPass) + glide * (std::log(mLowPassHz) - std::log(lowPass)));
    mHighPassHz = std::exp(std::log(highPass) + glide * (std::log(mHighPassHz) - std::log(highPass)));
    mLowPassG = std::tan(grid_fx::pi * mLowPassHz / mRate); mLowPassG /= 1. + mLowPassG;
    mHighPassG = std::tan(grid_fx::pi * mHighPassHz / mRate); mHighPassG /= 1. + mHighPassG;
    const double r = std::clamp(s.space, 0., 1.);
    mSpace = r;
    mReverb.Configure(1., .8 + 7.2 * r * r, 6500. + 3000. * f, .5);
    if (mFirst) { mDiffusion = mVoicing.diffusion; mSpaceMix = r; mSpaceRepeats = mVoicing.spaceRepeats; mFirst = false; }
  }
  // One stereo frame. `stereo` false reads and writes the left buffer only (mono in, mono out).
  Stereo Tick(Stereo in, bool stereo, bool frozen, double live)
  {
    mProcessed = true;
    const int channels = stereo ? 2 : 1;
    // Repeats: high-passed against DC build-up, then soft-limited with the input.
    const double g = mFeedbackGain * live;
    if (!frozen)
    {
      for (int ch = 0; ch < channels; ++ch)
      {
        double x = in[ch];
        if (g > 0.) x += g * (Saturate(x + g * mFeedback[ch]) - x);
        mBuffer[ch][static_cast<size_t>(mWrite)] = static_cast<float>(grid_fx::Flush(x));
        // Mirror the start of the ring so even a wrapped FIR read is contiguous.
        if (mWrite < ReadFilter::taps - 1) mBuffer[ch][mFrames + mWrite] = mBuffer[ch][mWrite];
      }
      if (++mWrite == mFrames) mWrite = 0;
    }
    const bool due = mSettings.IsBurst()
      ? mBurst.Tick(mDetector.Tick(std::max(std::abs(in[0]), stereo ? std::abs(in[1]) : 0.)), mClockIncrement)
      : mScheduler.Tick(mOverlap, mVoicing.regular, mHintFrames, mActive, mClockIncrement);
    if (due)
      if (Start()) mScheduler.Started();
    Stereo wet{}, sum{};
    int active = 0;
    for (auto& grain : mGrains)
    {
      if (!grain.active) continue;
      ++active;
      const double w = Window(grain.phase, grain.peak);
      Stereo sample{};
      if (grain.band)
      {
        const int center = static_cast<int>(grain.position);
        const int phase = static_cast<int>((grain.position - center) * ReadFilter::phases + .5);
        const auto& kernel = (*grain.band)[phase];
        int first = center - (ReadFilter::taps / 2 - 1);
        if (first < 0) first += mFrames;
        for (int ch = 0; ch < channels; ++ch)
          sample[ch] = ReadFilter::Read(mBuffer[ch].data() + first, kernel);
      }
      else
      {
        sample[0] = Read(0, grain.position);
        if (stereo) sample[1] = Read(1, grain.position);
      }
      if (stereo)
      {
        wet[0] += w * grain.gain[0] * sample[0]; wet[1] += w * grain.gain[1] * sample[1];
        sum[0] += w * grain.gain[0]; sum[1] += w * grain.gain[1];
      }
      else
      {
        const double gain = w * .5 * (grain.gain[0] + grain.gain[1]);
        wet[0] += gain * sample[0]; sum[0] += gain;
      }
      grain.position += grain.step;
      if (grain.position >= mFrames) grain.position -= mFrames;
      else if (grain.position < 0.) grain.position += mFrames;
      grain.phase += grain.phaseStep;
      if (grain.phase >= 1.) { grain.active = false; --active; }
    }
    mActive = active;
    // Smoothed, so the windows still shape the sound instead of being divided out.
    for (int ch = 0; ch < channels; ++ch)
    {
      const double target = Normalization(sum[ch], mDecorrelation);
      mGain[ch] = target + mGainCoefficient * (mGain[ch] - target);
      wet[ch] *= mGain[ch];
    }
    if (!stereo) wet[1] = wet[0];
    // Configure updates every 32 samples; glide the blend amounts per sample, since they
    // scale the difference between whole signals. The diffuser always runs: started from
    // silence under a sounding cloud, its first echoes would arrive as hard edges.
    mDiffusion = mVoicing.diffusion + mMixCoefficient * (mDiffusion - mVoicing.diffusion);
    for (int ch = 0; ch < 2; ++ch)
    {
      double x = wet[ch];
      for (int i = 0; i < 4; ++i) x = mDiffuser[ch][i].Tick(x, mRate * kDiffusion[ch][i], .625);
      x = wet[ch] + mDiffusion * (x - wet[ch]);
      // Filter: two-pole low- and high-pass (TPT one-poles), before Repeats and Space.
      for (int stage = 0; stage < 2; ++stage)
      {
        const double v = mLowPassG * (x - mLowPass[ch][stage]);
        const double low = v + mLowPass[ch][stage];
        mLowPass[ch][stage] = grid_fx::Flush(low + v);
        x = low;
      }
      for (int stage = 0; stage < 2; ++stage)
      {
        const double v = mHighPassG * (x - mHighPass[ch][stage]);
        const double low = v + mHighPass[ch][stage];
        mHighPass[ch][stage] = grid_fx::Flush(low + v);
        x -= low;
      }
      wet[ch] = x;
    }
    // The plate always runs, so its tail rings out when Space is turned down.
    const auto tail = mReverb.Tick({.35 * wet[0], .35 * wet[1]});
    const double r = mSpaceMix = mSpace + mMixCoefficient * (mSpaceMix - mSpace);
    const Stereo out{wet[0] * (1. - .3 * r) + 1.6 * r * tail[0], wet[1] * (1. - .3 * r) + 1.6 * r * tail[1]};
    mSpaceRepeats = mVoicing.spaceRepeats + mMixCoefficient * (mSpaceRepeats - mVoicing.spaceRepeats);
    for (int ch = 0; ch < 2; ++ch)
    {
      // Repeats path: two more one-pole high-passes (20-120 Hz with Repeats).
      double y = wet[ch] + mSpaceRepeats * (out[ch] - wet[ch]);
      for (int stage = 0; stage < 2; ++stage)
      {
        const double z = mFeedbackCoefficient * (mFeedbackHP[ch][stage][0] + y - mFeedbackHP[ch][stage][1]);
        mFeedbackHP[ch][stage][1] = y; mFeedbackHP[ch][stage][0] = grid_fx::Flush(z); y = z;
      }
      mFeedback[ch] = y;
    }
    return out;
  }
  int Active() const { return mActive; }

private:
  struct Grain
  {
    double position = 0., step = 1., phase = 0., phaseStep = 0., peak = .5;
    Stereo gain{};
    const ReadFilter::Band* band = nullptr;
    bool active = false;
  };
  // Rational tanh approximation with unity slope at zero, limited to +/-1.
  static double Saturate(double x)
  {
    if (x > 3.) return 1.;
    if (x < -3.) return -1.;
    const double x2 = x * x;
    return x * (27. + x2) / (27. + 9. * x2);
  }
  double Read(int ch, double position) const
  {
    const auto& b = mBuffer[ch];
    const int i = static_cast<int>(position);
    const double t = position - i;
    const auto at = [&](int k) {
      k += i;
      if (k < 0) k += mFrames; else if (k >= mFrames) k -= mFrames;
      return static_cast<double>(b[static_cast<size_t>(k)]);
    };
    const double xm = at(-1), x0 = at(0), x1 = at(1), x2 = at(2);
    const double c1 = .5 * (x1 - xm), c2 = xm - 2.5 * x0 + 2. * x1 - .5 * x2, c3 = .5 * (x2 - xm) + 1.5 * (x0 - x1);
    return ((c3 * t + c2) * t + c1) * t + x0;
  }
  bool Start()
  {
    Grain* free = nullptr;
    for (auto& grain : mGrains)
      if (!grain.active) { free = &grain; break; }
    if (!free) return false;
    const auto c = Choose(mVoicing, mScheduler.Dice(), mStep);
    const double ratio = std::exp2(std::clamp(c.pitch, -36., 36.) / 12.);
    const double frames = GrainFrames(std::clamp(mSettings.size * mVoicing.sizeScale * c.sizeFactor, 10., 3000.), ratio, mRate, mFrames);
    // The grain reads frames*ratio samples while the record head writes frames more;
    // Position spreads the start over what remains, newest material at 0%.
    const double played = frames * ratio;
    // Evenly spaced pitched grains read evenly spaced positions and partly cancel. A
    // random offset of up to a quarter grain, growing over the first semitone, keeps them
    // apart; unpitched grains read the same audio in phase and need none.
    free->band = mReadFilter->ForPitch(c.pitch);
    const int margin = free->band ? ReadFilter::taps / 2 : 0;
    const double room = std::max(0., mFrames - 4. * kGuard - 2. * margin - played - frames);
    const double jitter = std::min(room, .25 * frames * std::min(1., std::abs(c.pitch)));
    const double available = room - jitter;
    const double offset = std::clamp(mSettings.position, 0., 1.) * available, remaining = available - offset;
    const double extra = std::min(remaining, mVoicing.scatter * mRate + mVoicing.reach * remaining) * c.reachU;
    double start = mWrite - (offset + extra + jitter * c.jitterU + played + kGuard + margin);
    while (start < 0.) start += mFrames;
    free->gain = c.pan < .5 ? Stereo{1., 2. * c.pan} : Stereo{2. * (1. - c.pan), 1.};
    // A reversed grain reads the same span from its newest end, moving away from the head.
    free->position = c.reverse ? start + played : start;
    if (free->position >= mFrames) free->position -= mFrames;
    free->step = c.reverse ? -ratio : ratio;
    free->phase = 0.;
    free->phaseStep = 1. / frames;
    free->peak = mPeak;
    free->active = true;
    ++mActive;
    return true;
  }
  inline static constexpr double kDiffusion[2][4]{{.0040, .0057, .0084, .0139}, {.0047, .0064, .0072, .0120}};
  std::array<std::vector<float>, 2> mBuffer;
  const ReadFilter* mReadFilter = nullptr;
  std::array<Grain, kMaxGrains> mGrains{};
  std::array<std::array<grid_reverb::Allpass, 4>, 2> mDiffuser;
  grid_reverb::PlateTank mReverb;
  Scheduler mScheduler;
  PickDetector mDetector;
  BurstClock mBurst;
  Settings mSettings;
  Voicing mVoicing;
  Stereo mFeedback{}, mGain{1., 1.};
  std::array<std::array<std::array<double, 2>, 2>, 2> mFeedbackHP{};
  std::array<std::array<double, 2>, 2> mLowPass{}, mHighPass{};
  double mRate = 48000., mGainCoefficient = 0., mMixCoefficient = 0., mFeedbackGain = 0., mFeedbackCoefficient = 1.;
  double mOverlap = 0., mDecorrelation = 0., mPeak = .5, mSpace = 0., mSpaceMix = 0., mDiffusion = 0., mSpaceRepeats = 0.;
  double mHintFrames = 0., mClockIncrement = 0.;
  double mLowPassHz = 20000., mHighPassHz = 20., mLowPassG = 0., mHighPassG = 0.;
  int mFrames = 1, mWrite = 0, mActive = 0, mStep = 0;
  bool mFirst = true, mProcessed = false;
};
} // namespace grid_granular
