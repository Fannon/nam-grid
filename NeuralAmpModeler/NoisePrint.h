#pragma once
#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdint>

// Original MIT nam-grid experiment. Public product descriptions of multiband
// expanders informed the concept; no Klevgrand code or proprietary assets used.
namespace noise_print
{
inline constexpr int bands = 96;
struct Profile
{
  std::array<double, bands> power{};
  bool valid = false;
  bool operator==(const Profile&) const = default;
};
struct Controls
{
  bool learn = false;
  double sensitivity = 1.5, reduction = 24., attack = 2., release = 120., mix = 100.;
  double gateThreshold = -100.; // Minimum is Off, independent of the learned print.
};

// Stage-lifetime mailbox: audio is the sole writer. Control never touches an
// audio-owned Prepared object. Bounded reads fall back to the committed profile;
// epochs prevent a retired/replaced module from supplying another module's print.
class Snapshot
{
public:
  static_assert(std::atomic<double>::is_always_lock_free && std::atomic<uint64_t>::is_always_lock_free);
  void Publish(const Profile& profile, uint64_t epoch)
  {
    mSequence.fetch_add(1);
    mEpoch.store(epoch);
    mValid.store(profile.valid);
    for (int i = 0; i < bands; ++i)
      mPower[i].store(profile.power[i]);
    mSequence.fetch_add(1);
  }
  Profile Read(uint64_t epoch, const Profile& fallback) const
  {
    for (int attempt = 0; attempt < 3; ++attempt)
    {
      const auto sequence = mSequence.load();
      if (sequence & 1)
        continue;
      const auto actualEpoch = mEpoch.load();
      Profile profile;
      profile.valid = mValid.load();
      for (int i = 0; i < bands; ++i)
        profile.power[i] = mPower[i].load();
      if (sequence == mSequence.load())
        return actualEpoch == epoch ? profile : fallback;
    }
    return fallback;
  }
  uint64_t Sequence() const { return mSequence.load(); }

private:
  std::array<std::atomic<double>, bands> mPower{};
  std::atomic<uint64_t> mSequence{0}, mEpoch{0};
  std::atomic<bool> mValid{false};
};

class Processor
{
public:
  void Reset(double rate)
  {
    mRate = std::max(1., rate);
    mFilters = {};
    mPower = {};
    mMean = {};
    mGain.fill(1.);
    mSamples = mWarmup = 0;
    mLearning = false;
    mChanged = true;
    mReduction = 0.;
    mGateEnvelope = 0.;
    mGateGain = 1.;
    mGateHold = 0;
    mGateOpen = false;
    mGateFirst = true;
    constexpr double q = 4.;
    const double step = std::log(4000.) / (bands - 1);
    // The real response of a constant-Q bandpass integrates to
    // 2*atan(sqrt(4Q^2-1))/sqrt(4Q^2-1) over log frequency. Normalize
    // the dense logarithmic sum; synthesize only deviations from common gain.
    const double root = std::sqrt(4. * q * q - 1.);
    mSynthesis = step * root / (2. * std::atan(root));
    for (int i = 0; i < bands; ++i)
    {
      const double frequency = std::min(5. * std::exp(step * i), .45 * mRate);
      const double omega = 6.283185307179586 * frequency / mRate;
      const double alpha = omega / (2. * q);
      const double inverse = 1. / (1. + alpha);
      mCoefficient[i] = {alpha * inverse, -2. * std::cos(omega) * inverse, (1. - alpha) * inverse};
      mDetector[i] = std::exp(-1. / (mRate * std::max(.008, .5 / frequency)));
    }
  }
  void Restore(const Profile& profile)
  {
    mProfile = profile;
    mChanged = true;
  }
  const Profile& GetProfile() const { return mProfile; }
  bool TakeChanged()
  {
    const bool changed = mChanged;
    mChanged = false;
    return changed;
  }
  double Reduction() const { return mReduction; }

  void Process(double* left, double* right, int frames, const Controls& c)
  {
    if (c.learn && !mLearning)
    {
      mMean = {};
      mSamples = mWarmup = 0;
    }
    if (!c.learn && mLearning && mSamples >= static_cast<uint64_t>(mRate * .25))
    {
      double total = 0.;
      for (double power : mMean)
        total += power;
      // Reject digital silence/short captures, preserving a previous valid print.
      if (std::isfinite(total) && total > 1.e-18)
      {
        mProfile = {mMean, true};
        for (auto& power : mProfile.power)
          power = std::min(power, 1.e6);
        mChanged = true;
      }
    }
    mLearning = c.learn;
    const double margin = std::pow(10., std::clamp(c.sensitivity, -6., 12.) * .1);
    const double floor = std::pow(10., -std::clamp(c.reduction, 0., 48.) * .05);
    const double attack = std::exp(-1. / (.001 * std::clamp(c.attack, .2, 20.) * mRate));
    const double release = std::exp(-1. / (.001 * std::clamp(c.release, 20., 500.) * mRate));
    const double mixTarget = mProfile.valid && !c.learn && c.reduction > 0. ? std::clamp(c.mix, 0., 100.) * .01 : 0.;
    const double mixStep = 1. / (.005 * mRate);
    const double gateThreshold = std::clamp(c.gateThreshold, -100., 0.);
    const bool gateEnabled = !c.learn && gateThreshold > -100.;
    const double gateLevel = std::pow(10., gateThreshold * .05);
    const double gateDetectorRelease = std::exp(-1. / (.005 * mRate));
    const double gateOpenStep = 1. / (.0005 * mRate);
    const double gateCloseStep = 1. / (.001 * std::clamp(c.release, 20., 500.) * mRate);
    const auto gateHoldSamples = static_cast<uint64_t>(.020 * mRate);
    double learnedPower = 0.;
    for (double power : mProfile.power)
      learnedPower += power;
    double minimumGain = 1.;
    for (int sample = 0; sample < frames; ++sample)
    {
      if (mGateFirst)
      {
        // An enabled gate starts closed on recall/reset, avoiding a noise burst.
        mGateGain = gateEnabled ? 0. : 1.;
        mGateFirst = false;
      }
      const double dry[2]{Safe(left[sample]), right ? Safe(right[sample]) : 0.};
      std::array<std::array<double, bands>, 2> split{};
      for (int channel = 0; channel < (right ? 2 : 1); ++channel)
      {
        for (int i = 0; i < bands; ++i)
        {
          auto& state = mFilters[channel][i];
          const auto& coefficient = mCoefficient[i];
          const double value = coefficient.b0 * dry[channel] + state[0];
          state[0] = Flush(-coefficient.a1 * value + state[1]);
          state[1] = Flush(-coefficient.b0 * dry[channel] - coefficient.a2 * value);
          split[channel][i] = value;
        }
      }
      const bool collecting = c.learn && ++mWarmup > static_cast<uint64_t>(mRate * .05);
      if (collecting)
        ++mSamples;
      double wet[2]{}, bandSum[2]{}, reference = 1., gainSum = 0., totalPower = 0.;
      for (int i = 0; i < bands; ++i)
      {
        const double power =
          right ? .5 * (split[0][i] * split[0][i] + split[1][i] * split[1][i]) : split[0][i] * split[0][i];
        mPower[i] = Flush(power + mDetector[i] * (mPower[i] - power));
        if (collecting)
          mMean[i] += (power - mMean[i]) / static_cast<double>(mSamples);
        totalPower += mPower[i];
      }
      // Strong notes need less spectral attenuation. The resonator
      // bank overlaps: limiting reduction during confident signal prevents
      // low-level band leakage from coloring the wanted harmonics.
      const double confidence = std::max(0., totalPower / std::max(1.e-30, learnedPower) - 4.);
      const double protection = .5 * confidence / (confidence + 8.);
      for (int i = 0; i < bands; ++i)
      {
        // Soft expander knee: fully open confident bands rather than
        // continuously shaving wanted harmonics with a Wiener-style mask.
        const double ratio = mPower[i] / std::max(1.e-30, mProfile.power[i] * margin);
        const double knee = std::clamp(ratio - 1., 0., 1.);
        const double target =
          !mProfile.valid || c.learn ? 1. : std::max(protection, floor + (1. - floor) * knee * knee * (3. - 2. * knee));
        const double coefficient = target > mGain[i] ? attack : release;
        mGain[i] = target + coefficient * (mGain[i] - target);
        gainSum += mGain[i];
        reference = std::min(reference, mGain[i]);
        bandSum[0] += split[0][i];
        if (right)
          bandSum[1] += split[1][i];
        wet[0] += split[0][i] * mGain[i];
        if (right)
          wet[1] += split[1][i] * mGain[i];
      }
      // Equal gains reconstruct the scaled input exactly. Unequal gains
      // use the normalized resonator sum and can alter frequency/phase response.
      for (int channel = 0; channel < (right ? 2 : 1); ++channel)
        wet[channel] = reference * dry[channel] + mSynthesis * (wet[channel] - reference * bandSum[channel]);
      mMix += std::clamp(mixTarget - mMix, -mixStep, mixStep);
      // Exact dry/untrained/learning paths: no crossover coloration or buffering.
      const bool neutral = !mProfile.valid || mMix == 0.;
      // Apply the broadband gate after the spectral mix. Detect the original
      // input so changing sensitivity/mix cannot move its threshold. Max L/R
      // detection and one gain preserve stereo placement, including anti-phase.
      const double peak = std::max(std::abs(dry[0]), std::abs(dry[1]));
      mGateEnvelope = std::max(peak, Flush(mGateEnvelope * gateDetectorRelease));
      if (!gateEnabled || mGateEnvelope >= gateLevel)
      {
        mGateOpen = true;
        mGateHold = gateHoldSamples;
      }
      else if (mGateOpen)
      {
        if (mGateEnvelope >= gateLevel * .7079457843841379) // 3 dB hysteresis.
          mGateHold = gateHoldSamples;
        else if (mGateHold)
          --mGateHold;
        else
          mGateOpen = false;
      }
      const double gateTarget = mGateOpen ? 1. : 0.;
      mGateGain += std::clamp(gateTarget - mGateGain, -gateCloseStep, gateOpenStep);
      left[sample] = (neutral ? dry[0] : dry[0] + mMix * (wet[0] - dry[0])) * mGateGain;
      if (right)
        right[sample] = (neutral ? dry[1] : dry[1] + mMix * (wet[1] - dry[1])) * mGateGain;
      minimumGain = std::min(minimumGain, gainSum / bands * mGateGain);
    }
    mReduction = -20. * std::log10(std::max(1.e-6, minimumGain));
  }

private:
  static double Safe(double x) { return std::isfinite(x) ? std::clamp(x, -1000., 1000.) : 0.; }
  static double Flush(double x) { return std::abs(x) < 1.e-30 ? 0. : x; }
  std::array<std::array<std::array<double, 2>, bands>, 2> mFilters{};
  struct Coefficient
  { double b0, a1, a2; };
  std::array<Coefficient, bands> mCoefficient{};
  std::array<double, bands> mDetector{}, mPower{}, mMean{}, mGain{};
  Profile mProfile;
  uint64_t mSamples = 0, mWarmup = 0;
  double mRate = 48000., mMix = 1., mReduction = 0., mSynthesis = 1.;
  double mGateEnvelope = 0., mGateGain = 1.;
  uint64_t mGateHold = 0;
  bool mGateOpen = false, mGateFirst = true;
  bool mLearning = false, mChanged = true;
};
} // namespace noise_print
