#pragma once
#include <numeric>
#include <complex>
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
  // Bloom swells each note into the tank instead of spreading the input diffusers into
  // separate repeats, which the released stretch produced at high settings.
  {
    UtilityControls c;
    c.shimmer = {100., 14., 7400., 12., 50., 55., 0.};
    // Echo density of the impulse response, 0.1-1 s: about 1 for a dense tail. The
    // 28x diffuser stretch measured about 0.6 at Bloom 92-100.
    const auto density = [&](double bloom) {
      grid_hq::Shimmer shimmer;
      shimmer.Reset(rate);
      auto u = c; u.shimmerBloom = bloom;
      std::vector<double> l(static_cast<size_t>(1.2 * rate)), r(l.size());
      l[0] = r[0] = .1;
      shimmer.Process(l.data(), r.data(), static_cast<int>(l.size()), u);
      double sum = 0.; int windows = 0;
      for (double t = .1; t < 1.; t += .05, ++windows)
      {
        const size_t a = static_cast<size_t>(t * rate), b = static_cast<size_t>((t + .05) * rate);
        double e = 0.; for (size_t i = a; i < b; ++i) e += l[i] * l[i];
        const double sd = std::sqrt(e / (b - a)); int outside = 0;
        for (size_t i = a; i < b; ++i) outside += std::abs(l[i]) > sd;
        sum += outside / double(b - a) / .3173;
      }
      return sum / windows;
    };
    assert(density(92.) > .9 && density(100.) > .9);
    // A sharply picked note: wet energy in 100 ms windows, and over three seconds.
    const auto note = [&](double bloom) {
      grid_hq::Shimmer shimmer;
      shimmer.Reset(rate);
      auto u = c; u.shimmerBloom = bloom;
      std::vector<double> l(static_cast<size_t>(3. * rate)), r(l.size());
      for (size_t i = 0; i < l.size(); ++i)
      {
        const double t = i / rate; double x = 0.;
        for (int k = 1; k <= 12; ++k) x += std::sin(2. * grid_fx::pi * 196. * k * t) / k * std::exp(-t * (1.5 + .6 * k));
        l[i] = r[i] = .15 * x;
      }
      shimmer.Process(l.data(), r.data(), static_cast<int>(l.size()), u);
      std::vector<double> window(30);
      for (size_t i = 0; i < l.size(); ++i) window[i * 10 / static_cast<size_t>(rate)] += l[i] * l[i] + r[i] * r[i];
      return window;
    };
    const auto close = note(0.), bloom = note(100.);
    const auto total = [](const std::vector<double>& w) { return std::accumulate(w.begin(), w.end(), 0.); };
    // The wet starts from near silence and swells for half a second or more ...
    assert(bloom[0] + bloom[1] < 1.e-3 * (close[0] + close[1]));
    assert(std::max_element(bloom.begin(), bloom.end()) - bloom.begin() >= 4);
    for (int w = 1; w < 4; ++w) assert(bloom[w] > bloom[w - 1]);
    // ... and its level stays near Bloom 0's.
    assert(std::abs(10. * std::log10(total(bloom) / total(close))) < 5.);
  }
  // Strong shimmer: no DC builds up in the loop, the higher octaves taper off rather than
  // piling up, and the shifted layers sit slightly flat of the exact interval.
  {
    const auto tail = [&](double amount, double density, bool pluck) {
      grid_hq::Shimmer shimmer;
      shimmer.Reset(rate);
      UtilityControls c;
      c.shimmer = {100., 14., 7400., 12., amount, 25., 0.};
      c.shimmerDensity = density;
      std::vector<double> l(static_cast<size_t>(7. * rate)), r(l.size());
      for (size_t i = 0; i < static_cast<size_t>((pluck ? 2. : .6) * rate); ++i)
      {
        const double t = i / rate;
        if (pluck)
        {
          double x = 0.;
          for (int k = 1; k <= 8; ++k) x += std::sin(2. * grid_fx::pi * 110. * k * t) / k * std::exp(-t * (1.5 + .6 * k));
          l[i] = r[i] = .15 * x;
        }
        else
          l[i] = r[i] = .1 * std::min(1., std::min(t, .6 - t) / .005) * std::sin(2. * grid_fx::pi * 440. * t);
      }
      shimmer.Process(l.data(), r.data(), static_cast<int>(l.size()), c);
      return l;
    };
    // Energy of a Hann-windowed segment [a, b) s between lo and hi Hz, in 1 Hz steps.
    const auto energy = [&](const std::vector<double>& x, double a, double b, double lo, double hi, double* centroid = nullptr) {
      const size_t i0 = static_cast<size_t>(a * rate), n = static_cast<size_t>((b - a) * rate);
      double total = 0., weighted = 0.;
      for (double hz = lo; hz <= hi; hz += 1.)
      {
        std::complex<double> sum = 0., w = std::polar(1., -2. * grid_fx::pi * hz / rate), z = 1.;
        for (size_t i = 0; i < n; ++i, z *= w)
          sum += x[i0 + i] * (.5 - .5 * std::cos(2. * grid_fx::pi * i / (n - 1))) * z;
        total += std::norm(sum);
        weighted += std::norm(sum) * 1200. * std::log2(hz / 880.);
      }
      if (centroid) *centroid = weighted / total;
      return total;
    };
    const auto plucked = tail(100., 40., true);
    double mean = 0., power = 0.;
    for (size_t i = static_cast<size_t>(rate); i < static_cast<size_t>(4. * rate); ++i) { mean += plucked[i]; power += plucked[i] * plucked[i]; }
    const double samples = 3. * rate;
    assert(std::abs(mean / samples) < .01 * std::sqrt(power / samples));
    const auto tone = tail(100., 40., false);
    const double first = energy(tone, 2., 3.5, 845., 915.), fourth = energy(tone, 2., 3.5, 6760., 7320.);
    assert(fourth < first * std::pow(10., -.3));
    double cents = 0.;
    energy(tail(60., 100., false), 1., 3., 760., 1000., &cents);
    assert(cents < -15. && cents > -55.);
  }
  // Density: lower settings thin the tank into clearer, distinct echoes while the octave
  // layers keep sounding. Removing all line mixing instead let the shifted lines climb
  // out of hearing within a second.
  {
    const auto impulseDensity = [&](double density, double amount) {
      grid_hq::Shimmer shimmer;
      shimmer.Reset(rate);
      UtilityControls c;
      c.shimmer = {100., 6., 7400., 12., amount, 10., 0.};
      c.shimmerDensity = density;
      std::vector<double> l(static_cast<size_t>(1.2 * rate)), r(l.size());
      l[0] = r[0] = .1;
      shimmer.Process(l.data(), r.data(), static_cast<int>(l.size()), c);
      double sum = 0.; int windows = 0;
      for (double t = .1; t < 1.; t += .05, ++windows)
      {
        const size_t a = static_cast<size_t>(t * rate), b = static_cast<size_t>((t + .05) * rate);
        double e = 0.; for (size_t i = a; i < b; ++i) e += l[i] * l[i];
        const double sd = std::sqrt(e / (b - a)); int outside = 0;
        for (size_t i = a; i < b; ++i) outside += std::abs(l[i]) > sd;
        sum += outside / double(b - a) / .3173;
      }
      return sum / windows;
    };
    assert(impulseDensity(100., 35.) > .9 && impulseDensity(0., 35.) < .8);
    // Taking some wet output directly from the shifters fills gaps at high
    // Amount, but Density 0 should still have fewer echoes than Density 100.
    assert(impulseDensity(0., 100.) + .1 < impulseDensity(100., 100.));
    // Octave layer against the played note in the tail of a 220 Hz tone, 1-3 s.
    const auto octave = [&](double density) {
      grid_hq::Shimmer shimmer;
      shimmer.Reset(rate);
      UtilityControls c;
      c.shimmer = {100., 6., 7400., 12., 100., 10., 0.};
      c.shimmerDensity = density;
      std::vector<double> l(static_cast<size_t>(3. * rate)), r(l.size());
      for (size_t i = 0; i < static_cast<size_t>(.5 * rate); ++i)
      {
        const double t = i / rate;
        l[i] = r[i] = .1 * std::min(1., std::min(t, .5 - t) / .005) * std::sin(2. * grid_fx::pi * 220. * t);
      }
      shimmer.Process(l.data(), r.data(), static_cast<int>(l.size()), c);
      const auto band = [&](double hz) {
        std::complex<double> sum = 0.;
        for (size_t i = static_cast<size_t>(rate); i < l.size(); ++i)
          sum += l[i] * std::polar(1., -2. * grid_fx::pi * hz * i / rate);
        return std::norm(sum);
      };
      double fundamental = 0., layer = 0.;
      for (double d = -4.; d <= 4.; d += 1.) { fundamental += band(220. + d); layer += band(440. + d); }
      return 10. * std::log10(layer / fundamental);
    };
    assert(octave(0.) > -6.);
  }
  // At high Amount the first octave should come through the wet
  // signal with sparse reflections or a smooth, high-Density tail.
  for (const double density : {7.8, 100.})
  {
    grid_hq::Shimmer shimmer;
    shimmer.Reset(rate);
    UtilityControls c;
    c.shimmer = {100., density < 10. ? 1.4 : 5., 7400., 12., 100., 12., 0.};
    c.shimmerDensity = density;
    std::vector<double> l(static_cast<size_t>(.5 * rate)), r(l.size());
    for (size_t i = 0; i < l.size(); ++i)
    {
      const double t = i / rate;
      const double gain = .15 * std::min(1., t / .005);
      l[i] = r[i] = gain * std::sin(2. * grid_fx::pi * 220. * t);
    }
    shimmer.Process(l.data(), r.data(), static_cast<int>(l.size()), c);
    for (const auto* channel : {&l, &r})
      assert(hq_test::BandPower(*channel, static_cast<size_t>(.15 * rate), 440., rate)
             > 2. * hq_test::BandPower(*channel, static_cast<size_t>(.15 * rate), 220., rate));
  }
  // Wet level: about 6 dB above Room and Hall, as a studio shimmer at the same decay
  // (IR energy .28-.8 at default settings; Room and Hall sit at .07-.2).
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
    assert(energy > .28 && energy < .8);
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
    // Fully wet at these settings the tail peaks near twice the 0.5 input; 4x would
    // mean runaway feedback.
    assert(peak < 2. && late < early * 1.e-2);
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
