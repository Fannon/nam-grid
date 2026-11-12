#pragma once
#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <memory>
#include "FinalGate.h"
#include "NoisePrintCapture.h"
#include "NoisePrintTracking.h"
#if (defined(_M_X64) || defined(__SSE2__)) && !defined(NAM_NOISE_PRINT_SCALAR)
  #include <emmintrin.h>
#elif defined(__aarch64__) && !defined(NAM_NOISE_PRINT_SCALAR)
  #include <arm_neon.h>
#endif

// Original MIT NAM Grid experiment. Public product descriptions of multiband
// expanders informed the concept; no Klevgrand code or proprietary assets used.
namespace noise_print
{
inline constexpr int bands = 96;
static_assert(bands == RobustCapture::bands);
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
  double gateRelease = 40.; // Independent final-gate closing time.
  double protectAttack = 100.; // 0 restores the original opening response.
  int trackingMode = 0; // Fixed=0; Follow=1, saved selector.
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
    // Reset/preparation is outside processing. Other module types carry only a
    // null pointer; the bounded capture storage belongs to Smart Gate instances.
    if (!mCapture)
      mCapture = std::make_unique<RobustCapture>();
    mCapture->Reset(mRate);
    mOnsetFast = mOnsetSlow = 0.;
    mOnsetHold = 0;
    mOnsetFastPole = std::exp(-1. / (.0005 * mRate));
    mOnsetSlowPole = std::exp(-1. / (.020 * mRate));
    mTracking.Reset(mRate);
    mTracking.Reference(mProfile.power, mProfile.valid);
    mFilters = {};
    mPower = {};
    mMean = {};
    mGain.fill(1.);
    mSamples = mWarmup = 0;
    mLearning = false;
    mChanged = true;
    mReduction = 0.;
    mCleanPeak = 0.;
    mGate.Reset();
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
      mCoefficient.b0[i] = alpha * inverse;
      mCoefficient.a1[i] = -2. * std::cos(omega) * inverse;
      mCoefficient.a2[i] = (1. - alpha) * inverse;
      mDetector[i] = std::exp(-1. / (mRate * std::max(.008, .5 / frequency)));
    }
  }
  void Restore(const Profile& profile)
  {
    mProfile = profile;
    mTracking.Reference(mProfile.power, mProfile.valid);
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
  double CleanPeak() const { return mCleanPeak; }
  double TrackingScale() const { return mTracking.Scale(); }

  void Process(double* left, double* right, int frames, const Controls& c)
  {
    mCleanPeak = 0.;
    if (c.learn && !mLearning)
    {
      mMean = {};
      mCapture->Reset(mRate);
      mTracking.Clear();
      mSamples = mWarmup = 0;
    }
    if (!c.learn && mLearning && mSamples >= static_cast<uint64_t>(mRate * .25))
    {
      const auto captured = mCapture->Finish(mMean);
      double total = 0.;
      for (double power : captured)
        total += power;
      // Reject digital silence/short captures, preserving a previous valid print.
      if (std::isfinite(total) && total > 1.e-18)
      {
        mProfile = {captured, true};
        for (auto& power : mProfile.power)
          power = std::min(power, 1.e6);
        mTracking.Reference(mProfile.power, mProfile.valid);
        mChanged = true;
      }
    }
    mLearning = c.learn;
    const bool reducing = mProfile.valid && !c.learn;
    const bool following = reducing && c.trackingMode == 1;
    const double margin = std::pow(10., std::clamp(c.sensitivity, -6., 12.) * .1);
    const double floor = std::pow(10., -std::clamp(c.reduction, 0., 48.) * .05);
    const double attack = std::exp(-1. / (.001 * std::clamp(c.attack, .2, 20.) * mRate));
    const double onsetAmount = std::clamp(c.protectAttack, 0., 100.) * .01;
    const double onsetAttack = onsetAmount > 0. ? std::pow(attack, 1. + 3. * onsetAmount) : attack;
    const double release = std::exp(-1. / (.001 * std::clamp(c.release, 20., 500.) * mRate));
    const double mixTarget = mProfile.valid && !c.learn && c.reduction > 0. ? std::clamp(c.mix, 0., 100.) * .01 : 0.;
    const double mixStep = 1. / (.005 * mRate);
    const auto gate = final_gate::Prepare(c.gateThreshold, c.gateRelease, mRate, !c.learn);
    double learnedPower = 0.;
    std::array<double, bands> inverseThreshold;
    for (int i = 0; i < bands; ++i)
    {
      learnedPower += mProfile.power[i];
      if (reducing)
        inverseThreshold[i] = 1. / std::max(1.e-30, mProfile.power[i] * margin);
    }
    const double inverseLearnedPower = 1. / std::max(1.e-30, learnedPower);
    std::array<double, bands> followThreshold;
    double minimumGain = 1.;
    for (int sample = 0; sample < frames; ++sample)
    {
      const double dry[2]{Safe(left[sample]), right ? Safe(right[sample]) : 0.};
      const bool synthesize = mProfile.valid && (mMix > 0. || mixTarget > 0.);
      // Each used channel is fully written before reading. Contiguous band
      // arrays let the compiler vectorize independent resonators without
      // requiring AVX or changing the detector/gain update rate.
      Split split;
      const bool collecting = c.learn && ++mWarmup > static_cast<uint64_t>(mRate * .05);
      if (collecting)
        ++mSamples;
      const double inverseSamples = collecting ? 1. / static_cast<double>(mSamples) : 0.;
      const double totalPower = Analyze(dry, right != nullptr, collecting, inverseSamples, split);
      if (collecting)
        mCapture->Advance();
      if (onsetAmount > 0.)
      {
        const double power = right ? .5 * (dry[0] * dry[0] + dry[1] * dry[1]) : dry[0] * dry[0];
        mOnsetFast = power + mOnsetFastPole * (mOnsetFast - power);
        mOnsetSlow = power + mOnsetSlowPole * (mOnsetSlow - power);
        if (mOnsetFast > 3. * mOnsetSlow && mOnsetFast > 1.e-12)
          mOnsetHold = static_cast<int>(.008 * mRate);
        else if (mOnsetHold)
          --mOnsetHold;
      }
      // Strong notes need less spectral attenuation. The resonator
      // bank overlaps: limiting reduction during confident signal prevents
      // low-level band leakage from coloring the wanted harmonics.
      double scale = 1.;
      if (following || mTracking.Scale() != 1. || mTracking.Active())
        scale = mTracking.Tick(mPower, following);
      const bool adjusted = reducing && scale != 1.;
      const double inverseScale = adjusted ? 1. / scale : 1.;
      if (adjusted)
        for (int i = 0; i < bands; ++i) followThreshold[i] = inverseThreshold[i] * inverseScale;
      const double ratio = totalPower * inverseLearnedPower;
      const double confidence = reducing ? std::max(0., (adjusted ? ratio * inverseScale : ratio) - 4.) : 0.;
      const double protection = .5 * confidence / (confidence + 8.);
      // Brief opening assistance leaves the target gains and closing Release
      // untouched. Noise bursts can trigger it too, so the player controls it.
      const double selectedAttack = onsetAmount > 0. && mOnsetHold > 0 && confidence < 4. ? onsetAttack : attack;
      double wet[2]{};
      const double gainSum = Synthesize(
        dry, right != nullptr, reducing, synthesize, protection, floor, selectedAttack, release,
        adjusted ? followThreshold : inverseThreshold, split, wet);
      mMix += std::clamp(mixTarget - mMix, -mixStep, mixStep);
      // Exact dry/untrained/learning paths: no crossover coloration or buffering.
      const bool neutral = !mProfile.valid || mMix == 0.;
      // Both detector and gain follow the spectral mix. Detect before the gate's
      // own gain so a closed gate can reopen. Max L/R preserves anti-phase input.
      const double cleaned[2]{neutral ? dry[0] : dry[0] + mMix * (wet[0] - dry[0]),
                              right ? (neutral ? dry[1] : dry[1] + mMix * (wet[1] - dry[1])) : 0.};
      const double peak = std::max(std::abs(cleaned[0]), std::abs(cleaned[1]));
      mCleanPeak = std::max(mCleanPeak, peak);
      const double gateGain = mGate.Tick(peak, gate);
      left[sample] = cleaned[0] * gateGain;
      if (right)
        right[sample] = cleaned[1] * gateGain;
      minimumGain = std::min(minimumGain, (1. - mMix + mMix * gainSum / bands) * gateGain);
    }
    mReduction = -20. * std::log10(std::max(1.e-6, minimumGain));
  }

private:
  using Split = std::array<std::array<double, bands>, 2>;
#if (defined(_M_X64) || defined(__SSE2__)) && !defined(NAM_NOISE_PRINT_SCALAR)
  static_assert(bands % 2 == 0);
  // SSE2 is baseline on x64. No AVX, FMA, reduced precision or slower detector rate.
  static __m128d FlushPair(__m128d value)
  {
    const auto magnitude = _mm_andnot_pd(_mm_set1_pd(-0.), value);
    return _mm_andnot_pd(_mm_cmplt_pd(magnitude, _mm_set1_pd(1.e-30)), value);
  }
  static double SumPair(__m128d value) { return _mm_cvtsd_f64(value) + _mm_cvtsd_f64(_mm_unpackhi_pd(value, value)); }
#endif
  double Analyze(const double* dry, bool stereo, bool collecting, double inverseSamples, Split& split)
  {
#if (defined(_M_X64) || defined(__SSE2__)) && !defined(NAM_NOISE_PRINT_SCALAR)
    const auto inputL = _mm_set1_pd(dry[0]), inputR = _mm_set1_pd(dry[1]);
    for (int i = 0; i < bands; i += 2)
    {
      const auto b0 = _mm_loadu_pd(&mCoefficient.b0[i]);
      const auto a1 = _mm_loadu_pd(&mCoefficient.a1[i]), a2 = _mm_loadu_pd(&mCoefficient.a2[i]);
      const auto negative = _mm_set1_pd(-0.);
      const auto tick = [&](int channel, __m128d input) {
        auto& state = mFilters[channel];
        const auto excitation = _mm_mul_pd(b0, input);
        const auto value = _mm_add_pd(excitation, _mm_loadu_pd(&state.first[i]));
        _mm_storeu_pd(&state.first[i], FlushPair(_mm_sub_pd(_mm_loadu_pd(&state.second[i]), _mm_mul_pd(a1, value))));
        _mm_storeu_pd(&state.second[i], FlushPair(_mm_sub_pd(_mm_xor_pd(excitation, negative), _mm_mul_pd(a2, value))));
        _mm_storeu_pd(&split[channel][i], value);
        return value;
      };
      const auto left = tick(0, inputL);
      auto power = _mm_mul_pd(left, left);
      if (stereo)
      {
        const auto right = tick(1, inputR);
        power = _mm_mul_pd(_mm_set1_pd(.5), _mm_add_pd(power, _mm_mul_pd(right, right)));
      }
      const auto previous = _mm_loadu_pd(&mPower[i]);
      _mm_storeu_pd(
        &mPower[i], FlushPair(_mm_add_pd(power, _mm_mul_pd(_mm_loadu_pd(&mDetector[i]), _mm_sub_pd(previous, power)))));
      if (collecting)
      {
        const auto mean = _mm_loadu_pd(&mMean[i]);
        _mm_storeu_pd(&mMean[i], _mm_add_pd(mean, _mm_mul_pd(_mm_sub_pd(power, mean), _mm_set1_pd(inverseSamples))));
        mCapture->Accumulate(i, power);
      }
    }
#else
    for (int channel = 0; channel < (stereo ? 2 : 1); ++channel)
      for (int i = 0; i < bands; ++i)
      {
        auto& state = mFilters[channel];
        const double value = mCoefficient.b0[i] * dry[channel] + state.first[i];
        state.first[i] = Flush(-mCoefficient.a1[i] * value + state.second[i]);
        state.second[i] = Flush(-mCoefficient.b0[i] * dry[channel] - mCoefficient.a2[i] * value);
        split[channel][i] = value;
      }
    for (int i = 0; i < bands; ++i)
    {
      const double power =
        stereo ? .5 * (split[0][i] * split[0][i] + split[1][i] * split[1][i]) : split[0][i] * split[0][i];
      mPower[i] = Flush(power + mDetector[i] * (mPower[i] - power));
      if (collecting)
      {
        mMean[i] += (power - mMean[i]) * inverseSamples;
        mCapture->Accumulate(i, power);
      }
    }
#endif
    // Preserve the learned-power reduction order on both implementations.
    double total = 0.;
    for (double power : mPower)
      total += power;
    return total;
  }
  double Synthesize(const double* dry, bool stereo, bool reducing, bool synthesize, double protection, double floor,
                    double attack, double release, const std::array<double, bands>& inverseThreshold,
                    const Split& split, double* wet)
  {
    double reference = 1., gainSum = 0., bandSum[2]{};
    const double fastAttack = attack * attack * attack * attack;
#if (defined(_M_X64) || defined(__SSE2__)) && !defined(NAM_NOISE_PRINT_SCALAR)
    const auto one = _mm_set1_pd(1.), zero = _mm_setzero_pd();
    auto gains = zero, minimum = one, sumL = zero, sumR = zero, wetL = zero, wetR = zero;
    for (int i = 0; i < bands; i += 2)
    {
      auto target = one;
      auto openingAttack = _mm_set1_pd(attack);
      if (reducing)
      {
        const auto ratio = _mm_mul_pd(_mm_loadu_pd(&mPower[i]), _mm_loadu_pd(&inverseThreshold[i]));
        // Bands well above the learned floor open faster, including abrupt noise.
        // Near the learned floor keep the selected Attack and suppression curve.
        const auto evidence = _mm_min_pd(one, _mm_max_pd(zero, _mm_mul_pd(_mm_sub_pd(ratio, _mm_set1_pd(4.)), _mm_set1_pd(1. / 12.))));
        openingAttack = _mm_add_pd(openingAttack, _mm_mul_pd(evidence, _mm_set1_pd(fastAttack - attack)));
        const auto knee = _mm_min_pd(one, _mm_max_pd(zero, _mm_sub_pd(ratio, one)));
        auto curve = _mm_mul_pd(_mm_mul_pd(_mm_set1_pd(1. - floor), knee), knee);
        curve = _mm_mul_pd(curve, _mm_sub_pd(_mm_set1_pd(3.), _mm_mul_pd(_mm_set1_pd(2.), knee)));
        target = _mm_max_pd(_mm_set1_pd(protection), _mm_add_pd(_mm_set1_pd(floor), curve));
      }
      const auto previous = _mm_loadu_pd(&mGain[i]);
      const auto opening = _mm_cmpgt_pd(target, previous);
      const auto coefficient =
        _mm_or_pd(_mm_and_pd(opening, openingAttack), _mm_andnot_pd(opening, _mm_set1_pd(release)));
      const auto gain = _mm_add_pd(target, _mm_mul_pd(coefficient, _mm_sub_pd(previous, target)));
      _mm_storeu_pd(&mGain[i], gain);
      gains = _mm_add_pd(gains, gain);
      if (synthesize)
      {
        minimum = _mm_min_pd(minimum, gain);
        const auto left = _mm_loadu_pd(&split[0][i]);
        sumL = _mm_add_pd(sumL, left);
        wetL = _mm_add_pd(wetL, _mm_mul_pd(left, gain));
        if (stereo)
        {
          const auto right = _mm_loadu_pd(&split[1][i]);
          sumR = _mm_add_pd(sumR, right);
          wetR = _mm_add_pd(wetR, _mm_mul_pd(right, gain));
        }
      }
    }
    gainSum = SumPair(gains);
    reference = std::min(_mm_cvtsd_f64(minimum), _mm_cvtsd_f64(_mm_unpackhi_pd(minimum, minimum)));
    bandSum[0] = SumPair(sumL);
    wet[0] = SumPair(wetL);
    if (stereo)
    {
      bandSum[1] = SumPair(sumR);
      wet[1] = SumPair(wetR);
    }
#elif defined(__aarch64__) && !defined(NAM_NOISE_PRINT_SCALAR)
    // Fused operations match scalar ARM rounding; the sums below stay ordered.
    static_assert(bands % 2 == 0);
    const auto one = vdupq_n_f64(1.), zero = vdupq_n_f64(0.);
    for (int i = 0; i < bands; i += 2)
    {
      auto target = one;
      auto openingAttack = vdupq_n_f64(attack);
      if (reducing)
      {
        const auto ratio = vmulq_f64(vld1q_f64(&mPower[i]), vld1q_f64(&inverseThreshold[i]));
        const auto evidence = vminq_f64(one, vmaxq_f64(zero, vmulq_n_f64(vsubq_f64(ratio, vdupq_n_f64(4.)), 1. / 12.)));
        openingAttack = vfmaq_n_f64(openingAttack, evidence, fastAttack - attack);
        const auto knee = vminq_f64(one, vmaxq_f64(zero, vsubq_f64(ratio, one)));
        const auto curve = vmulq_f64(vmulq_n_f64(knee, 1. - floor), knee);
        target = vmaxq_f64(vdupq_n_f64(protection),
                          vfmaq_f64(vdupq_n_f64(floor), curve, vfmsq_n_f64(vdupq_n_f64(3.), knee, 2.)));
      }
      const auto previous = vld1q_f64(&mGain[i]);
      const auto coefficient = vbslq_f64(vcgtq_f64(target, previous), openingAttack, vdupq_n_f64(release));
      vst1q_f64(&mGain[i], vfmaq_f64(target, coefficient, vsubq_f64(previous, target)));
    }
    // Keep reductions in their original band order after updating gains in pairs.
    for (int i = 0; i < bands; ++i)
    {
      gainSum += mGain[i];
      if (synthesize)
      {
        reference = std::min(reference, mGain[i]);
        bandSum[0] += split[0][i];
        wet[0] += split[0][i] * mGain[i];
        if (stereo)
        {
          bandSum[1] += split[1][i];
          wet[1] += split[1][i] * mGain[i];
        }
      }
    }
#else
    for (int i = 0; i < bands; ++i)
    {
      double target = 1.;
      double openingAttack = attack;
      if (reducing)
      {
        const double ratio = mPower[i] * inverseThreshold[i];
        openingAttack += std::clamp((ratio - 4.) / 12., 0., 1.) * (fastAttack - attack);
        const double knee = std::clamp(ratio - 1., 0., 1.);
        target = std::max(protection, floor + (1. - floor) * knee * knee * (3. - 2. * knee));
      }
      const double coefficient = target > mGain[i] ? openingAttack : release;
      mGain[i] = target + coefficient * (mGain[i] - target);
      gainSum += mGain[i];
      if (synthesize)
      {
        reference = std::min(reference, mGain[i]);
        bandSum[0] += split[0][i];
        wet[0] += split[0][i] * mGain[i];
        if (stereo)
        {
          bandSum[1] += split[1][i];
          wet[1] += split[1][i] * mGain[i];
        }
      }
    }
#endif
    // Equal gains reconstruct the scaled input exactly, as before.
    for (int channel = 0; channel < (stereo ? 2 : 1); ++channel)
      wet[channel] = reference * dry[channel] + mSynthesis * (wet[channel] - reference * bandSum[channel]);
    return gainSum;
  }
  static double Safe(double x) { return std::isfinite(x) ? std::clamp(x, -1000., 1000.) : 0.; }
  static double Flush(double x) { return std::abs(x) < 1.e-30 ? 0. : x; }
  struct FilterState
  { std::array<double, bands> first{}, second{}; };
  std::array<FilterState, 2> mFilters{};
  struct Coefficient
  { std::array<double, bands> b0{}, a1{}, a2{}; };
  Coefficient mCoefficient;
  std::array<double, bands> mDetector{}, mPower{}, mMean{}, mGain{};
  std::unique_ptr<RobustCapture> mCapture;
  double mOnsetFast = 0., mOnsetSlow = 0., mOnsetFastPole = 0., mOnsetSlowPole = 0.;
  int mOnsetHold = 0;
  Profile mProfile;
  Tracking mTracking;
  uint64_t mSamples = 0, mWarmup = 0;
  double mRate = 48000., mMix = 1., mReduction = 0., mSynthesis = 1.;
  double mCleanPeak = 0.;
  final_gate::Gate mGate;
  bool mLearning = false, mChanged = true;
};
} // namespace noise_print
