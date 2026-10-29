#pragma once
#include <cmath>
#include <vector>
#include "SlotHQEffects.h"

namespace hq_test
{
inline double ToneEnergy(const std::vector<double>& x, size_t start, double hz, double rate)
{
  double re = 0., im = 0.;
  for (size_t i = start; i < x.size(); ++i)
  {
    const double phase = 2. * grid_fx::pi * hz * static_cast<double>(i) / rate;
    re += x[i] * std::cos(phase);
    im += x[i] * std::sin(phase);
  }
  return std::hypot(re, im) / std::max<size_t>(1, x.size() - start);
}
inline double Rms(const std::vector<double>& x, size_t start)
{
  double sum = 0.;
  for (size_t i = start; i < x.size(); ++i)
    sum += x[i] * x[i];
  return std::sqrt(sum / std::max<size_t>(1, x.size() - start));
}
} // namespace hq_test

void TestHQEffects()
{
  constexpr double rate = 48000.;
  // Granular pitch shifter: a 440 Hz tone moves to exactly the requested
  // interval at unity level, rejecting the input and the other tested intervals.
  // Squaring or rectifying also makes 880 Hz, but neither reaches the requested downward intervals
  // nor keeps the level; a wrong ratio or sign misses the wanted bin.
  constexpr std::array<double, 4> intervals{12., -12., 7., -5.};
  for (const double hostRate : {44100., 48000., 96000.})
    for (const double semitones : intervals)
    {
      grid_hq::PitchShifter shifter;
      shifter.Reset(hostRate);
      std::vector<double> out(static_cast<size_t>(hostRate));
      for (size_t i = 0; i < out.size(); ++i)
        out[i] = shifter.ProcessSample(.2 * std::sin(2. * grid_fx::pi * 440. * i / hostRate), 0, semitones);
      const size_t start = out.size() / 2;
      const double wanted = hq_test::ToneEnergy(out, start, 440. * std::exp2(semitones / 12.), hostRate);
      assert(wanted > .09 && wanted < .11); // ToneEnergy is half the amplitude: unity gain gives .1.
      assert(wanted > 30. * hq_test::ToneEnergy(out, start, 440., hostRate));
      for (const double other : intervals)
        if (other != semitones)
          assert(wanted > 30. * hq_test::ToneEnergy(out, start, 440. * std::exp2(other / 12.), hostRate));
    }
  // Frequency shifter: one sideband, with the carrier and the opposite sideband
  // rejected across the guitar range (a ring modulator keeps both). The upper
  // sideband moves by Shift; the lower sideband mirrors it.
  for (const double hostRate : {44100., 48000., 96000.})
    for (const double tone : {110., 440., 3000.})
      for (const double shift : {100., -100., 40.})
        for (const double sideband : {100., -100.})
        {
          grid_hq::FrequencyShifter shifter;
          shifter.Reset(hostRate);
          UtilityControls c;
          c.shift = {shift, 0., 1., sideband, 12000., 0., 100.};
          std::vector<double> out(static_cast<size_t>(hostRate));
          for (size_t i = 0; i < out.size(); ++i)
          {
            double value = .2 * std::sin(2. * grid_fx::pi * tone * i / hostRate);
            shifter.Process(&value, nullptr, 1, c);
            out[i] = value;
          }
          const size_t start = out.size() / 2;
          const double moved = sideband > 0. ? shift : -shift;
          const double wanted = hq_test::ToneEnergy(out, start, tone + moved, hostRate);
          assert(wanted > .08); // ToneEnergy is half the amplitude: unity gain gives .1.
          assert(wanted > 30. * hq_test::ToneEnergy(out, start, tone - moved, hostRate));
          assert(wanted > 30. * hq_test::ToneEnergy(out, start, tone, hostRate));
        }
  // Juno chorus at 100% wet, fed the same tone on both channels: the swept
  // delays spread the tone's pitch (a no-op or a static delay keeps it fully
  // coherent), the channels decorrelate (a mono chorus keeps Side at zero) and
  // the wet level stays near the input. 0% mix is exact dry.
  for (const double hostRate : {44100., 48000., 96000.})
    for (const double mode : {0., 1., 2.})
    {
      grid_hq::JunoChorus chorus;
      chorus.Reset(hostRate);
      UtilityControls c;
      c.juno = {mode, 100., 100.};
      std::vector<double> l(static_cast<size_t>(2. * hostRate)), r(l.size());
      for (size_t i = 0; i < l.size(); ++i)
        l[i] = r[i] = .2 * std::sin(2. * grid_fx::pi * 440. * i / hostRate);
      chorus.Process(l.data(), r.data(), static_cast<int>(l.size()), c);
      const size_t start = l.size() / 4;
      std::vector<double> mid(l.size()), side(l.size());
      for (size_t i = 0; i < l.size(); ++i)
      {
        mid[i] = .5 * (l[i] + r[i]);
        side[i] = .5 * (l[i] - r[i]);
      }
      const double inputRms = .2 / std::sqrt(2.);
      for (const auto* channel : {&l, &r})
      {
        const double rms = hq_test::Rms(*channel, start);
        // A steady sine gives 2 * ToneEnergy == sqrt(2) * RMS; the chorus measures .15-.27.
        assert(2. * hq_test::ToneEnergy(*channel, start, 440., hostRate) < .5 * std::sqrt(2.) * rms);
        assert(rms > .7 * inputRms && rms < 1.4 * inputRms);
      }
      assert(hq_test::Rms(side, start) > .5 * hq_test::Rms(mid, start));
    }
  {
    grid_hq::JunoChorus chorus;
    UtilityControls c;
    c.juno = {1., 100., 100.};
    std::vector<double> l(4096), r(4096);
    chorus.Reset(rate);
    c.juno[1] = 0.;
    for (size_t i = 0; i < l.size(); ++i)
    {
      l[i] = .2 * std::sin(2. * grid_fx::pi * 440. * i / rate);
      r[i] = .1 * std::sin(2. * grid_fx::pi * 330. * i / rate);
    }
    std::vector<double> dryL = l, dryR = r;
    chorus.Process(l.data(), r.data(), static_cast<int>(l.size()), c);
    for (size_t i = 0; i < l.size(); ++i)
      assert(std::abs(l[i] - dryL[i]) < 1.e-12 && std::abs(r[i] - dryR[i]) < 1.e-12);
  }
  // Infer the vintage delay directly from a tiny linear ramp. After the filters
  // settle, ramp displacement measures delay without exposing DSP internals.
  for (const double hostRate : {44100., 48000., 96000.})
  {
    grid_hq::JunoChorus chorus;
    chorus.Reset(hostRate);
    UtilityControls c;
    c.juno = {3., 100., 100.};
    const double coefficient = std::exp(-2. * grid_fx::pi * std::min(.45 * hostRate, 20000.) / hostRate);
    const double filterLag = 2. * coefficient / (1. - coefficient);
    constexpr double step = 1.e-10;
    double minimum = 1., maximum = 0.;
    int crossings = 0, previousCrossing = 0;
    double periodSum = 0., previousDelay = .0035;
    for (int i = 0; i < static_cast<int>(2. * hostRate); ++i)
    {
      double l = step * i, r = l;
      chorus.Process(&l, &r, 1, c);
      if (i < static_cast<int>(.1 * hostRate)) continue;
      assert(l == r);
      // DelayLine reads relative to the position after Push (one sample ahead).
      const double delay = (i - l / step - filterLag + 1.) / hostRate;
      minimum = std::min(minimum, delay);
      maximum = std::max(maximum, delay);
      if (previousDelay < .0035 && delay >= .0035)
      {
        if (previousCrossing) { periodSum += i - previousCrossing; ++crossings; }
        previousCrossing = i;
      }
      previousDelay = delay;
    }
    assert(std::abs(minimum - .0033) < .000002);
    assert(std::abs(maximum - .0037) < .000002);
    assert(crossings >= 16);
    const double measuredRate = hostRate * crossings / periodSum;
    assert(std::abs(measuredRate - 9.75) < .01);
  }
  // Shimmer: feedback changes the tail, freeze holds it, reset returns silence.
  {
    grid_hq::Shimmer shimmer;
    shimmer.Reset(rate);
    UtilityControls c;
    c.shimmer[0] = 100.;
    c.shimmer[1] = 4.;
    c.shimmer[3] = 12.;
    c.shimmer[4] = 60.;
    c.shimmer[6] = 0.;
    std::vector<double> l(rate / 2), r(rate / 2);
    for (size_t i = 0; i < l.size(); ++i)
    {
      l[i] = .2 * std::sin(2. * grid_fx::pi * 220. * i / rate);
      r[i] = .2 * std::sin(2. * grid_fx::pi * 330. * i / rate);
    }
    for (size_t pos = 0; pos < l.size(); pos += 256)
      shimmer.Process(l.data() + pos, r.data() + pos, static_cast<int>(std::min<size_t>(256, l.size() - pos)), c);
    c.shimmer[6] = 1.;
    std::vector<double> heldL(12000), heldR(12000);
    for (size_t pos = 0; pos < heldL.size(); pos += 256)
      shimmer.Process(
        heldL.data() + pos, heldR.data() + pos, static_cast<int>(std::min<size_t>(256, heldL.size() - pos)), c);
    assert(hq_test::Rms(heldL, 4096) > 1.e-4);
    for (size_t i = 1000; i < heldL.size(); ++i)
      assert(std::isfinite(heldL[i]) && std::isfinite(heldR[i]));
    shimmer.Reset(rate);
    std::fill(heldL.begin(), heldL.end(), 0.);
    std::fill(heldR.begin(), heldR.end(), 0.);
    c.shimmer[6] = 0.;
    shimmer.Process(heldL.data(), heldR.data(), static_cast<int>(heldL.size()), c);
    for (double value : heldL)
      assert(value == 0.);
  }
  // Extreme settings stay finite at every supported rate and during a long tail.
  for (double hostRate : {44100., 48000., 96000., 192000.})
  {
    grid_hq::Shimmer shimmer;
    shimmer.Reset(hostRate);
    UtilityControls c;
    c.shimmer = {100., 20., 12000., 24., 100., 100., 0.};
    std::vector<double> l(8192), r(8192);
    for (size_t i = 0; i < l.size(); ++i)
    {
      l[i] = .8 * std::sin(2. * grid_fx::pi * 110. * i / hostRate);
      r[i] = .8 * std::cos(2. * grid_fx::pi * 131. * i / hostRate);
    }
    for (int pass = 0; pass < 6; ++pass)
    {
      shimmer.Process(l.data(), r.data(), static_cast<int>(l.size()), c, pass > 0);
      for (size_t i = 0; i < l.size(); ++i)
        assert(std::isfinite(l[i]) && std::isfinite(r[i]) && std::abs(l[i]) < 100. && std::abs(r[i]) < 100.);
    }
    grid_hq::FrequencyShifter shifter;
    shifter.Reset(hostRate);
    c.shift = {2000., 100., .05, 100., 12000., 100., 100.};
    for (int pass = 0; pass < 4; ++pass)
    {
      for (size_t i = 0; i < l.size(); ++i)
      {
        l[i] = .8 * std::sin(2. * grid_fx::pi * 110. * i / hostRate);
        r[i] = .8 * std::cos(2. * grid_fx::pi * 131. * i / hostRate);
      }
      shifter.Process(l.data(), r.data(), static_cast<int>(l.size()), c);
      for (size_t i = 0; i < l.size(); ++i)
        assert(std::isfinite(l[i]) && std::isfinite(r[i]) && std::abs(l[i]) < 100. && std::abs(r[i]) < 100.);
    }
  }
  std::cout << "HQ effects: pitch/frequency shift, Juno stereo/mix and Shimmer freeze/reset passed\n";
}
