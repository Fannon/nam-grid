#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#if (defined(_M_X64) || defined(__SSE2__)) && !defined(NAM_AI_GATE_SCALAR)
  #include <emmintrin.h>
  #define NAM_AI_GATE_SSE2 1
#else
  #define NAM_AI_GATE_SSE2 0
#endif
#include "AIGateModel.h"

// Original MIT NAM Grid module. A small recurrent network reads the Smart Gate's
// zero-latency resonator bank about 1500 times a second and sets one gain per band:
// no Learn step, lookahead or reported latency. Training: scripts/ai-gate/.
namespace ai_gate
{
inline constexpr int bands = 96, maxUnits = 64;
// Row-major float weights in PyTorch layout (GRU gate order r, z, n).
// inner and hidden are multiples of 8, at most maxUnits.
struct Model
{
  int inner = 0, hidden = 0, tickRate = 1500;
  const float *inputWeight = nullptr, *inputBias = nullptr;
  const float *gruInput = nullptr, *gruHidden = nullptr, *gruInputBias = nullptr, *gruHiddenBias = nullptr;
  const float *outputWeight = nullptr, *outputBias = nullptr;
};
// The inference loops read rows in groups of eight into fixed-size scratch arrays.
constexpr bool Supported(int inner, int hidden, int tickRate)
{
  return inner > 0 && hidden > 0 && inner <= maxUnits && hidden <= maxUnits && inner % 8 == 0 && hidden % 8 == 0
         && tickRate >= 100 && tickRate <= 8000;
}
inline bool Supported(const Model& m)
{
  return Supported(m.inner, m.hidden, m.tickRate) && m.inputWeight && m.inputBias && m.gruInput && m.gruHidden
         && m.gruInputBias && m.gruHiddenBias && m.outputWeight && m.outputBias;
}
static_assert(Supported(model::inner, model::hidden, model::tickRate));
static_assert(sizeof(model::inputWeight) == sizeof(float) * model::inner * bands
              && sizeof(model::gruInput) == sizeof(float) * 3 * model::hidden * model::inner
              && sizeof(model::gruHidden) == sizeof(float) * 3 * model::hidden * model::hidden
              && sizeof(model::outputWeight) == sizeof(float) * bands * model::hidden);
inline Model DefaultModel()
{
  namespace m = model;
  return {m::inner, m::hidden, m::tickRate, m::inputWeight, m::inputBias, m::gruInput, m::gruHidden,
          m::gruInputBias, m::gruHiddenBias, m::outputWeight, m::outputBias};
}
struct Controls
{
  double reduction = 40.; // dB, maximum attenuation per band
  double sensitivity = 0.; // -100..100 %: positive closes sooner, negative protects quiet sustain.
  double mix = 100.;
  double attackProtection = 50.; // Clear pick attacks versus passing sudden noise bursts, 0..100%.
};

class Processor
{
public:
  // The model's weights are borrowed and must outlive the processor. An unsupported
  // model falls back to the built-in one instead of reaching the inference loops.
  void Reset(double rate, const Model& model = DefaultModel())
  {
    mModel = Supported(model) ? model : DefaultModel();
    mHop = std::max(1, static_cast<int>(std::lround(std::max(1., rate) / mModel.tickRate)));
    mFilters = {};
    mAccumulator = {};
    mState = {};
    mGain.fill(1.);
    mStep.fill(0.);
    mCount = 0;
    mReduction = 0.;
    mFirst = true;
    mFastPower = mSlowPower = mOnsetGuard = 0.;
    mOnsetHold = 0;
    mFastPowerStep = 1. - std::exp(-1. / (.0002 * rate));
    mSlowPowerStep = 1. - std::exp(-1. / (.020 * rate));
    mGuardOpenStep = 1. / (.0001 * rate);
    mGuardRelease = std::exp(-1. / (.005 * rate));
    mOnsetHoldSamples = static_cast<int>(.003 * rate);
    constexpr double q = 4.;
    const double step = std::log(4000.) / (bands - 1);
    const double root = std::sqrt(4. * q * q - 1.);
    mSynthesis = step * root / (2. * std::atan(root));
    for (int i = 0; i < bands; ++i)
    {
      const double frequency = std::min(5. * std::exp(step * i), .45 * rate);
      const double omega = 6.283185307179586 * frequency / rate;
      const double alpha = omega / (2. * q);
      const double inverse = 1. / (1. + alpha);
      mB0[i] = alpha * inverse;
      mA1[i] = -2. * std::cos(omega) * inverse;
      mA2[i] = (1. - alpha) * inverse;
    }
    mMixStep = 1. / (.005 * rate);
    mControlStep = 1. - std::exp(-mHop / (.005 * std::max(1., rate)));
  }
  double Reduction() const { return mReduction; }

  void Process(double* left, double* right, int frames, const Controls& c)
  {
    const double reduction = std::clamp(c.reduction, 0., 100.);
    const double bias = std::clamp(c.sensitivity, -100., 100.) * .04;
    const double mixTarget = std::clamp(c.mix, 0., 100.) * .01;
    const double protectionTarget = std::clamp(c.attackProtection, 0., 100.) * .01;
    double minimumGain = 1.;
    if (mFirst)
    {
      // Starts closed after a reset or recall, so idle noise does not burst through
      // before the first decisions. A note opens it within a couple of control ticks.
      // Controls start at their values: a reset does not depend on earlier automation.
      mReductionDB = reduction;
      mBias = bias;
      mGain.fill(std::pow(10., -reduction * .05));
      mMix = mixTarget;
      mProtection = protectionTarget;
      mFirst = false;
    }
    for (int sample = 0; sample < frames; ++sample)
    {
      const double dry[2]{Safe(left[sample]), right ? Safe(right[sample]) : 0.};
      double wet[2];
      const double gainSum = Tick(dry, right != nullptr, wet);
      // A causal onset protector bridges the network's decision interval. Maximum
      // L/R power preserves one-sided and inverted stereo. It favors clear attacks,
      // but also passes more abrupt noise bursts: it is not a guitar classifier.
      const double power = std::max(dry[0] * dry[0], dry[1] * dry[1]);
      mFastPower = Flush(mFastPower + mFastPowerStep * (power - mFastPower));
      mSlowPower = Flush(mSlowPower + mSlowPowerStep * (power - mSlowPower));
      const double onset = std::clamp((mFastPower / (mSlowPower + 1.e-5) - 4.) / 12., 0., 1.);
      if (onset > .8)
        mOnsetHold = mOnsetHoldSamples;
      else if (mOnsetHold > 0)
        --mOnsetHold;
      const double guardTarget = mOnsetHold > 0 ? 1. : onset;
      mOnsetGuard = guardTarget > mOnsetGuard ? std::min(guardTarget, mOnsetGuard + mGuardOpenStep)
                                            : Flush(std::max(guardTarget, mOnsetGuard * mGuardRelease));
      mMix += std::clamp(mixTarget - mMix, -mMixStep, mMixStep);
      mProtection += std::clamp(protectionTarget - mProtection, -mMixStep, mMixStep);
      // Rounded ramp endpoints can sit an ulp below unity. Once Reduction has
      // settled at zero, return exact dry while continuing all processor histories.
      const double effectiveMix = mReductionDB == 0. ? 0. : mMix * (1. - mProtection * mOnsetGuard);
      left[sample] = dry[0] + effectiveMix * (wet[0] - dry[0]);
      if (right)
        right[sample] = dry[1] + effectiveMix * (wet[1] - dry[1]);
      minimumGain = std::min(minimumGain, 1. - effectiveMix + effectiveMix * gainSum / bands);
      if (++mCount >= mHop)
      {
        // Reduction and Sensitivity jumps glide over about 5 ms and then settle exactly.
        mReductionDB = Glide(mReductionDB, reduction);
        mBias = Glide(mBias, bias);
        Infer(std::pow(10., -mReductionDB * .05), static_cast<float>(mBias));
        mCount = 0;
      }
    }
    mReduction = -20. * std::log10(std::max(1.e-6, minimumGain));
  }

private:
  // One sample: split, measure, advance the gain ramps and resynthesize.
  double Tick(const double* dry, bool stereo, double* wet)
  {
#if NAM_AI_GATE_SSE2
    const auto inputL = _mm_set1_pd(dry[0]), inputR = _mm_set1_pd(dry[1]);
    const auto negative = _mm_set1_pd(-0.), half = _mm_set1_pd(.5);
    auto minimum = _mm_set1_pd(1.), gains = _mm_setzero_pd();
    auto sumL = _mm_setzero_pd(), sumR = sumL, wetL = sumL, wetR = sumL;
    for (int i = 0; i < bands; i += 2)
    {
      const auto b0 = _mm_loadu_pd(&mB0[i]), a1 = _mm_loadu_pd(&mA1[i]), a2 = _mm_loadu_pd(&mA2[i]);
      const auto band = [&](int channel, __m128d input) {
        auto& state = mFilters[channel];
        const auto excitation = _mm_mul_pd(b0, input);
        const auto value = _mm_add_pd(excitation, _mm_loadu_pd(&state.first[i]));
        _mm_storeu_pd(&state.first[i], FlushPair(_mm_sub_pd(_mm_loadu_pd(&state.second[i]), _mm_mul_pd(a1, value))));
        _mm_storeu_pd(&state.second[i], FlushPair(_mm_sub_pd(_mm_xor_pd(excitation, negative), _mm_mul_pd(a2, value))));
        return value;
      };
      const auto gain = _mm_add_pd(_mm_loadu_pd(&mGain[i]), _mm_loadu_pd(&mStep[i]));
      _mm_storeu_pd(&mGain[i], gain);
      minimum = _mm_min_pd(minimum, gain);
      gains = _mm_add_pd(gains, gain);
      const auto valueL = band(0, inputL);
      auto power = _mm_mul_pd(valueL, valueL);
      sumL = _mm_add_pd(sumL, valueL);
      wetL = _mm_add_pd(wetL, _mm_mul_pd(valueL, gain));
      if (stereo)
      {
        const auto valueR = band(1, inputR);
        power = _mm_mul_pd(half, _mm_add_pd(power, _mm_mul_pd(valueR, valueR)));
        sumR = _mm_add_pd(sumR, valueR);
        wetR = _mm_add_pd(wetR, _mm_mul_pd(valueR, gain));
      }
      _mm_storeu_pd(&mAccumulator[i], _mm_add_pd(_mm_loadu_pd(&mAccumulator[i]), power));
    }
    const double reference = std::min(_mm_cvtsd_f64(minimum), _mm_cvtsd_f64(_mm_unpackhi_pd(minimum, minimum)));
    wet[0] = reference * dry[0] + mSynthesis * (SumPair(wetL) - reference * SumPair(sumL));
    if (stereo)
      wet[1] = reference * dry[1] + mSynthesis * (SumPair(wetR) - reference * SumPair(sumR));
    return SumPair(gains);
#else
    double reference = 1., gainSum = 0., sum[2]{}, weighted[2]{};
    for (int i = 0; i < bands; ++i)
    {
      mGain[i] += mStep[i];
      reference = std::min(reference, mGain[i]);
      gainSum += mGain[i];
      double power = 0.;
      for (int channel = 0; channel < (stereo ? 2 : 1); ++channel)
      {
        auto& state = mFilters[channel];
        const double value = mB0[i] * dry[channel] + state.first[i];
        state.first[i] = Flush(-mA1[i] * value + state.second[i]);
        state.second[i] = Flush(-mB0[i] * dry[channel] - mA2[i] * value);
        power += value * value;
        sum[channel] += value;
        weighted[channel] += value * mGain[i];
      }
      mAccumulator[i] += stereo ? .5 * power : power;
    }
    for (int channel = 0; channel < (stereo ? 2 : 1); ++channel)
      wet[channel] = reference * dry[channel] + mSynthesis * (weighted[channel] - reference * sum[channel]);
    return gainSum;
#endif
  }

  // Control tick: band powers of the last hop -> gain targets reached over the next hop.
  void Infer(double floor, float bias)
  {
    const Model& m = mModel;
    alignas(16) float feature[bands], x[maxUnits], gi[3 * maxUnits], gh[3 * maxUnits], logit[bands];
    const double inverseHop = 1. / mHop;
    for (int i = 0; i < bands; ++i)
    {
      feature[i] = static_cast<float>(10. * std::log10(mAccumulator[i] * inverseHop + 1.e-12) + 60.) * (1.f / 30.f);
      mAccumulator[i] = 0.;
    }
    for (int j = 0; j < m.inner; ++j)
      x[j] = m.inputBias[j] + Dot(m.inputWeight + j * bands, feature, bands);
    Tanh(x, m.inner);
    for (int j = 0; j < 3 * m.hidden; ++j)
    {
      gi[j] = m.gruInputBias[j] + Dot(m.gruInput + j * m.inner, x, m.inner);
      gh[j] = m.gruHiddenBias[j] + Dot(m.gruHidden + j * m.hidden, mState.data(), m.hidden);
    }
    const int h = m.hidden;
    for (int j = 0; j < 2 * h; ++j)
      gi[j] += gh[j];
    Sigmoid(gi, 2 * h);
    for (int j = 0; j < h; ++j)
      gi[2 * h + j] += gi[j] * gh[2 * h + j];
    Tanh(gi + 2 * h, h);
    for (int j = 0; j < h; ++j)
      mState[j] = gi[2 * h + j] + gi[h + j] * (mState[j] - gi[2 * h + j]);
    for (int k = 0; k < bands; ++k)
      logit[k] = m.outputBias[k] - bias + Dot(m.outputWeight + k * h, mState.data(), h);
    Sigmoid(logit, bands);
    for (int k = 0; k < bands; ++k)
      mStep[k] = (floor + (1. - floor) * logit[k] - mGain[k]) * inverseHop;
  }

#if NAM_AI_GATE_SSE2
  static __m128d FlushPair(__m128d value)
  {
    const auto magnitude = _mm_andnot_pd(_mm_set1_pd(-0.), value);
    return _mm_andnot_pd(_mm_cmplt_pd(magnitude, _mm_set1_pd(1.e-30)), value);
  }
  static double SumPair(__m128d value) { return _mm_cvtsd_f64(value) + _mm_cvtsd_f64(_mm_unpackhi_pd(value, value)); }
  static float Dot(const float* a, const float* b, int n)
  {
    auto first = _mm_setzero_ps(), second = first;
    for (int i = 0; i < n; i += 8)
    {
      first = _mm_add_ps(first, _mm_mul_ps(_mm_loadu_ps(a + i), _mm_loadu_ps(b + i)));
      second = _mm_add_ps(second, _mm_mul_ps(_mm_loadu_ps(a + i + 4), _mm_loadu_ps(b + i + 4)));
    }
    first = _mm_add_ps(first, second);
    first = _mm_add_ps(first, _mm_movehl_ps(first, first));
    return _mm_cvtss_f32(_mm_add_ss(first, _mm_shuffle_ps(first, first, 1)));
  }
  // Cephes-style single-precision exp; inputs are clamped where the sigmoid saturates.
  static __m128 Exp(__m128 x)
  {
    x = _mm_min_ps(_mm_max_ps(x, _mm_set1_ps(-30.f)), _mm_set1_ps(30.f));
    const auto k = _mm_cvtps_epi32(_mm_mul_ps(x, _mm_set1_ps(1.44269504088896341f)));
    const auto fk = _mm_cvtepi32_ps(k);
    auto r = _mm_sub_ps(x, _mm_mul_ps(fk, _mm_set1_ps(.693359375f)));
    r = _mm_sub_ps(r, _mm_mul_ps(fk, _mm_set1_ps(-2.12194440e-4f)));
    auto y = _mm_set1_ps(1.9875691500e-4f);
    for (float coefficient : {1.3981999507e-3f, 8.3334519073e-3f, 4.1665795894e-2f, 1.6666665459e-1f, 5.0000001201e-1f})
      y = _mm_add_ps(_mm_mul_ps(y, r), _mm_set1_ps(coefficient));
    y = _mm_add_ps(_mm_add_ps(_mm_mul_ps(_mm_mul_ps(y, r), r), r), _mm_set1_ps(1.f));
    return _mm_mul_ps(y, _mm_castsi128_ps(_mm_slli_epi32(_mm_add_epi32(k, _mm_set1_epi32(127)), 23)));
  }
  static void Sigmoid(float* x, int n)
  {
    const auto one = _mm_set1_ps(1.f);
    for (int i = 0; i < n; i += 4)
      _mm_storeu_ps(x + i, _mm_div_ps(one, _mm_add_ps(one, Exp(_mm_xor_ps(_mm_loadu_ps(x + i), _mm_set1_ps(-0.f))))));
  }
  static void Tanh(float* x, int n)
  {
    const auto one = _mm_set1_ps(1.f), two = _mm_set1_ps(2.f);
    for (int i = 0; i < n; i += 4)
      _mm_storeu_ps(x + i, _mm_sub_ps(one, _mm_div_ps(two, _mm_add_ps(one, Exp(_mm_mul_ps(two, _mm_loadu_ps(x + i)))))));
  }
#else
  // Portable path: the same summation order and exp polynomial as the SSE2 code.
  static float Dot(const float* a, const float* b, int n)
  {
    float sum[8]{};
    for (int i = 0; i < n; i += 8)
      for (int j = 0; j < 8; ++j)
        sum[j] += a[i + j] * b[i + j];
    return ((sum[0] + sum[4]) + (sum[2] + sum[6])) + ((sum[1] + sum[5]) + (sum[3] + sum[7]));
  }
  static float Exp(float x)
  {
    x = std::min(std::max(x, -30.f), 30.f);
    const float fk = std::nearbyint(x * 1.44269504088896341f);
    float r = x - fk * .693359375f;
    r -= fk * -2.12194440e-4f;
    float y = 1.9875691500e-4f;
    for (float coefficient : {1.3981999507e-3f, 8.3334519073e-3f, 4.1665795894e-2f, 1.6666665459e-1f, 5.0000001201e-1f})
      y = y * r + coefficient;
    return std::ldexp(y * r * r + r + 1.f, static_cast<int>(fk));
  }
  static void Sigmoid(float* x, int n)
  {
    for (int i = 0; i < n; ++i)
      x[i] = 1.f / (1.f + Exp(-x[i]));
  }
  static void Tanh(float* x, int n)
  {
    for (int i = 0; i < n; ++i)
      x[i] = 1.f - 2.f / (1.f + Exp(2.f * x[i]));
  }
#endif
  double Glide(double value, double target) const
  {
    return std::abs(target - value) < 1.e-3 ? target : value + (target - value) * mControlStep;
  }
  static double Safe(double x) { return std::isfinite(x) ? std::clamp(x, -1000., 1000.) : 0.; }
  static double Flush(double x) { return std::abs(x) < 1.e-30 ? 0. : x; }
  struct FilterState
  { std::array<double, bands> first{}, second{}; };
  std::array<FilterState, 2> mFilters{};
  std::array<double, bands> mB0{}, mA1{}, mA2{}, mAccumulator{}, mGain{}, mStep{};
  alignas(16) std::array<float, maxUnits> mState{};
  Model mModel;
  double mSynthesis = 1., mMix = 1., mMixStep = 0., mReduction = 0.;
  double mReductionDB = 40., mBias = 0., mControlStep = 1.;
  int mHop = 32, mCount = 0;
  double mProtection = .5, mFastPower = 0., mSlowPower = 0., mOnsetGuard = 0.;
  double mFastPowerStep = 0., mSlowPowerStep = 0., mGuardOpenStep = 0., mGuardRelease = 0.;
  int mOnsetHold = 0, mOnsetHoldSamples = 0;
  bool mFirst = true;
};
} // namespace ai_gate
