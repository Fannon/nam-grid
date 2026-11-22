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
// Power of a sine-like component within 4% or 12 Hz of `hz`, whichever is wider:
// Goertzel probes at the window's bin spacing.
inline double BandPower(const std::vector<double>& x, size_t start, double hz, double rate)
{
  const double spacing = rate / static_cast<double>(x.size() - start), width = std::max(.04 * hz, 12.);
  double power = 0.;
  for (double f = hz - width; f <= hz + width; f += spacing)
  {
    const double a = 2. * ToneEnergy(x, start, f, rate);
    power += .5 * a * a;
  }
  return power;
}
// Decay time in the octave around `hz`: a band-pass, then a -5 to -25 dB Schroeder fit.
inline double BandDecay(const std::vector<double>& x, double hz, double rate)
{
  const double w = 2. * grid_fx::pi * hz / rate, alpha = std::sin(w) * std::sinh(std::log(2.) / 2. * w / std::sin(w));
  const double a0 = 1. + alpha, b0 = alpha / a0, a1 = -2. * std::cos(w) / a0, a2 = (1. - alpha) / a0;
  std::vector<double> energy(x.size());
  double x1 = 0., x2 = 0., y1 = 0., y2 = 0.;
  for (size_t i = 0; i < x.size(); ++i)
  {
    const double y = b0 * (x[i] - x2) - a1 * y1 - a2 * y2;
    x2 = x1; x1 = x[i]; y2 = y1; y1 = y; energy[i] = y * y;
  }
  for (size_t i = energy.size() - 1; i-- > 0;) energy[i] += energy[i + 1];
  double sx = 0., sy = 0., sxx = 0., sxy = 0., n = 0.;
  for (size_t i = 0; i < energy.size(); ++i)
  {
    const double db = 10. * std::log10(energy[i] / energy[0] + 1.e-30);
    if (db <= -5. && db >= -25.) { const double t = i / rate; sx += t; sy += db; sxx += t * t; sxy += t * db; ++n; }
  }
  return -60. / ((n * sxy - sx * sy) / (n * sxx - sx * sx));
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
  // Granular pitch shifter: a tone moves to the requested interval at unity power,
  // with the input and the other tested intervals rejected. Grains at a fixed spacing
  // cancelled tones a half cycle apart between grains (220 and 660 Hz at 25 ms), so
  // the test covers those frequencies. Random grain phases widen the shifted line a
  // little, so the wanted level is the power within 4% of the target frequency.
  constexpr std::array<double, 4> intervals{12., -12., 7., -5.};
  for (const double hostRate : {44100., 96000.})
    for (const double tone : {110., 220., 660.})
      for (const double semitones : intervals)
      {
        grid_hq::PitchShifter shifter;
        shifter.Reset(hostRate);
        std::vector<double> out(static_cast<size_t>(1.2 * hostRate));
        for (size_t i = 0; i < out.size(); ++i)
          out[i] = shifter.ProcessSample(.2 * std::sin(2. * grid_fx::pi * tone * i / hostRate), 0, semitones);
        const size_t start = out.size() / 6;
        const double wanted = hq_test::BandPower(out, start, tone * std::exp2(semitones / 12.), hostRate);
        const double total = hq_test::Rms(out, start) * hq_test::Rms(out, start);
        // Unity power is .02. Random grain phases average to it; over one second a
        // tone lands within -3.5/+1 dB, with most of it at the wanted pitch.
        assert(total > .0089 && total < .0252 && wanted > .6 * total);
        assert(wanted > 30. * hq_test::BandPower(out, start, tone, hostRate));
        for (const double other : intervals)
          if (other != semitones)
            assert(wanted > 30. * hq_test::BandPower(out, start, tone * std::exp2(other / 12.), hostRate));
      }
  // Channels keep their own ratio and grain timing.
  {
    grid_hq::PitchShifter stereo, independent;
    stereo.Reset(rate);
    independent.Reset(rate);
    for (int i = 0; i < static_cast<int>(rate * .3); ++i)
    {
      const double x = .1 * std::sin(.037 * i);
      const double l = stereo.ProcessSample(x, 0, -5.);
      stereo.ProcessSample(x, 1, 12.);
      assert(l == independent.ProcessSample(x, 0, -5.));
    }
  }
  // The shifter sits inside Shimmer's feedback, so its power must never exceed the
  // input's, even where grains read coherent audio: a held low tone is the worst case.
  for (const double tone : {55., 82.5, 110.})
  {
    grid_hq::PitchShifter shifter;
    shifter.Reset(rate);
    std::vector<double> in(static_cast<size_t>(2. * rate)), out(in.size());
    for (size_t i = 0; i < in.size(); ++i)
      out[i] = shifter.ProcessSample(in[i] = .2 * std::sin(2. * grid_fx::pi * tone * i / rate), 0, 1.);
    assert(hq_test::Rms(out, in.size() / 4) < 1.02 * hq_test::Rms(in, in.size() / 4));
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
  // Shimmer layers: a held 220 Hz note at the default Amount grows an octave layer
  // close to the note within the first second (Valhalla-style shimmers sit within
  // 10-15 dB). A shifted copy that re-enters through the tank's input stayed 48 dB down.
  {
    grid_hq::Shimmer shimmer;
    shimmer.Reset(rate);
    UtilityControls c;
    c.shimmer = {100., 5., 6000., 12., 25., 30., 0.};
    std::vector<double> l(static_cast<size_t>(2. * rate)), r(l.size());
    for (size_t i = 0; i < static_cast<size_t>(.4 * rate); ++i)
      l[i] = r[i] = .25 * std::sin(2. * grid_fx::pi * 220. * i / rate);
    shimmer.Process(l.data(), r.data(), static_cast<int>(l.size()), c);
    const std::vector<double> window(l.begin() + static_cast<long>(.5 * rate), l.begin() + static_cast<long>(1.5 * rate));
    const double octave = hq_test::BandPower(window, 0, 440., rate), note = hq_test::BandPower(window, 0, 220., rate);
    assert(octave > .03 * note && octave < 3. * note);
  }
  // Decay sets the mid-band tail at any Decay and Damping; Damping shelves the treble
  // relative to it. A fixed low-pass on every pass cut a 12 s tail at 3 kHz Damping
  // to 8 s at 500 Hz.
  for (const double damping : {3000., 12000.})
    for (const double decay : {2., 12.})
    {
      grid_hq::Shimmer shimmer;
      shimmer.Reset(rate);
      UtilityControls c;
      c.shimmer = {100., decay, damping, 12., 0., 30., 0.};
      std::vector<double> l(static_cast<size_t>((decay * .7 + .5) * rate)), r(l.size());
      l[0] = r[0] = .1;
      shimmer.Process(l.data(), r.data(), static_cast<int>(l.size()), c);
      const double measured = hq_test::BandDecay(l, 500., rate);
      assert(measured > .88 * decay && measured < 1.12 * decay);
    }
  // Wet level: the tank sits with Room and Hall (IR energy .07-.2 at default settings).
  {
    grid_hq::Shimmer shimmer;
    shimmer.Reset(rate);
    UtilityControls c;
    c.shimmer = {100., 5., 6000., 12., 25., 30., 0.};
    std::vector<double> l(static_cast<size_t>(12. * rate)), r(l.size());
    l[0] = r[0] = 1.;
    shimmer.Process(l.data(), r.data(), static_cast<int>(l.size()), c);
    double energy = 0.;
    for (size_t i = 0; i < l.size(); ++i) energy += l[i] * l[i] + r[i] * r[i];
    assert(energy > .07 && energy < .2);
  }
  // Any Pitch at full Amount and the longest Decay stays bounded and dies away: the
  // shifted share replaces part of the recirculation instead of adding to it. Small
  // and zero intervals are the worst case, since they keep energy in the same band.
  for (const double pitch : {0., 1., 12., -12.})
  {
    grid_hq::Shimmer shimmer;
    shimmer.Reset(44100.);
    UtilityControls c;
    c.shimmer = {100., 20., 12000., pitch, 100., 100., 0.};
    std::vector<double> l(static_cast<size_t>(16. * 44100.)), r(l.size());
    for (size_t i = 0; i < static_cast<size_t>(2. * 44100.); ++i)
    {
      l[i] = .5 * std::sin(2. * grid_fx::pi * 110. * i / 44100.) * std::exp(-2. * std::fmod(i / 44100., .5));
      r[i] = .5 * std::sin(2. * grid_fx::pi * 165. * i / 44100.) * std::exp(-2. * std::fmod(i / 44100., .5));
    }
    for (size_t pos = 0; pos < l.size(); pos += 512)
      shimmer.Process(l.data() + pos, r.data() + pos, static_cast<int>(std::min<size_t>(512, l.size() - pos)), c);
    double peak = 0., early = 0., late = 0.;
    for (size_t i = 0; i < l.size(); ++i)
    {
      peak = std::max({peak, std::abs(l[i]), std::abs(r[i])});
      if (i >= static_cast<size_t>(2. * 44100.) && i < static_cast<size_t>(4. * 44100.)) early += l[i] * l[i];
      if (i >= static_cast<size_t>(14. * 44100.)) late += l[i] * l[i];
    }
    // 20 s Decay loses 36 dB over 12 s; allow the shimmer to hold it 16 dB longer.
    assert(peak < 1. && late < early * 1.e-2);
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

  // Feedback at 100%: the loop gain stays below unity and the feedback path loses DC, so a
  // burst always decays. A unity loop only bounded by the limiter could ring indefinitely, and
  // downward shifts folded partials into a standing near-DC tone with a DC offset.
  for (const double hostRate : {44100., 48000., 96000.})
    for (const double shift : {300., -300.})
      for (const double delayMS : {20., 200., 1000.})
      {
        grid_hq::FrequencyShifter shifter;
        shifter.Reset(hostRate);
        UtilityControls c;
        c.shift = {shift, 100., delayMS, 100., 12000., 0., 100.};
        const int total = static_cast<int>(hostRate * 45.);
        std::vector<double> x(total, 0.);
        for (int i = 0; i < static_cast<int>(hostRate * .05); ++i)
          x[i] = .5 * std::sin(2. * grid_fx::pi * 110. * i / hostRate);
        // Short blocks, as the stage runs the module.
        for (int pos = 0; pos < total; pos += 128)
          shifter.Process(x.data() + pos, nullptr, std::min(128, total - pos), c);
        const auto window = [&](double from, double to, double& mean) {
          double peak = 0., sum = 0.;
          const int first = static_cast<int>(hostRate * from), last = std::min(total, static_cast<int>(hostRate * to));
          for (int i = first; i < last; ++i)
          {
            peak = std::max(peak, std::abs(x[i]));
            sum += x[i];
          }
          mean = last > first ? std::abs(sum / (last - first)) : 0.;
          return peak;
        };
        double mean = 0.;
        const double firstPass = delayMS * .001 + .02; // The wet path is delayed once.
        const double burst = window(firstPass, firstPass + .5, mean);
        const double tail = window(40., 45., mean);
        // A short delay decays by more than 60 dB; a one-second delay is far down and still falling.
        assert(burst > .2 && std::isfinite(tail));
        assert(mean < 1.e-4); // No standing DC offset, whatever the shift direction.
        if (delayMS <= 500.)
          assert(tail < .001 * burst); // More than 60 dB down after 40 s.
        else
        {
          // A one-second loop loses 0.03 per pass: still falling, never standing.
          const double middle = window(20., 25., mean);
          assert(tail < .25 * burst && tail < .5 * middle);
        }
      }
  std::cout << "HQ effects: pitch/frequency shift, Juno stereo/mix and Shimmer freeze/reset passed\n";
}
