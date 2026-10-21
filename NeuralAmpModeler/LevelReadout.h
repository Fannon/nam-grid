#pragma once
#include <algorithm>
#include <cmath>

struct LevelReadout
{
  double average = 0., peak = 0., hold = 0.;
  void Receive(double rms, double incomingPeak)
  {
    average = std::isfinite(rms) ? std::max(0., rms) : 0.;
    if (std::isfinite(incomingPeak) && incomingPeak > peak)
    { peak = incomingPeak; hold = 1.5; }
  }
  void Tick(double seconds, bool stale)
  {
    if (stale) average *= std::exp(-seconds / .35);
    const double release = std::max(0., seconds - hold);
    hold = std::max(0., hold - seconds);
    if (release > 0.) peak = std::max(average, peak * std::pow(10., -12. * release / 20.));
    if (average < 1.e-8) average = 0.;
    if (peak < 1.e-8) peak = 0.;
  }
};
