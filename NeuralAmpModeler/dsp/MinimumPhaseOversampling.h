#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <vector>
#if (defined(_M_X64) || defined(__x86_64__)) && !defined(NAM_PEDAL_DRIVE_SCALAR)
  #include <emmintrin.h>
#endif
#if defined(__aarch64__) && !defined(NAM_PEDAL_DRIVE_SCALAR)
  #include <arm_neon.h>
#endif

// Original MIT DSP, generalized from Pedal Drive's tested minimum-phase FIR.
// Factor selection and coefficient design happen off the audio thread.
namespace grid_oversampling
{
inline constexpr double pi = 3.14159265358979323846;
inline double Flush(double x) { return std::abs(x) < 1.e-30 ? 0. : x; }
// Minimum-phase windowed-sinc interpolation and decimation. Polyphase interpolation
// skips zero-stuffed samples; mirrored rings make each dot product contiguous.
// The filters are minimum phase, so callers need no fixed lookahead or reported delay.
// Minimum-phase conversion preserves the windowed-sinc magnitude response. Callers may pass dry audio through unshifted; the wet path retains the
// filters' frequency-dependent group delay. Design and clearing happen in preparation.
template <int Factor, int BaseTaps = 64>
class FIR
{
public:
  static_assert(Factor == 2 || Factor == 4 || Factor == 8);
  static_assert(BaseTaps >= 16 && BaseTaps <= 64 && BaseTaps % 2 == 0);
  static constexpr int factor = Factor, taps = BaseTaps * Factor + 1, latency = 0;
  static constexpr int highRing = 128 * Factor;
  void Reset()
  {
    mInput = {};
    mHigh = {};
    mInputIndex = mHighIndex = 0;
    mKernel = DesignedKernel();
  }
  static const std::array<double, taps>& DesignedKernel()
  {
    // The first caller is Reset during preparation. Coefficients depend on the
    // factor, not the host rate; subsequent instances copy the same design.
    static const auto kernel = [] { FIR filter; filter.Design(); return filter.mKernel; }();
    return kernel;
  }
  static std::complex<double> Response(double hz, double highRate)
  {
    const auto step = std::polar(1., -2. * pi * hz / highRate);
    std::complex<double> phase = 1., response{};
    for (double tap : DesignedKernel()) { response += tap * phase; phase *= step; }
    return response;
  }
  // The designed coefficients, for tests that convolve directly.
  const std::array<double, taps>& Kernel() const { return mKernel; }
  template <class Nonlinear>
  double Tick(double input, Nonlinear&& nonlinear)
  {
    mInputIndex = (mInputIndex - 1) & 127;
    mInput[mInputIndex] = mInput[mInputIndex + 128] = input;
    // Each input load feeds independent interpolation sums, allowing SIMD
    // without changing their tap order.
    std::array<double, factor> up{};
    Interpolate(up.data());
    up[0] += mKernel[taps - 1] * mInput[mInputIndex + (taps - 1) / factor];
    double result = 0.;
    for (int phase = 0; phase < factor; ++phase)
    {
      const double high = Flush(nonlinear(up[phase] * factor));
      mHighIndex = (mHighIndex - 1) & (highRing - 1);
      mHigh[mHighIndex] = mHigh[mHighIndex + highRing] = high;
      // Select a causal output at phase zero; there is no fixed lookahead.
      if (phase == 0)
      {
        // Independent sums remove the serial addition dependency. The kernel
        // and phase are unchanged; only roundoff from the final reduction differs.
        std::array<double, factor> down{};
        Decimate(down.data());
        if constexpr (factor == 4)
          result = ((down[0] + down[1]) + (down[2] + down[3]));
        else
          for (double value : down) result += value;
        result += mKernel[taps - 1] * mHigh[mHighIndex + taps - 1];
      }
    }
    return Flush(result);
  }

  // Split interface for joint stereo cores: interpolate both channels, process
  // corresponding high-rate samples together, then decimate independently.
  std::array<double, factor> Upsample(double input)
  {
    mInputIndex = (mInputIndex - 1) & 127;
    mInput[mInputIndex] = mInput[mInputIndex + 128] = input;
    std::array<double, factor> up{};
    Interpolate(up.data());
    up[0] += mKernel[taps - 1] * mInput[mInputIndex + (taps - 1) / factor];
    for (double& value : up) value *= factor;
    return up;
  }
  double Downsample(const std::array<double, factor>& samples)
  {
    double result = 0.;
    for (int phase = 0; phase < factor; ++phase)
    {
      mHighIndex = (mHighIndex - 1) & (highRing - 1);
      mHigh[mHighIndex] = mHigh[mHighIndex + highRing] = Flush(samples[phase]);
      if (phase == 0)
      {
        std::array<double, factor> down{};
        Decimate(down.data());
        if constexpr (factor == 4) result = ((down[0] + down[1]) + (down[2] + down[3]));
        else for (double value : down) result += value;
        result += mKernel[taps - 1] * mHigh[mHighIndex + taps - 1];
      }
    }
    return Flush(result);
  }


  // Cepstral minimum-phase conversion: same magnitude, causal impulse response, so
  // the interpolation and decimation filters need no lookahead. The polyphase index
  // mapping is unchanged, which is why only latency and phase change.
  void MinimumPhase()
  {
    constexpr int fftSize = 1 << 15;
    std::vector<std::complex<double>> spectrum(fftSize);
    for (int i = 0; i < taps; ++i)
      spectrum[i] = mKernel[i];
    CepstrumFft(spectrum, false);
    for (auto& value : spectrum)
      value = std::log(std::max(1.e-12, std::abs(value)));
    CepstrumFft(spectrum, true); // real cepstrum
    std::vector<std::complex<double>> folded(fftSize, std::complex<double>(0.));
    folded[0] = spectrum[0];
    for (int i = 1; i < fftSize / 2; ++i)
      folded[i] = spectrum[i] * 2.;
    folded[fftSize / 2] = spectrum[fftSize / 2];
    CepstrumFft(folded, false);
    for (auto& value : folded)
      value = std::exp(value);
    CepstrumFft(folded, true);
    double sum = 0.;
    for (int i = 0; i < taps; ++i)
    {
      mKernel[i] = folded[i].real();
      sum += mKernel[i];
    }
    for (double& x : mKernel)
      x /= sum;
  }
  static void CepstrumFft(std::vector<std::complex<double>>& values, bool inverse)
  {
    const size_t n = values.size();
    for (size_t i = 1, j = 0; i < n; ++i)
    {
      size_t bit = n >> 1;
      for (; j & bit; bit >>= 1)
        j ^= bit;
      j ^= bit;
      if (i < j)
        std::swap(values[i], values[j]);
    }
    for (size_t length = 2; length <= n; length <<= 1)
    {
      const double angle = 2. * pi / double(length) * (inverse ? 1. : -1.);
      const std::complex<double> step(std::cos(angle), std::sin(angle));
      for (size_t i = 0; i < n; i += length)
      {
        std::complex<double> rotation(1.);
        for (size_t k = 0; k < length / 2; ++k)
        {
          const auto even = values[i + k], odd = values[i + k + length / 2] * rotation;
          values[i + k] = even + odd;
          values[i + k + length / 2] = even - odd;
          rotation *= step;
        }
      }
    }
    if (inverse)
      for (auto& value : values)
        value /= double(n);
  }

private:
  void Design()
  {
    double sum = 0.;
    for (int i = 0; i < taps; ++i)
    {
      const double x = i - (taps - 1) * .5;
      const double sinc = x == 0. ? .9 / factor : std::sin(2. * pi * (.45 / factor) * x) / (pi * x);
      const double window = .42 - .5 * std::cos(2. * pi * i / (taps - 1)) + .08 * std::cos(4. * pi * i / (taps - 1));
      mKernel[i] = sinc * window;
      sum += mKernel[i];
    }
    for (double& x : mKernel)
      x /= sum;
    MinimumPhase();
  }
  // Pair independent phases in SIMD registers and split long sums across four
  // accumulators to hide add latency (2x/4x on arm64, 4x on x64). NEON uses
  // explicit fused multiply-add; regrouping and fusion agree with the portable
  // loop to roundoff. Other tap counts keep the sequential SIMD fallback.
  void Interpolate(double* up) const
  {
#if defined(__aarch64__) && !defined(NAM_PEDAL_DRIVE_SCALAR)
    if constexpr (factor == 2 && BaseTaps % 4 == 0)
    {
      auto a = vdupq_n_f64(0.), b = a, c = a, d = a;
      for (int sample = 0; sample < BaseTaps; sample += 4)
      {
        a = vfmaq_f64(a, vld1q_f64(mKernel.data() + 2 * (sample + 0)), vdupq_n_f64(mInput[mInputIndex + sample + 0]));
        b = vfmaq_f64(b, vld1q_f64(mKernel.data() + 2 * (sample + 1)), vdupq_n_f64(mInput[mInputIndex + sample + 1]));
        c = vfmaq_f64(c, vld1q_f64(mKernel.data() + 2 * (sample + 2)), vdupq_n_f64(mInput[mInputIndex + sample + 2]));
        d = vfmaq_f64(d, vld1q_f64(mKernel.data() + 2 * (sample + 3)), vdupq_n_f64(mInput[mInputIndex + sample + 3]));
      }
      vst1q_f64(up, vaddq_f64(vaddq_f64(a, b), vaddq_f64(c, d)));
      return;
    }
    if constexpr (factor == 2)
    {
      auto sum = vdupq_n_f64(0.);
      for (int sample = 0; sample < BaseTaps; ++sample)
        sum = vfmaq_f64(sum, vld1q_f64(mKernel.data() + 2 * sample), vdupq_n_f64(mInput[mInputIndex + sample]));
      vst1q_f64(up, sum);
      return;
    }
    if constexpr (factor == 4 && BaseTaps % 4 == 0)
    {
      // Four accumulators per phase pair hide the multiply-add dependency.
      const double* in = &mInput[mInputIndex];
      const double* kernel = mKernel.data();
      float64x2_t a0 = vdupq_n_f64(0.), a1 = a0, b0 = a0, b1 = a0, c0 = a0, c1 = a0, d0 = a0, d1 = a0;
      for (int sample = 0; sample < BaseTaps; sample += 4, in += 4, kernel += 16)
      {
        const float64x2_t i0 = vdupq_n_f64(in[0]), i1 = vdupq_n_f64(in[1]), i2 = vdupq_n_f64(in[2]), i3 = vdupq_n_f64(in[3]);
        a0 = vfmaq_f64(a0, vld1q_f64(kernel), i0);
        a1 = vfmaq_f64(a1, vld1q_f64(kernel + 2), i0);
        b0 = vfmaq_f64(b0, vld1q_f64(kernel + 4), i1);
        b1 = vfmaq_f64(b1, vld1q_f64(kernel + 6), i1);
        c0 = vfmaq_f64(c0, vld1q_f64(kernel + 8), i2);
        c1 = vfmaq_f64(c1, vld1q_f64(kernel + 10), i2);
        d0 = vfmaq_f64(d0, vld1q_f64(kernel + 12), i3);
        d1 = vfmaq_f64(d1, vld1q_f64(kernel + 14), i3);
      }
      vst1q_f64(up, vaddq_f64(vaddq_f64(a0, b0), vaddq_f64(c0, d0)));
      vst1q_f64(up + 2, vaddq_f64(vaddq_f64(a1, b1), vaddq_f64(c1, d1)));
      return;
    }
    if constexpr (factor == 4)
    {
      float64x2_t low = vdupq_n_f64(0.), high = vdupq_n_f64(0.);
      const double* in = &mInput[mInputIndex];
      const double* kernel = mKernel.data();
      for (int group = 0; group < (taps - 1) / (factor * 2); ++group)
      {
        const float64x2_t first = vdupq_n_f64(in[0]), second = vdupq_n_f64(in[1]);
        low = vfmaq_f64(low, vld1q_f64(kernel), first);
        high = vfmaq_f64(high, vld1q_f64(kernel + 2), first);
        low = vfmaq_f64(low, vld1q_f64(kernel + 4), second);
        high = vfmaq_f64(high, vld1q_f64(kernel + 6), second);
        kernel += 2 * factor;
        in += 2;
      }
      vst1q_f64(up, low);
      vst1q_f64(up + 2, high);
      return;
    }
#endif
#if (defined(_M_X64) || defined(__x86_64__)) && !defined(NAM_PEDAL_DRIVE_SCALAR)
    // SSE2 is baseline on x64; no AVX/FMA or CPU dispatch is required.
    if constexpr (factor == 4 && BaseTaps % 4 == 0)
    {
      // Two phases share a register and each sum is split over four accumulators
      // held in named registers. One accumulator is a serial chain of additions,
      // which bounds the filter by add latency instead of throughput; the split
      // changes the result by roundoff only.
      const double* in = &mInput[mInputIndex];
      const double* kernel = mKernel.data();
      __m128d a0 = _mm_setzero_pd(), a1 = a0, b0 = a0, b1 = a0, c0 = a0, c1 = a0, d0 = a0, d1 = a0;
      for (int sample = 0; sample < BaseTaps; sample += 4, in += 4, kernel += 16)
      {
        const __m128d i0 = _mm_set1_pd(in[0]), i1 = _mm_set1_pd(in[1]), i2 = _mm_set1_pd(in[2]), i3 = _mm_set1_pd(in[3]);
        a0 = _mm_add_pd(a0, _mm_mul_pd(_mm_loadu_pd(kernel), i0));
        a1 = _mm_add_pd(a1, _mm_mul_pd(_mm_loadu_pd(kernel + 2), i0));
        b0 = _mm_add_pd(b0, _mm_mul_pd(_mm_loadu_pd(kernel + 4), i1));
        b1 = _mm_add_pd(b1, _mm_mul_pd(_mm_loadu_pd(kernel + 6), i1));
        c0 = _mm_add_pd(c0, _mm_mul_pd(_mm_loadu_pd(kernel + 8), i2));
        c1 = _mm_add_pd(c1, _mm_mul_pd(_mm_loadu_pd(kernel + 10), i2));
        d0 = _mm_add_pd(d0, _mm_mul_pd(_mm_loadu_pd(kernel + 12), i3));
        d1 = _mm_add_pd(d1, _mm_mul_pd(_mm_loadu_pd(kernel + 14), i3));
      }
      _mm_storeu_pd(up, _mm_add_pd(_mm_add_pd(a0, b0), _mm_add_pd(c0, d0)));
      _mm_storeu_pd(up + 2, _mm_add_pd(_mm_add_pd(a1, b1), _mm_add_pd(c1, d1)));
      return;
    }
    // Other factors pack independent phases with one accumulator per register.
    __m128d sums[factor / 2];
    for (auto& sum : sums) sum = _mm_setzero_pd();
    for (int sample = 0; sample < (taps - 1) / factor; ++sample)
    {
      const auto input = _mm_set1_pd(mInput[mInputIndex + sample]);
      for (int pair = 0; pair < factor / 2; ++pair)
        sums[pair] = _mm_add_pd(sums[pair], _mm_mul_pd(
          _mm_loadu_pd(mKernel.data() + factor * sample + 2 * pair), input));
    }
    for (int pair = 0; pair < factor / 2; ++pair) _mm_storeu_pd(up + 2 * pair, sums[pair]);
    return;
#endif
    for (int sample = 0; sample < (taps - 1) / factor; ++sample)
      for (int phase = 0; phase < factor; ++phase)
        up[phase] += mKernel[factor * sample + phase] * mInput[mInputIndex + sample];

  }
  void Decimate(double* down) const
  {
#if defined(__aarch64__) && !defined(NAM_PEDAL_DRIVE_SCALAR)
    if constexpr (factor == 2 && BaseTaps % 4 == 0)
    {
      auto a = vdupq_n_f64(0.), b = a, c = a, d = a;
      for (int sample = 0; sample < BaseTaps; sample += 4)
      {
        a = vfmaq_f64(a, vld1q_f64(mKernel.data() + 2 * (sample + 0)), vld1q_f64(mHigh.data() + mHighIndex + 2 * (sample + 0)));
        b = vfmaq_f64(b, vld1q_f64(mKernel.data() + 2 * (sample + 1)), vld1q_f64(mHigh.data() + mHighIndex + 2 * (sample + 1)));
        c = vfmaq_f64(c, vld1q_f64(mKernel.data() + 2 * (sample + 2)), vld1q_f64(mHigh.data() + mHighIndex + 2 * (sample + 2)));
        d = vfmaq_f64(d, vld1q_f64(mKernel.data() + 2 * (sample + 3)), vld1q_f64(mHigh.data() + mHighIndex + 2 * (sample + 3)));
      }
      vst1q_f64(down, vaddq_f64(vaddq_f64(a, b), vaddq_f64(c, d)));
      return;
    }
    if constexpr (factor == 2)
    {
      auto sum = vdupq_n_f64(0.);
      for (int tap = 0; tap < taps - 1; tap += 2)
        sum = vfmaq_f64(sum, vld1q_f64(mKernel.data() + tap), vld1q_f64(mHigh.data() + mHighIndex + tap));
      vst1q_f64(down, sum);
      return;
    }
    if constexpr (factor == 4 && BaseTaps % 4 == 0)
    {
      const double* ring = &mHigh[mHighIndex];
      const double* kernel = mKernel.data();
      float64x2_t a0 = vdupq_n_f64(0.), a1 = a0, b0 = a0, b1 = a0, c0 = a0, c1 = a0, d0 = a0, d1 = a0;
      for (int tap = 0; tap < taps - 1; tap += 16, ring += 16, kernel += 16)
      {
        a0 = vfmaq_f64(a0, vld1q_f64(kernel), vld1q_f64(ring));
        a1 = vfmaq_f64(a1, vld1q_f64(kernel + 2), vld1q_f64(ring + 2));
        b0 = vfmaq_f64(b0, vld1q_f64(kernel + 4), vld1q_f64(ring + 4));
        b1 = vfmaq_f64(b1, vld1q_f64(kernel + 6), vld1q_f64(ring + 6));
        c0 = vfmaq_f64(c0, vld1q_f64(kernel + 8), vld1q_f64(ring + 8));
        c1 = vfmaq_f64(c1, vld1q_f64(kernel + 10), vld1q_f64(ring + 10));
        d0 = vfmaq_f64(d0, vld1q_f64(kernel + 12), vld1q_f64(ring + 12));
        d1 = vfmaq_f64(d1, vld1q_f64(kernel + 14), vld1q_f64(ring + 14));
      }
      vst1q_f64(down, vaddq_f64(vaddq_f64(a0, b0), vaddq_f64(c0, d0)));
      vst1q_f64(down + 2, vaddq_f64(vaddq_f64(a1, b1), vaddq_f64(c1, d1)));
      return;
    }
    if constexpr (factor == 4)
    {
      float64x2_t low = vdupq_n_f64(0.), high = vdupq_n_f64(0.);
      const double* ring = &mHigh[mHighIndex];
      const double* kernel = mKernel.data();
      for (int group = 0; group < (taps - 1) / (factor * 2); ++group)
      {
        low = vfmaq_f64(low, vld1q_f64(kernel), vld1q_f64(ring));
        high = vfmaq_f64(high, vld1q_f64(kernel + 2), vld1q_f64(ring + 2));
        low = vfmaq_f64(low, vld1q_f64(kernel + 4), vld1q_f64(ring + 4));
        high = vfmaq_f64(high, vld1q_f64(kernel + 6), vld1q_f64(ring + 6));
        kernel += 2 * factor;
        ring += 2 * factor;
      }
      vst1q_f64(down, low);
      vst1q_f64(down + 2, high);
      return;
    }
#endif
#if (defined(_M_X64) || defined(__x86_64__)) && !defined(NAM_PEDAL_DRIVE_SCALAR)
    if constexpr (factor == 4 && BaseTaps % 4 == 0)
    {
      // Four accumulators per register, as in Interpolate.
      const double* ring = &mHigh[mHighIndex];
      const double* kernel = mKernel.data();
      __m128d a0 = _mm_setzero_pd(), a1 = a0, b0 = a0, b1 = a0, c0 = a0, c1 = a0, d0 = a0, d1 = a0;
      for (int tap = 0; tap < taps - 1; tap += 16, ring += 16, kernel += 16)
      {
        a0 = _mm_add_pd(a0, _mm_mul_pd(_mm_loadu_pd(kernel), _mm_loadu_pd(ring)));
        a1 = _mm_add_pd(a1, _mm_mul_pd(_mm_loadu_pd(kernel + 2), _mm_loadu_pd(ring + 2)));
        b0 = _mm_add_pd(b0, _mm_mul_pd(_mm_loadu_pd(kernel + 4), _mm_loadu_pd(ring + 4)));
        b1 = _mm_add_pd(b1, _mm_mul_pd(_mm_loadu_pd(kernel + 6), _mm_loadu_pd(ring + 6)));
        c0 = _mm_add_pd(c0, _mm_mul_pd(_mm_loadu_pd(kernel + 8), _mm_loadu_pd(ring + 8)));
        c1 = _mm_add_pd(c1, _mm_mul_pd(_mm_loadu_pd(kernel + 10), _mm_loadu_pd(ring + 10)));
        d0 = _mm_add_pd(d0, _mm_mul_pd(_mm_loadu_pd(kernel + 12), _mm_loadu_pd(ring + 12)));
        d1 = _mm_add_pd(d1, _mm_mul_pd(_mm_loadu_pd(kernel + 14), _mm_loadu_pd(ring + 14)));
      }
      _mm_storeu_pd(down, _mm_add_pd(_mm_add_pd(a0, b0), _mm_add_pd(c0, d0)));
      _mm_storeu_pd(down + 2, _mm_add_pd(_mm_add_pd(a1, b1), _mm_add_pd(c1, d1)));
      return;
    }
    __m128d sums[factor / 2];
    for (auto& sum : sums) sum = _mm_setzero_pd();
    for (int tap = 0; tap < taps - 1; tap += factor)
      for (int pair = 0; pair < factor / 2; ++pair)
        sums[pair] = _mm_add_pd(sums[pair], _mm_mul_pd(
          _mm_loadu_pd(mKernel.data() + tap + 2 * pair),
          _mm_loadu_pd(mHigh.data() + mHighIndex + tap + 2 * pair)));
    for (int pair = 0; pair < factor / 2; ++pair) _mm_storeu_pd(down + 2 * pair, sums[pair]);
    return;
#endif
    for (int tap = 0; tap < taps - 1; tap += factor)
      for (int j = 0; j < factor; ++j)
        down[j] += mKernel[tap + j] * mHigh[mHighIndex + tap + j];

  }

  std::array<double, taps> mKernel{};
  std::array<double, 256> mInput{};
  std::array<double, 2 * highRing> mHigh{};
  int mInputIndex = 0, mHighIndex = 0;
};

} // namespace grid_oversampling
