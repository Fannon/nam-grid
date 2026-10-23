#pragma once
#include "EQResponse.h"

void TestEQResponse()
{
  // Compare the visualized magnitude with actual settled audio processing.
  for (double rate : {44100., 48000., 96000., 192000.})
    for (auto effect : {SlotEffect::ToneEQ, SlotEffect::CutEQ, SlotEffect::ParametricEQ, SlotEffect::SynthFilter})
      for (int variant = 0; variant < 3; ++variant)
        for (double hz : {250., 1000., 6000.})
        {
          if (variant > (effect == SlotEffect::SynthFilter ? 2 : effect == SlotEffect::ParametricEQ ? 1 : 0)) continue;
          EQControls eq; eq.bass = 6.; eq.mid = -5.; eq.treble = 8.;
          eq.highPass = 180.; eq.lowPass = 4000.; eq.presence = 3.;
          UtilityControls utility; utility.frequency = 1000.; utility.gain = 9.; utility.q = 2.; utility.eqMode = variant == 1;
          utility.cutoff = 1000.; utility.resonance = 2.; utility.envelope = 0.;
          utility.filterMode = variant;
          grid_eq_response::Response response(rate, effect, eq, utility);
          SlotEQ tone; tone.Reset(rate);
          SlotParametricEQ parametric; parametric.Reset(rate);
          SlotEnvelopeFilter filter; filter.Reset(rate);
          double inputPower = 0., outputPower = 0.;
          const int warmup = static_cast<int>(rate * .2), total = 2 * warmup;
          std::array<double, 512> audio{}, dry{};
          for (int pos = 0; pos < total;)
          {
            const int frames = std::min(512, total - pos);
            for (int i = 0; i < frames; ++i)
              dry[i] = audio[i] = .1 * std::sin(2. * MATH_PI * hz * (pos + i) / rate);
            if (effect == SlotEffect::SynthFilter) filter.Process(audio.data(), frames, utility);
            else if (effect == SlotEffect::ParametricEQ) parametric.Process(audio.data(), frames, utility, eq);
            else tone.Process(audio.data(), frames, effect == SlotEffect::CutEQ, eq);
            for (int i = 0; i < frames; ++i)
              if (pos + i >= warmup) { inputPower += dry[i] * dry[i]; outputPower += audio[i] * audio[i]; }
            pos += frames;
          }
          assert(std::abs(std::sqrt(outputPower / inputPower) - response.Magnitude(hz)) < 2.e-5);
        }
  // Boost/Tilt/Low cut preview uses the same filters as the audio path.
  for (double rate : {44100., 48000., 96000., 192000.})
    for (double tilt : {-6., 0., 6.})
      for (double hz : {250., 1000., 6000.})
      {
        UtilityControls c; c.boost = 6.; c.tilt = tilt;
        EQControls eq; eq.highPass = tilt == 0. ? 0. : 180.;
        grid_eq_response::Boost response(rate, c.boost, tilt, eq.highPass);
        SlotBoost processor; processor.Reset(rate);
        double inPower = 0., outPower = 0.;
        for (int n = 0; n < static_cast<int>(rate * .8); ++n)
        {
          const double input = .1 * std::sin(2. * MATH_PI * hz * n / rate);
          double output = input; processor.Process(&output, 1, c, eq);
          if (n >= static_cast<int>(rate * .3)) { inPower += input * input; outPower += output * output; }
        }
        assert(std::abs(std::sqrt(outPower / inPower) - std::abs(response.At(hz))) < 2.e-5);
      }
  EQControls flat; UtilityControls utility;
  for (auto effect : {SlotEffect::ToneEQ, SlotEffect::CutEQ, SlotEffect::ParametricEQ})
  {
    grid_eq_response::Response response(48000., effect, flat, utility);
    for (double hz : {20., 800., 20000.}) assert(std::abs(response.Magnitude(hz) - 1.) < 1.e-9);
  }
  // Display telemetry comes from both channels' applied coefficients, only while enabled.
  NAMStage stage;
  auto prepared = std::make_unique<NAMStage::Prepared>(); prepared->effect = SlotEffect::SynthFilter;
  stage.Publish(std::move(prepared)); stage.ResetStopped(48000., 512);
  utility.cutoff = 300.; utility.resonance = 1.6; utility.envelope = 3.;
  std::array<double, 512> left{}, right{};
  allocations = deletions = 0;
  for (int block = 0; block < 100; ++block)
  {
    left.fill(.25); right.fill(.001);
    tracking = true;
    stage.Process(left.data(), 512, true, 0., 0., {}, {}, utility, right.data());
    tracking = false;
  }
  const auto meter = stage.TakeMeter();
  assert(meter.filterCutoff[0] > 2 * meter.filterCutoff[1] && meter.filterCutoff[1] >= 300.f);
  assert(std::abs(meter.filterQ[0] - 1.6f) < 1.e-4f && std::abs(meter.filterQ[1] - 1.6f) < 1.e-4f);
  stage.Process(left.data(), 512, false, 0., 0., {}, {}, utility, right.data());
  assert((stage.TakeMeter().filterCutoff == std::array<float, 2>{}));
  assert(allocations == 0 && deletions == 0);
  for (double rate : {44100., 48000., 96000., 192000.})
  {
    SlotEnvelopeFilter whole, split; whole.Reset(rate); split.Reset(rate);
    allocations = deletions = 0;
    for (int block = 0; block < 120; ++block)
    {
      utility.filterMode = block / 20 % 3;
      utility.cutoff = block < 60 ? 300. : 6000.; utility.resonance = 4.;
      for (int i = 0; i < left.size(); ++i) left[i] = right[i] = .2 * std::sin((block * 512 + i) * .031);
      tracking = true;
      whole.Process(left.data(), 512, utility);
      for (int i = 0; i < right.size(); ++i) split.Process(&right[i], 1, utility);
      tracking = false;
      assert(left == right && allocations == 0 && deletions == 0);
      for (double sample : left) assert(std::isfinite(sample) && std::abs(sample) < 4.);
    }
  }
}
