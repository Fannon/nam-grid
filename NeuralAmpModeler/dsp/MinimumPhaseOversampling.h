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
  // Each phase accumulates its own taps in the scalar order the reference uses,
  // so pairing two phases in one SIMD register keeps the output bit-identical.
  // On arm64 this halves the instructions per tap of the four dot products that
  // dominate Pedal Drive's cost; other targets keep the portable loop.
  void Interpolate(double* up) const
  {
#if defined(__aarch64__) && !defined(NAM_PEDAL_DRIVE_SCALAR)
    if constexpr (factor == 4)
    {
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
      return;
    }
#endif
#if (defined(_M_X64) || defined(__x86_64__)) && !defined(NAM_PEDAL_DRIVE_SCALAR)
    // Pack independent phases, preserving the scalar tap order within each
    // lane. SSE2 is baseline on x64; no AVX/FMA or CPU dispatch is required.
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
    if constexpr (factor == 4)
    {
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
      return;
    }
#endif
#if (defined(_M_X64) || defined(__x86_64__)) && !defined(NAM_PEDAL_DRIVE_SCALAR)
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
