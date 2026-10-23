#pragma once
#include "EffectPreview.h"
#include "EQSignalSpectrum.h"
#include "NAMStage.h"

// Checks the parameter-interaction previews against the actual processors:
// Uni-Vibe notches, delay-sweep pitch, reverb band decay and the EQ analyzer.
void TestVisualizationInteractions()
{
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
