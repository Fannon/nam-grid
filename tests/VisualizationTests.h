#pragma once
#include "EffectPreview.h"
#include "EQSignalSpectrum.h"
#include "NAMStage.h"
#include "ImpulseProbe.h"
#include "LevelReadout.h"
#include "FrequencyReadout.h"

// Checks the parameter-interaction previews against the actual processors:
// Uni-Vibe notches, delay-sweep pitch, reverb band decay and the EQ analyzer.
void TestVisualizationInteractions()
{
  // Frequency charts use their own logarithmic or linear axis, and notes use A4=440 Hz.
  assert(std::abs(frequency_readout::Hertz(.5, 20., 20000.) - std::sqrt(20. * 20000.)) < 1.e-9);
  assert(frequency_readout::Hertz(.5, 0., 2000., false) == 1000.);
  assert(frequency_readout::Hertz(-1., 20., 20000.) == 20.);
  assert(frequency_readout::Hertz(2., 20., 20000.) == 20000.);
  assert(frequency_readout::Label(440.) == "440.0 Hz | A4");
  assert(frequency_readout::Label(698.456462866) == "698.5 Hz | F5");
  assert(frequency_readout::Label(261.6255653) == "261.6 Hz | C4");
  assert(frequency_readout::Label(0.) == "0 Hz");
  for (double rate : {44100., 48000., 96000.})
  {
    // Uni-Vibe at a frozen lamp (Depth 0): chorus mode notches, vibrato mode flat.
    for (double mode : {0., 1.})
      for (double mix : {50., 100.})
        for (double hz : {300., 1500., 6000.})
        {
          UtilityControls c; c.modulation = {.05, 0., 40., 40., mix}; c.vibeMode = mode;
          grid_roadmap::Vibe vibe; vibe.Reset(rate);
          double inPower = 0., outPower = 0.;
          for (int i = 0; i < 24000; ++i)
          {
            const double input = .001 * std::sin(2. * grid_fx::pi * hz * i / rate);
            double output = input; vibe.Process(&output, 1, c);
            if (i >= 12000) { inPower += input * input; outPower += output * output; }
          }
          assert(std::abs(std::sqrt(outPower / inPower) - effect_preview::VibeMagnitude(hz, 0., c, rate)) < .003);
        }

    // Vibrato's peak pitch follows 1 - d(delay)/dt; compare the shortest
    // measured period of a 1 kHz tone with SweepCents.
    {
      UtilityControls c; c.modulation = {2., 50., 0., 0., 100.};
      SlotModulation vibrato; vibrato.Reset(rate);
      double previous = 0., lastCrossing = -1., shortest = 1.e9;
      for (int i = 0; i < static_cast<int>(rate * 2.); ++i)
      {
        double output = .5 * std::sin(2. * grid_fx::pi * 1000. * i / rate);
        vibrato.Process(&output, 1, SlotEffect::Vibrato, c);
        if (i > rate * .1 && previous < 0. && output >= 0.)
        {
          const double crossing = i - output / (output - previous);
          if (lastCrossing > 0.) shortest = std::min(shortest, crossing - lastCrossing);
          lastCrossing = crossing;
        }
        previous = output;
      }
      const double measured = 1200. * std::log2(rate / shortest / 1000.);
      assert(std::abs(measured - effect_preview::SweepCents(.006 * .5, 2.)) < 4.);
    }

    // Room/Hall shelve fixed ratios; Plate/Cloud decay shortens with frequency
    // through the in-loop damping and is unchanged far below it.
    assert(std::abs(effect_preview::ReverbDecay(SlotEffect::RoomReverb, 100., 4., 1., 3000., rate) - 4. * .65) < 1.e-12);
    assert(std::abs(effect_preview::ReverbDecay(SlotEffect::HallReverb, 5000., 4., 1., 3000., rate) - 4. * .35) < 1.e-12);
    assert(effect_preview::ReverbDecay(SlotEffect::RoomReverb, 1000., 4., 1., 3000., rate) == 4.);
    for (auto effect : {SlotEffect::PlateReverb, SlotEffect::CloudReverb})
    {
      assert(std::abs(effect_preview::ReverbDecay(effect, 20., 4., 1., 12000., rate) - 4.) < .01);
      const double bright = effect_preview::ReverbDecay(effect, 5000., 4., 1., 8000., rate);
      const double dark = effect_preview::ReverbDecay(effect, 5000., 4., 1., 1500., rate);
      assert(dark < bright && bright < 4. && dark > 0.);
    }

    // The analyzer reads a sine's peak amplitude in its band, including high
    // bands that span many FFT bins.
    for (double hz : {1000., 9700.})
    {
      grid_eq_signal::Window window;
      for (size_t i = 0; i < window.samples.size(); ++i)
        window.samples[i] = static_cast<float>(.5 * std::sin(2. * grid_fx::pi * hz * i / rate));
      const auto bands = grid_eq_signal::Analyze(window.samples, rate);
      const double upper = std::min(20000., .49 * rate);
      const auto band = static_cast<size_t>(std::lround(std::log(hz / 20.) / std::log(upper / 20.) * (grid_eq_signal::bands - 1)));
      assert(std::abs(bands[band] - 20.f * std::log10(.5f)) < 1.5f);
      assert(bands[2] < -30.f);
    }
  }
}

// The measured Cosmic Echo view and the module level strip report what the audio does.
void TestMeasuredVisualizations()
{
  // Aligned lines (Warp 0) give a few distinct spikes. Warp spreads the lines into a
  // wash, and Density then fills it further; at Warp 0, Density changes little.
  const auto filled = [](double warp, double density) {
    ImpulseProbe::Request request;
    request.effect = SlotEffect::CosmicEcho;
    request.controls.cosmicStyle = static_cast<int>(effect_modes::CosmicStyle::Centaurus);
    request.controls.cosmic = {120., warp, 60., density, 12000., 0., 100.};
    request.seconds = effect_preview::CosmicProbeSeconds(120.);
    const auto result = ImpulseProbe::Measure(request);
    assert(result.valid && result.seconds == request.seconds);
    const auto again = ImpulseProbe::Measure(request);
    assert(again.peak == result.peak); // Deterministic, so a cached result stays valid.
    const float loudest = *std::max_element(result.peak.begin(), result.peak.end());
    assert(loudest > 0.f);
    // Time bins within 20 dB of the loudest: how much of the window the response fills.
    return std::count_if(result.peak.begin(), result.peak.end(), [&](float peak) { return peak > .1f * loudest; });
  };
  assert(filled(0., 0.) < 10 && filled(0., 100.) < 10);
  assert(filled(50., 0.) > 4 * filled(0., 0.));
  assert(filled(50., 100.) > 2 * filled(50., 0.));
  assert(ImpulseProbe::Key(SlotEffect::CosmicEcho, {1., 2.}) != ImpulseProbe::Key(SlotEffect::CosmicEcho, {2., 1.}));

  // Gain is the RMS change; a clipped sine has a lower crest factor than a clean one.
  const auto compare = [](double gain, double clip) {
    LevelComparison levels;
    for (int block = 0; block < 100 && !levels.valid; ++block)
    {
      double in = 0., out = 0., inPeak = 0., outPeak = 0.;
      for (int i = 0; i < 480; ++i)
      {
        const double x = .25 * std::sin((block * 480 + i) * .05), y = std::clamp(gain * x, -clip, clip);
        in += x * x; out += y * y; inPeak = std::max(inPeak, std::abs(x)); outPeak = std::max(outPeak, std::abs(y));
      }
      levels.Receive(in, out, inPeak, outPeak, 480, 48000.);
    }
    assert(levels.valid);
    return levels;
  };
  const auto louder = compare(2., 10.);
  assert(std::abs(louder.GainDB() - 20. * std::log10(2.)) < .01);
  assert(std::abs(louder.input.crestDB - 3.01) < .05 && std::abs(louder.CrestChangeDB()) < .05);
  const auto clipped = compare(4., .3);
  assert(clipped.CrestChangeDB() < -1.5);
  // Silence is not compared, and empty packets expire a reading.
  LevelComparison quiet;
  quiet.Receive(1.e-12, 1.e-12, 1.e-6, 1.e-6, 48000, 48000.);
  assert(!quiet.valid);
  auto stale = louder;
  for (int i = 0; i < 31; ++i) stale.Receive(0., 0., 0., 0., 0, 48000.);
  assert(!stale.valid);
  std::cout << "Measured visualizations: Cosmic impulse blur and module level strip passed\n";
}
