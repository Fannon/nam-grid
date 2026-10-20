#pragma once
#include "SlotDynamics.h"

void TestDynamics()
{
  assert(SlotDynamics::CompressionDB(-30., -18., 4., 6.) == 0.);
  assert(std::abs(SlotDynamics::CompressionDB(-18., -18., 4., 6.) - .5625) < 1.e-12);
  assert(SlotDynamics::CompressionDB(-15., -18., 4., 6.) == 2.25);
  assert(SlotDynamics::CompressionDB(-6., -18., 4., 6.) == 9.);
  for (double rate : {44100., 48000., 96000.})
    for (int kind = 1; kind <= 4; ++kind)
    {
      const auto effect = static_cast<SlotEffect>(kind);
      DynamicsControls c;
      c.gateThreshold = -40.;
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
          stage.Process(a.data(), 512, block < 30, 12., 12., true, 12., 2, 1., c);
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
      stage.Process(&x, 1, true, 0., 0., false, 0., 0, 1., c);
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
    p->effect = slot == 0 ? SlotEffect::NaturalGate : SlotEffect::SmoothComp;
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
