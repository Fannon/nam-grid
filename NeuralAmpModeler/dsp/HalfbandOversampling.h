#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <cstddef>

// Original MIT DSP. Oversampling by cascaded elliptic halfband filters, each the sum of
// two allpass paths: H(z) = (A0(z^2) + z^-1 A1(z^2)) / 2. Coefficients follow the closed
// form of Valenzuela and Constantinides, "Digital signal processing schemes for efficient
// interpolation and decimation", IEE Proceedings G 130(6), 1983. The guard lowpass is an
// elliptic filter split into two allpass paths after Vaidyanathan, Mitra and Neuvo, "A
// new approach to the realization of low-sensitivity IIR digital filters", IEEE Trans.
// ASSP 34(2), 1986. No external implementation was copied.
//
// Like the minimum-phase FIR in MinimumPhaseOversampling.h these filters are causal and
// need no lookahead or reported delay, so a caller may pass dry audio through unshifted.
// They cost a few multiplies per sample instead of a long dot product, in portable
// scalar code.
//
// A halfband stage is 3 dB down at a quarter of its rate, so what a nonlinearity produces
// between the host Nyquist frequency and the stopband edge folds back, to above the
// passband edge only. Interpolation followed by decimation is exactly allpass, which
// would let that band through. The guard, a lowpass at the host rate, removes it: the wet
// path is flat to 0.417 of the host rate (18.4 kHz at 44.1 kHz) and 70 dB down from 0.4535
// (20 kHz), where the folded band begins. Input above 20 kHz still reaches the
// nonlinearity with an attenuated image.
namespace grid_oversampling
{
namespace halfband
{
// The stage next to the host rate passes 20 kHz at 44.1 kHz and reaches its 104 dB
// stopband at 24.1 kHz. The outer stages only protect that stage's band, so they are
// much shorter (109 and 90 dB).
inline constexpr std::array<double, 8> first{0.037365116712056175, 0.13936182479934969, 0.28134601140385507,
                                             0.43626182684834169,  0.58446117084052807, 0.71706542969938247,
                                             0.83458204350038012,  0.94430406888858154};
inline constexpr std::array<double, 4> second{0.045384630115199498, 0.18025637104579495, 0.40744422275441589,
                                              0.75488630243484567};
inline constexpr std::array<double, 2> third{0.11390984867226996, 0.54341188212309099};

// One allpass path: first-order sections in z^2, y[n] = c (x[n] - y[n-1]) + x[n-1], in
// transposed form: y = c x + t, then t = x - c y. One value is kept per section, and
// the signal passes through one multiply and one add per section; the other half is
// computed beside it.
template <size_t Count, size_t Offset>
struct Path
{
  static constexpr size_t sections = (Count + 1 - Offset) / 2;
  std::array<double, sections> state{};
  double Tick(const std::array<double, Count>& coefficients, double x)
  {
    for (size_t i = 0; i < sections; ++i)
    {
      const double c = coefficients[2 * i + Offset], y = c * x + state[i];
      state[i] = x - c * y;
      x = y;
    }
    return x;
  }
  void Flush()
  {
    for (double& value : state)
      if (std::abs(value) < 1.e-30) value = 0.;
  }
};

// Seventh-order elliptic lowpass at the host rate, 0.01 dB ripple and 70 dB rejection, as
// the half sum of two allpass paths that take its poles alternately: seven multiplies.
// Second-order sections are (a2 + a1 z^-1 + z^-2) / (1 + a1 z^-1 + a2 z^-2), transposed.
class Guard
{
public:
  void Reset() { mFirst = 0.; mState = {}; }
  double Tick(double x)
  {
    const double first = single * x + mFirst;
    mFirst = x - single * first;
    return .5 * (Second(0, first) + Second(2, Second(1, x)));
  }
  void Flush()
  {
    if (std::abs(mFirst) < 1.e-30) mFirst = 0.;
    for (auto& section : mState)
      for (double& value : section)
        if (std::abs(value) < 1.e-30) value = 0.;
  }
  // rate is the host rate.
  static std::complex<double> Response(double hz, double rate)
  {
    const auto z1 = std::polar(1., -2. * 3.14159265358979323846 * hz / rate), z2 = z1 * z1;
    const auto second = [&](int i) { return (pairs[i][1] + pairs[i][0] * z1 + z2) / (1. + pairs[i][0] * z1 + pairs[i][1] * z2); };
    return .5 * ((single + z1) / (1. + single * z1) * second(0) + second(1) * second(2));
  }

private:
  static constexpr double single = 0.41717956035068204;
  static constexpr std::array<std::array<double, 2>, 3> pairs{{{1.5057655005569677, 0.73945286802220744},
                                                               {1.1321613976369809, 0.42300539710544838},
                                                               {1.7128464967460704, 0.92702927116416423}}};
  double Second(int i, double x)
  {
    const double y = pairs[i][1] * x + mState[i][0];
    mState[i][0] = pairs[i][0] * (x - y) + mState[i][1];
    mState[i][1] = x - pairs[i][1] * y;
    return y;
  }
  double mFirst = 0.;
  std::array<std::array<double, 2>, 3> mState{};
};

template <size_t Count>
class Stage
{
public:
  explicit constexpr Stage(const std::array<double, Count>& coefficients) : mCoefficients(coefficients) {}
  void Reset() { mUp = {}; mDown = {}; }
  // Two output samples per input; each path is allpass, so the level is kept.
  void Up(double x, double& first, double& second)
  {
    first = mUp.even.Tick(mCoefficients, x);
    second = mUp.odd.Tick(mCoefficients, x);
  }
  // The even path takes the newer sample, which supplies the z^-1 of the odd path.
  double Down(double older, double newer)
  { return .5 * (mDown.even.Tick(mCoefficients, newer) + mDown.odd.Tick(mCoefficients, older)); }
  void Flush() { mUp.even.Flush(); mUp.odd.Flush(); mDown.even.Flush(); mDown.odd.Flush(); }
  // Response of one direction at hz, where rate is this stage's higher rate.
  std::complex<double> Response(double hz, double rate) const
  {
    const auto z1 = std::polar(1., -2. * 3.14159265358979323846 * hz / rate), z2 = z1 * z1;
    std::array<std::complex<double>, 2> paths{1., 1.};
    for (size_t i = 0; i < Count; ++i) paths[i % 2] *= (mCoefficients[i] + z2) / (1. + mCoefficients[i] * z2);
    return .5 * (paths[0] + z1 * paths[1]);
  }

private:
  struct Pair
  {
    Path<Count, 0> even;
    Path<Count, 1> odd;
  };
  std::array<double, Count> mCoefficients;
  Pair mUp, mDown;
};
} // namespace halfband

// Align delays the oversampled signal by that many samples before decimation. The wet
// path of these filters lags about one host sample less than that of the minimum-phase
// FIR, which moves the comb of a partial Mix: with the FIR the first notch of a clean
// blend sits near 3.9 kHz at 48 kHz. AlignedIIR restores the delay of the FIR, so
// modules released with the FIR keep the sound of their parallel blends.
template <int Factor, int Align = 0>
class IIR
{
public:
  static_assert(Factor == 2 || Factor == 4 || Factor == 8);
  static_assert(Align >= 0);
  static constexpr int factor = Factor, latency = 0;
  void Reset()
  {
    mFirst.Reset(); mSecond.Reset(); mThird.Reset(); mGuard.Reset();
    mDelay = {};
    mSinceFlush = mDelayIndex = 0;
  }
  // Interpolation or decimation alone, as a response at the highest rate.
  static std::complex<double> Response(double hz, double highRate)
  {
    const IIR filter;
    auto response = filter.mFirst.Response(hz, 2. * highRate / factor);
    if constexpr (factor >= 4) response *= filter.mSecond.Response(hz, 4. * highRate / factor);
    if constexpr (factor >= 8) response *= filter.mThird.Response(hz, highRate);
    return response;
  }
  // Interpolation, decimation and guard together, as heard at the host rate below the
  // passband edge. The decimator answers with the newest oversampled sample of each host
  // sample, which takes factor - 1 oversampled samples off the delay of the filters.
  static std::complex<double> Linear(double hz, double highRate)
  {
    const auto response = Response(hz, highRate);
    return response * response * halfband::Guard::Response(hz, highRate / factor)
           * std::polar(1., 2. * 3.14159265358979323846 * hz * (factor - 1 - Align) / highRate);
  }
  std::array<double, factor> Upsample(double input)
  {
    std::array<double, factor> up{};
    if constexpr (factor == 2) mFirst.Up(input, up[0], up[1]);
    else
    {
      std::array<double, 2> two{};
      mFirst.Up(input, two[0], two[1]);
      if constexpr (factor == 4)
        for (int i = 0; i < 2; ++i) mSecond.Up(two[i], up[2 * i], up[2 * i + 1]);
      else
      {
        std::array<double, 4> four{};
        for (int i = 0; i < 2; ++i) mSecond.Up(two[i], four[2 * i], four[2 * i + 1]);
        for (int i = 0; i < 4; ++i) mThird.Up(four[i], up[2 * i], up[2 * i + 1]);
      }
    }
    return up;
  }
  double Downsample(std::array<double, factor> samples)
  {
    if constexpr (Align > 0)
      for (double& sample : samples)
      {
        std::swap(sample, mDelay[mDelayIndex]);
        mDelayIndex = mDelayIndex + 1 == Align ? 0 : mDelayIndex + 1;
      }
    // A decaying allpass must not idle on denormal values. Checking every kept value for
    // every sample would cost a quarter of the filter; between two checks 16 host samples
    // apart a value falls by at most 60 decades, far from the denormal range.
    if (++mSinceFlush == 16)
    {
      mSinceFlush = 0;
      mFirst.Flush();
      if constexpr (factor >= 4) mSecond.Flush();
      if constexpr (factor >= 8) mThird.Flush();
      mGuard.Flush();
    }
    // The stages keep state, so the earlier pair must be decimated first: named values,
    // because the order in which call arguments are evaluated is unspecified.
    if constexpr (factor == 2) return mGuard.Tick(mFirst.Down(samples[0], samples[1]));
    else
    {
      std::array<double, 4> four{};
      if constexpr (factor == 4) for (int i = 0; i < 4; ++i) four[i] = samples[i];
      else for (int i = 0; i < 4; ++i) four[i] = mThird.Down(samples[2 * i], samples[2 * i + 1]);
      const double older = mSecond.Down(four[0], four[1]);
      const double newer = mSecond.Down(four[2], four[3]);
      return mGuard.Tick(mFirst.Down(older, newer));
    }
  }
  // The nonlinearity sees the oversampled samples in time order.
  template <class Nonlinear>
  double Tick(double input, Nonlinear&& nonlinear)
  {
    auto samples = Upsample(input);
    for (double& sample : samples) sample = nonlinear(sample);
    return Downsample(samples);
  }

private:
  halfband::Stage<8> mFirst{halfband::first};
  halfband::Stage<4> mSecond{halfband::second};
  halfband::Stage<2> mThird{halfband::third};
  halfband::Guard mGuard;
  std::array<double, Align> mDelay{};
  int mSinceFlush = 0, mDelayIndex = 0;
};
// The delay that matches FIR<Factor> of MinimumPhaseOversampling.h.
template <int Factor>
using AlignedIIR = IIR<Factor, Factor == 2 ? 1 : Factor == 4 ? 4 : 10>;
} // namespace grid_oversampling
