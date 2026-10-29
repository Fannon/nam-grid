#pragma once
#include "SlotDynamics.h"

// A cached makeup factor must follow the smoothed dB value, not just the
// target knob. Verify the audio law independently using a zero-makeup wet
// reference, including mid-ramp retargeting, parallel mix and Reset reuse.
void TestCompressorMakeupGain()
{
  for (double rate : {44100., 48000., 96000., 192000.})
    for (auto effect : {SlotEffect::SmoothComp, SlotEffect::PunchComp})
      for (double mix : {0., 35., 100.})
      {
        SlotDynamics processor, wet;
        processor.Reset(rate); wet.Reset(rate);
        DynamicsControls controls;
        controls.compThreshold = -30.;
        controls.compMakeup = 6.;
        controls.compMix = mix;
        controls.detectorCut = 120.;
        double smoothedDB = controls.compMakeup;
        const double smoothing = std::exp(-1. / (.005 * rate));
        for (int pass = 0; pass < 2; ++pass)
        {
          std::array<double, 512> input{}, actual{}, compressed{}, monitor{};
          for (int block = 0; block < 400; ++block)
          {
            const int frames = std::array<int, 6>{1, 3, 64, 127, 256, 512}[block % 6];
            // Closely spaced reversals must not replace the ongoing dB ramp
            // with a block-rate step or a linear-amplitude crossfade.
            if (block == 20 || block == 150) controls.compMakeup = 18.;
            if (block == 21 || block == 80 || block == 260) controls.compMakeup = 0.;
            if (block == 22 || block == 180) controls.compMakeup = 9.;
            auto reference = controls;
            reference.compMakeup = 0.; reference.compMix = 100.;
            for (int i = 0; i < frames; ++i)
              input[i] = actual[i] = compressed[i] = (block < 300 ? .6 : .001) * std::sin((block * 512 + i) * .071);
            allocations = deletions = 0;
            tracking = true;
            processor.Process(actual.data(), frames, effect, controls, monitor.data());
            wet.Process(compressed.data(), frames, effect, reference);
            tracking = false;
            assert(allocations == 0 && deletions == 0);
            double minimum = 1.;
            for (int i = 0; i < frames; ++i)
            {
              smoothedDB = controls.compMakeup + smoothing * (smoothedDB - controls.compMakeup);
              if (std::abs(smoothedDB) < 1.e-30) smoothedDB = 0.;
              const double makeup = std::pow(10., smoothedDB / 20.);
              const double expected = input[i] + mix * .01 * (makeup * compressed[i] - input[i]);
              const double expectedMonitor = input[i] + mix * .01 * (compressed[i] - input[i]);
              assert(std::abs(actual[i] - expected) < 2.e-13);
              assert(std::abs(monitor[i] - expectedMonitor) < 2.e-14);
              if (input[i] != 0.) minimum = std::min(minimum, expectedMonitor / input[i]);
              if (mix == 0.) assert(actual[i] == input[i] && monitor[i] == input[i]);
            }
            assert(std::abs(processor.PeakReduction() + 20. * std::log10(std::max(1.e-12, minimum))) < 2.e-12);
          }
          // Start the same instances with fresh makeup after Reset; a gain
          // cached in the previous pass must not survive it.
          controls.compMakeup = pass == 0 ? 0. : 6.;
          smoothedDB = controls.compMakeup;
          processor.Reset(rate); wet.Reset(rate);
        }
      }
}

void TestDynamics()
{
  TestCompressorMakeupGain();
  // Monitoring excludes makeup and follows the actual parallel mix, while the
  // processed output retains its configured compensation.
  for (auto effect : {SlotEffect::SmoothComp, SlotEffect::PunchComp})
    for (double mix : {0., 35., 100.})
    {
      SlotDynamics compensated, reference; compensated.Reset(48000.); reference.Reset(48000.);
      DynamicsControls c; c.compMakeup = 12.; c.compMix = mix;
      auto neutral = c; neutral.compMakeup = 0.;
      std::array<double, 128> audio{}, expected{}, monitor{};
      for (int block = 0; block < 300; ++block)
      {
        for (int i = 0; i < 128; ++i) audio[i] = expected[i] = .7 * std::sin((block * 128 + i) * .071);
        compensated.Process(audio.data(), 128, effect, c, monitor.data());
        reference.Process(expected.data(), 128, effect, neutral);
        for (int i = 0; i < 128; ++i) assert(std::abs(monitor[i] - expected[i]) < 1.e-12);
      }
      assert(std::abs(compensated.PeakReduction() - reference.PeakReduction()) < 1.e-12);
      if (mix == 0.) assert(compensated.PeakReduction() == 0.);
    }
  assert(SlotDynamics::CompressionDB(-30., -18., 4., 6.) == 0.);
  assert(std::abs(SlotDynamics::CompressionDB(-18., -18., 4., 6.) - .5625) < 1.e-12);
  assert(SlotDynamics::CompressionDB(-15., -18., 4., 6.) == 2.25);
  assert(SlotDynamics::CompressionDB(-6., -18., 4., 6.) == 9.);
  for (double rate : {44100., 48000., 96000., 192000.})
  {
    // Gate Mode is the only Natural/Tight selector: Tight closes gaps to the full
    // reduction, Natural expands gradually. Mode changes never allocate.
    SlotDynamics natural, tight;
    natural.Reset(rate); tight.Reset(rate);
    DynamicsControls naturalMode, tightMode;
    tightMode.gateMode = 1;
    std::array<double, 512> naturalOut{}, tightOut{};
    double naturalGap = 0., tightGap = 0.;
    allocations = deletions = 0;
    for (int block = 0; block < 400; ++block)
    {
      for (int i = 0; i < naturalOut.size(); ++i)
        naturalOut[i] = tightOut[i] = (block % 80 < 20 ? .2 : .0001) * std::sin((block * 512 + i) * .031);
      tracking = true;
      natural.Process(naturalOut.data(), 512, SlotEffect::NoiseGate, naturalMode);
      tight.Process(tightOut.data(), 512, SlotEffect::NoiseGate, tightMode);
      tracking = false;
      assert(allocations == 0 && deletions == 0);
      for (int i = 0; i < naturalOut.size(); ++i)
      {
        assert(std::isfinite(naturalOut[i]) && std::isfinite(tightOut[i]));
        if (block % 80 >= 40)
        {
          naturalGap += std::abs(naturalOut[i]);
          tightGap += std::abs(tightOut[i]);
        }
      }
    }
    assert(tightGap < naturalGap);
  }
  for (double rate : {44100., 48000., 96000.})
    for (int kind = 1; kind <= 4; ++kind)
    {
      // 1/2: Natural/Tight gate, 3: Smooth, 4: Punch compressor.
      const auto effect = kind <= 2 ? SlotEffect::NoiseGate : kind == 3 ? SlotEffect::SmoothComp : SlotEffect::PunchComp;
      DynamicsControls c;
      c.gateThreshold = -40.;
      c.gateMode = kind == 2 ? 1 : 0;
      SlotDynamics positive, negative;
      positive.Reset(rate);
      negative.Reset(rate);
      double last = 0.;
      allocations = deletions = 0;
      tracking = true;
      for (int i = 0; i < static_cast<int>(rate * 3.); ++i)
      {
        double a = kind <= 2 ? .001 : .5, b = -a;
        positive.Process(&a, 1, effect, c);
        negative.Process(&b, 1, effect, c);
        assert(std::isfinite(a) && a == -b);
        last = a;
      }
      tracking = false;
      assert(allocations == 0 && deletions == 0);
      const double expected =
        kind == 1   ? .0001
        : kind == 2 ? .000001
                    : .5
                        * std::pow(10., -SlotDynamics::CompressionDB(
                                          20. * std::log10(.5), -18., kind == 3 ? 2.5 : 4., kind == 3 ? 9. : 3.)
                                          * .05);
      assert(std::abs(last - expected) < 1.e-8);
      // Constant settings must give identical samples across different block partitions.
      SlotDynamics single, blocked;
      single.Reset(rate);
      blocked.Reset(rate);
      std::array<double, 512> a{}, b{};
      for (int block = 0; block < 200; ++block)
      {
        const int frames = std::array<int, 6>{1, 3, 64, 127, 256, 512}[block % 6];
        c.gateThreshold = block < 60 ? -60. : -35.;
        c.compThreshold = block < 60 ? -24. : -12.;
        c.gateRelease = c.compRelease = block < 100 ? 40. : 400.;
        c.gateRange = block < 120 ? 80. : 0.;
        c.compMakeup = block < 120 ? 0. : 6.;
        for (int i = 0; i < frames; ++i)
          a[i] = b[i] = block < 80 ? .2 * std::sin((block * 512 + i) * .027) : 0.;
        tracking = true;
        blocked.Process(b.data(), frames, effect, c);
        for (int i = 0; i < frames; ++i)
          single.Process(&a[i], 1, effect, c);
        tracking = false;
        for (int i = 0; i < frames; ++i)
          assert(a[i] == b[i] && std::isfinite(b[i]));
      }
      assert(allocations == 0 && deletions == 0);
      if (kind <= 2)
      {
        SlotDynamics gate;
        gate.Reset(rate);
        c = {};
        c.gateThreshold = -40.;
        for (int i = 0; i < static_cast<int>(rate * .1); ++i)
        {
          double x = .1;
          gate.Process(&x, 1, effect, c);
        }
        for (int i = 0; i < static_cast<int>(rate * .01); ++i)
        {
          double x = .0001;
          gate.Process(&x, 1, effect, c);
          assert(x > .000099); // Hold must protect a short gap.
        }
        c.gateRange = 0.;
        double x = 0.;
        for (int i = 0; i < static_cast<int>(rate); ++i)
        {
          x = .0001;
          gate.Process(&x, 1, effect, c);
        }
        assert(std::abs(x - .0001) < 1.e-9);
      }
      NAMStage stage;
      for (int replacement = 0; replacement < 5; ++replacement)
      {
        auto prepared = std::make_unique<NAMStage::Prepared>();
        prepared->effect = effect;
        prepared->Reset(rate, 512);
        stage.Publish(std::move(prepared));
        if (replacement == 0)
          stage.ResetStopped(rate, 512);
        assert(stage.Latency() == 0);
        for (int block = 0; block < 60; ++block)
        {
          a.fill(.1);
          tracking = true;
          stage.Process(a.data(), 512, block < 30, 12., 12., c);
          tracking = false;
          for (auto value : a)
            assert(std::isfinite(value) && (block < 35 || value == .1));
        }
        stage.Collect();
      }
      assert(allocations == 0 && deletions == 0);
      stage.ResetStopped(rate, 1);
      double x = .1;
      tracking = true;
      stage.Process(&x, 1, true, 0., 0., c);
      tracking = false;
      assert(std::isfinite(x) && allocations == 0 && deletions == 0);
    }
  // Attack changes pick-transient retention; parallel mix leaves dry untouched
  // and reaches the arithmetic midpoint of dry and compressed signals.
  for (auto effect : {SlotEffect::SmoothComp, SlotEffect::PunchComp})
  {
    DynamicsControls fastControls, slowControls, dryControls, halfControls;
    fastControls.compThreshold = -30.;
    fastControls.compAttack = fastControls.punchAttack = .5;
    slowControls = fastControls;
    slowControls.compAttack = slowControls.punchAttack = 50.;
    dryControls = fastControls;
    dryControls.compMix = 0.;
    halfControls = fastControls;
    halfControls.compMix = 50.;
    SlotDynamics fast, slow, dry, half;
    for (auto* processor : {&fast, &slow, &dry, &half})
      processor->Reset(48000.);
    double a = 0., b = 0.;
    allocations = deletions = 0;
    tracking = true;
    for (int i = 0; i < 1000; ++i)
    {
      a = b = .5;
      double d = .5, h = .5;
      fast.Process(&a, 1, effect, fastControls);
      slow.Process(&b, 1, effect, slowControls);
      dry.Process(&d, 1, effect, dryControls);
      half.Process(&h, 1, effect, halfControls);
      assert(d == .5 && std::abs(h - .5 * (.5 + a)) < 1.e-12);
    }
    tracking = false;
    assert(allocations == 0 && deletions == 0);
    assert(b > a * 1.5);
  }
  // Mixed NAM/dynamics pipeline: gate -> x2 -> parallel(x3, dry) -> compressor.
  FixedNAMGrid mixed;
  const nlohmann::json linear{{"architecture", "Linear"},
                              {"config", {{"receptive_field", 1}, {"bias", true}}},
                              {"weights", {2., 0.}},
                              {"sample_rate", 48000.}};
  mixed.stages[1].Publish(prepare(linear, 48000.));
  auto triple = linear;
  triple["weights"] = {3., 0.};
  mixed.stages[4].Publish(prepare(triple, 48000.));
  for (const auto slot : {0, 12})
  {
    auto p = std::make_unique<NAMStage::Prepared>();
    p->effect = slot == 0 ? SlotEffect::NoiseGate : SlotEffect::SmoothComp;
    mixed.stages[slot].Publish(std::move(p));
  }
  mixed.ResetStopped(48000., 512);
  std::array<FixedNAMGrid::Controls, 16> controls{};
  std::array<double, 512> audio{};
  allocations = deletions = 0;
  tracking = true;
  for (int block = 0; block < 300; ++block)
  {
    audio.fill(.1);
    mixed.Process(audio.data(), 512, controls, .5);
  }
  tracking = false;
  const double expected = .4 * std::pow(10., -.05 * SlotDynamics::CompressionDB(20. * std::log10(.4), -18., 2.5, 9.));
  assert(std::abs(audio.back() - expected) < 1.e-7);
  assert(allocations == 0 && deletions == 0);
  std::cout << "Dynamics curves, polarity, hold, automation, block invariance, bypass, mixed NAM grid and realtime "
               "ownership passed\n";
}
