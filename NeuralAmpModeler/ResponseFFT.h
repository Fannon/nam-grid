#pragma once
#include "third_party/fftconvolver/AudioFFT.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <vector>

// Fast UI-thread frequency-response and IR-preview calculation via AudioFFT.
// Replaces O(N*M) brute-force DFTs in Draw() and lets Cabinet IR previews be
// computed once during slot preparation instead of reading WAV files on Draw().
namespace grid_response_fft
{
inline constexpr size_t kIRBins = 384;
inline constexpr size_t kMeasuredBins = 96;
inline constexpr double kPi = 3.14159265358979323846;

struct IRPreview
{
  std::array<float, kIRBins> response{};
  double upper = 20000.;
  double seconds = 0.;
  bool longIR = false;
  bool valid = false;
};

inline size_t NextPowerOfTwo(size_t value)
{
  size_t size = 1;
  while (size < value) size <<= 1;
  return size;
}

inline double WindowedSinc(double x, int a)
{
  const double ax = std::abs(x);
  if (ax < 1.e-12) return 1.;
  if (ax >= static_cast<double>(a)) return 0.;
  const double pix = kPi * x;
  const double sinc = std::sin(pix) / pix;
  const double u = kPi * x / static_cast<double>(a);
  // 4-term Blackman-Harris window (-92 dB sidelobes, flat across |t|/fftSize <= 1/8).
  const double window = 0.35875 + 0.48829 * std::cos(u) + 0.14128 * std::cos(2. * u) + 0.01168 * std::cos(3. * u);
  return sinc * window;
}

// Evaluates |X(bin)| from a centered real-to-complex AudioFFT spectrum using
// an 8-lobe Blackman-Harris windowed sinc with conjugate-symmetric reflection.
// Centering the length-N impulse around index 0 keeps phase slope <= pi/4 per
// bin when fftSize >= 4*N, matching exact DFT magnitudes well within 0.01 dB.
inline double InterpolateMagnitude(const std::vector<float>& re, const std::vector<float>& im,
                                   size_t fftSize, double bin)
{
  constexpr int kLobes = 8;
  const int half = static_cast<int>(fftSize / 2);
  const int center = static_cast<int>(std::floor(bin));
  double sumRe = 0., sumIm = 0.;
  for (int k = center - kLobes + 1; k <= center + kLobes; ++k)
  {
    const double weight = WindowedSinc(bin - static_cast<double>(k), kLobes);
    int idx = k % static_cast<int>(fftSize);
    if (idx < 0) idx += static_cast<int>(fftSize);
    float r = 0.f, i = 0.f;
    if (idx <= half)
    {
      r = re[static_cast<size_t>(idx)];
      i = im[static_cast<size_t>(idx)];
    }
    else
    {
      const size_t mirror = fftSize - static_cast<size_t>(idx);
      r = re[mirror];
      i = -im[mirror];
    }
    sumRe += static_cast<double>(r) * weight;
    sumIm += static_cast<double>(i) * weight;
  }
  return std::hypot(sumRe, sumIm);
}

template <typename Sample>
inline void CenteredRealFFT(const Sample* samples, size_t count, size_t fftSize,
                            audiofft::AudioFFT& fft, std::vector<float>& buffer,
                            std::vector<float>& re, std::vector<float>& im)
{
  buffer.assign(fftSize, 0.f);
  const size_t half = count / 2;
  for (size_t n = 0; n < count; ++n)
  {
    const size_t index = n >= half ? n - half : fftSize - (half - n);
    buffer[index] = static_cast<float>(samples[n]);
  }
  fft.fft(buffer.data(), re.data(), im.data());
}

// Measures a linear impulse response at `kMeasuredBins` log-spaced frequencies
// from 20 Hz to min(20000, 0.49 * rate), normalized by `level` in dB.
inline void MeasureLinearResponse(const double* impulse, size_t length, double rate,
                                  std::array<float, kMeasuredBins>& out, double level)
{
  out.fill(-240.f);
  if (!impulse || length == 0 || !(rate > 0.) || !(level > 0.)) return;
  const size_t fftSize = NextPowerOfTwo(std::max<size_t>(32768, length * 4));
  audiofft::AudioFFT fft;
  fft.init(fftSize);
  std::vector<float> buffer, re(audiofft::AudioFFT::ComplexSize(fftSize)),
    im(audiofft::AudioFFT::ComplexSize(fftSize));
  CenteredRealFFT(impulse, length, fftSize, fft, buffer, re, im);
  const double upper = std::min(20000., .49 * rate);
  const double ratio = upper / 20.;
  for (size_t i = 0; i < out.size(); ++i)
  {
    const double hz = 20. * std::pow(ratio, static_cast<double>(i) / static_cast<double>(out.size() - 1));
    const double bin = hz * static_cast<double>(fftSize) / rate;
    const double magnitude = InterpolateMagnitude(re, im, fftSize, bin);
    out[i] = static_cast<float>(20. * std::log10(std::max(1.e-12, magnitude / level)));
  }
}

// Computes the Cabinet IR editor preview (peak envelope for Long IR, or
// peak-normalized log-frequency magnitude curve for standard Cabinet IRs).
inline IRPreview ComputeIRPreview(const std::vector<float>& left, const std::vector<float>& right,
                                  double irRate, double hostRate, bool longIR, double maxLongSeconds)
{
  IRPreview preview;
  preview.longIR = longIR;
  if (left.empty() || !(irRate > 0.) || !(hostRate > 0.)) return preview;
  const size_t limit = std::max<size_t>(
    1, static_cast<size_t>(irRate * (longIR ? maxLongSeconds : 8192. / hostRate)));
  const size_t count = std::min(left.size(), limit);
  const bool hasRight = right.size() >= count;
  preview.seconds = static_cast<double>(count) / irRate;
  if (longIR)
  {
    float peak = 0.f;
    preview.response.fill(0.f);
    for (size_t n = 0; n < count; ++n)
    {
      const size_t bin = std::min(preview.response.size() - 1, n * preview.response.size() / count);
      const float level = std::max(std::abs(left[n]), hasRight ? std::abs(right[n]) : 0.f);
      preview.response[bin] = std::max(preview.response[bin], level);
      peak = std::max(peak, preview.response[bin]);
    }
    const float denom = std::max(1.e-12f, peak);
    for (auto& value : preview.response)
      value = static_cast<float>(20. * std::log10(std::max(1.e-6f, value / denom)));
    preview.valid = true;
    return preview;
  }
  preview.upper = std::min(20000., .49 * irRate);
  const size_t fftSize = NextPowerOfTwo(std::max<size_t>(32768, count * 4));
  audiofft::AudioFFT fft;
  fft.init(fftSize);
  const size_t complexSize = audiofft::AudioFFT::ComplexSize(fftSize);
  std::vector<float> buffer, reL(complexSize), imL(complexSize), reR, imR;
  CenteredRealFFT(left.data(), count, fftSize, fft, buffer, reL, imL);
  if (hasRight)
  {
    reR.resize(complexSize);
    imR.resize(complexSize);
    CenteredRealFFT(right.data(), count, fftSize, fft, buffer, reR, imR);
  }
  float peak = -1.e9f;
  const double ratio = preview.upper / 20.;
  for (size_t i = 0; i < preview.response.size(); ++i)
  {
    const double hz = 20. * std::pow(ratio, static_cast<double>(i) / static_cast<double>(preview.response.size() - 1));
    const double bin = hz * static_cast<double>(fftSize) / irRate;
    double magnitude = InterpolateMagnitude(reL, imL, fftSize, bin);
    if (hasRight)
      magnitude = std::max(magnitude, InterpolateMagnitude(reR, imR, fftSize, bin));
    preview.response[i] = static_cast<float>(20. * std::log10(std::max(1.e-12, magnitude)));
    peak = std::max(peak, preview.response[i]);
  }
  for (auto& value : preview.response) value -= peak;
  preview.valid = std::isfinite(peak);
  return preview;
}
} // namespace grid_response_fft
