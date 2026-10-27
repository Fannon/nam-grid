#pragma once
#include <algorithm>
#include <cmath>

// Original first-order antiderivative antialiasing (DAFx 2016, Parker et al.).
// Averages a fixed curve between adjacent inputs; close intervals use its
// midpoint to avoid cancellation. No allocation, lookahead or reported delay.
// The averaging has phase/group delay and must be included in wet/dry checks.
namespace grid_adaa
{
inline double LogCosh(double x)
{ return std::abs(x) + std::log1p(std::exp(-2. * std::abs(x))) - std::log(2.); }
class Average
{
public:
  void Reset() { mPrevious = mIntegral = 0.; mValid = false; }
  template <class Shape, class Integral>
  double Tick(double x, Shape&& shape, Integral&& integral)
  {
    const double delta = x - mPrevious;
    double value;
    if (std::abs(delta) <= 1.e-5 * std::max({1., std::abs(x), std::abs(mPrevious)}))
    {
      value = shape(.5 * (x + mPrevious));
      mValid = false;
    }
    else
    {
      const double current = integral(x);
      value = (current - (mValid ? mIntegral : integral(mPrevious))) / delta;
      mIntegral = current; mValid = true;
    }
    mPrevious = x;
    return value;
  }
  double Tanh(double x)
  { return Tick(x, [](double v) { return std::tanh(v); }, LogCosh); }
private:
  double mPrevious = 0., mIntegral = 0.;
  bool mValid = false;
};
// Integrate a continuous rate limiter driven by a line between target samples.
// It follows the target while possible, otherwise moves at +/-limit. A catch-up
// crossing splits the interval. Integrating the following hard clip over these
// segments avoids adding corners after a discrete Euler slew step.
class SlewClip
{
public:
  double Tick(double target, double limit, double ceiling)
  {
    const double slope = target - mTarget, error = mTarget - mState;
    const double firstSlope = std::abs(error) < 1.e-12 ? std::clamp(slope, -limit, limit) : std::copysign(limit, error);
    const double denominator = firstSlope - slope;
    const double crossing = denominator == 0. ? -1. : error / denominator;
    double value, next;
    if (crossing > 0. && crossing < 1.)
    {
      const double caught = mState + firstSlope * crossing;
      next = caught + std::clamp(slope, -limit, limit) * (1. - crossing);
      value = crossing * Mean(mState, caught, ceiling) + (1. - crossing) * Mean(caught, next, ceiling);
    }
    else
    {
      next = mState + firstSlope;
      value = Mean(mState, next, ceiling);
    }
    mState = next; mTarget = target;
    return value;
  }
  double State() const { return mState; }
private:
  static double Mean(double a, double b, double c)
  {
    if (a >= c && b >= c) return c;
    if (a <= -c && b <= -c) return -c;
    if ((a >= -c && a <= c && b >= -c && b <= c) || std::abs(b - a) < 1.e-10)
      return std::clamp(.5 * (a + b), -c, c);
    const auto primitive = [=](double x) { const double v = std::abs(x); return v <= c ? .5 * x * x : c * (v - .5 * c); };
    return (primitive(b) - primitive(a)) / (b - a);
  }
  double mState = 0., mTarget = 0.;
};
} // namespace grid_adaa
