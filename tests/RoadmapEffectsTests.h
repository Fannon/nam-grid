#pragma once
#include "SlotOperations.h"

void TestVibeCoefficientClock()
{
  for (double rate : {44100., 48000., 96000.})
  {
    grid_roadmap::Vibe changed, reference;
    changed.Reset(rate); reference.Reset(rate);
    UtilityControls c; c.modulation = {5., 0., 0., 50., 100.}; c.vibeMode = 1.;
    // Stop halfway through the sample-clock quantum, independently of block size.
    for (int i = 0; i < 1032; ++i)
    {
      double a = .1 * std::sin(.073 * i), b = a;
      changed.Process(&a, 1, c); reference.Process(&b, 1, c);
      assert(a == b);
    }
    auto moving = c; moving.modulation[1] = 100.;
    for (int i = 0; i < 8; ++i)
    {
      double a = .1 * std::sin(.073 * (1032 + i)), b = a;
      changed.Process(&a, 1, moving); reference.Process(&b, 1, c);
      assert(a == b); // Coefficients stay fixed until sample 1040.
    }
    double difference = 0.;
    for (int i = 0; i < 32; ++i)
    {
      double a = .1 * std::sin(.073 * (1040 + i)), b = a;
      changed.Process(&a, 1, moving); reference.Process(&b, 1, c);
      difference = std::max(difference, std::abs(a - b));
    }
    assert(difference > 1.e-8); // The next quantum still responds to Depth.
  }
}

void TestRoadmapEffects()
{
  TestVibeCoefficientClock();
  for (double rate : {44100., 48000., 96000., 192000.})
  {
    for (int id = static_cast<int>(SlotEffect::Rotary); id <= static_cast<int>(SlotEffect::Distortion); ++id)
    {
      SlotConfig cfg; cfg.effect = static_cast<SlotEffect>(id);
      NAMStage a, b;
      a.Publish(PrepareSlot(cfg, rate, 512).dsp); b.Publish(PrepareSlot(cfg, rate, 512).dsp);
      a.ResetStopped(rate, 512); b.ResetStopped(rate, 512);
      const int count = static_cast<int>(rate * .16);
      std::vector<double> l(count), r(count), expectedL(count), expectedR(count);
      for (int i = 0; i < count; ++i)
      {
        l[i] = expectedL[i] = .15 * std::sin(2. * grid_fx::pi * 220. * i / rate);
        r[i] = expectedR[i] = .1 * std::cos(2. * grid_fx::pi * 131. * i / rate);
      }
      UtilityControls c; c.delay[0] = 35.; c.stutter[0] = 20.;
      for (int pos = 0; pos < count; pos += 512)
        a.Process(expectedL.data() + pos, std::min(512, count - pos), true, 0, 0, {}, {}, c, expectedR.data() + pos);
      int block = 0;
      for (int pos = 0; pos < count;)
      {
        const int n = std::min(std::array<int, 5>{1, 3, 127, 64, 512}[block++ % 5], count - pos);
        allocations = deletions = 0; tracking = true; Eigen::internal::set_is_malloc_allowed(false);
        b.Process(l.data() + pos, n, true, 0, 0, {}, {}, c, r.data() + pos);
        Eigen::internal::set_is_malloc_allowed(true); tracking = false;
        assert(allocations == 0 && deletions == 0);
        pos += n;
      }
      double energy = 0.;
      for (int i = 0; i < count; ++i)
      {
        assert(std::isfinite(l[i]) && std::isfinite(r[i]));
        assert(std::abs(l[i] - expectedL[i]) < 1.e-9 && std::abs(r[i] - expectedR[i]) < 1.e-9);
        energy += l[i] * l[i] + r[i] * r[i];
      }
      assert(energy > .001);
      std::array<double, 512> x{}, y{};
      for (int n = 0; n < 8; ++n)
      {
        x.fill(.123); y.fill(-.234);
        b.Process(x.data(), 512, false, 0, 0, {}, {}, c, y.data());
      }
      assert(std::abs(x.back() - .123) < 1.e-12 && std::abs(y.back() + .234) < 1.e-12);
    }

    // Fractional timing has exact zero-delay passthrough, and a half-sample
    // impulse splits between neighboring samples without changing its sum.
    StereoUtility utility; utility.Reset(rate); UtilityControls c;
    c.fineDelay = 10.5 * 1000. / rate;
    std::array<double, 512> l{}, r{}; l[0] = 1.; r[0] = -.5;
    utility.Process(l.data(), r.data(), 512, c);
    assert(std::abs(l[10] - .5) < 1.e-10 && std::abs(l[11] - .5) < 1.e-10);
    assert(std::abs(r[10] + .25) < 1.e-10 && std::abs(r[11] + .25) < 1.e-10);

    grid_roadmap::Limiter limiter; limiter.Reset(rate); c = {}; c.limiter = {-6., 75.};
    for (int block = 0; block < 8; ++block)
    {
      for (int i = 0; i < 512; ++i) { l[i] = 4. * std::sin(i * .15 + block); r[i] = -.25 * l[i]; }
      limiter.Process(l.data(), r.data(), 512, c);
      for (int i = 0; i < 512; ++i)
      {
        assert(std::abs(l[i]) <= std::pow(10., -.3) + 1.e-12);
        assert(std::abs(r[i] + .25 * l[i]) < 1.e-12); // stereo-linked gain
      }
    }
    assert(limiter.Reduction() > 6.);
    {
      // Clip: 0% is the plain limiter exactly; 100% still holds the ceiling, and shaving
      // peaks first leaves the limiter less gain reduction to apply.
      const auto run = [&](double clip, std::array<double, 512>& out) {
        grid_roadmap::Limiter clipped; clipped.Reset(rate); UtilityControls cc; cc.limiter = {-6., 75.}; cc.limiterClip = clip;
        double reduction = 0.;
        for (int block = 0; block < 8; ++block)
        {
          for (int i = 0; i < 512; ++i) { out[i] = 1.2 * std::sin(i * .15 + block); }
          clipped.Process(out.data(), nullptr, 512, cc);
          for (double x : out) assert(std::abs(x) <= std::pow(10., -.3) + 1.e-12);
          reduction = std::max(reduction, clipped.Reduction());
        }
        return reduction;
      };
      std::array<double, 512> plain{}, none{}, full{};
      grid_roadmap::Limiter reference; reference.Reset(rate); UtilityControls rc; rc.limiter = {-6., 75.};
      for (int block = 0; block < 8; ++block)
      {
        for (int i = 0; i < 512; ++i) plain[i] = 1.2 * std::sin(i * .15 + block);
        reference.Process(plain.data(), nullptr, 512, rc);
      }
      const double withoutClip = run(0., none), withClip = run(100., full);
      assert(none == plain);
      assert(withClip < withoutClip - 1.);
      assert(std::abs(grid_roadmap::Limiter::SoftClip(.1, .5) - .1) < 1.e-15); // Below the knee: untouched.
      assert(grid_roadmap::Limiter::SoftClip(100., .5) <= .5 * 1.41253754462 + 1.e-12); // Never past +3 dB.
    }

    // A held capture continues after input stops and resets to actual silence.
    grid_roadmap::Capture capture; capture.Reset(rate); c = {}; c.freeze[1] = 50.;
    for (int block = 0; block < 30; ++block)
    {
      for (int i = 0; i < 512; ++i) l[i] = r[i] = .2 * std::sin((block * 512 + i) * .07);
      capture.Process(l.data(), r.data(), 512, SlotEffect::Freeze, c);
    }
    c.freeze[0] = 1.; double held = 0.;
    for (int block = 0; block < 15; ++block)
    {
      l.fill(0.); r.fill(0.); capture.Process(l.data(), r.data(), 512, SlotEffect::Freeze, c);
      for (double x : l) held += x * x;
    }
    assert(held > .01);
    capture.Reset(rate); l.fill(0.); r.fill(0.); capture.Process(l.data(), r.data(), 512, SlotEffect::Freeze, c);
    for (double x : l) assert(x == 0.);
  }
  UtilityControls c;
  std::array<double, 512> x{};
  // The expanded EQ's notch rejects its selected center without requiring gain.
  SlotParametricEQ notch; notch.Reset(48000.); c = {}; c.frequency = 1000.; c.q = 8.; c.eqMode = 1.;
  double notchEnergy = 0.;
  for (int block = 0; block < 60; ++block)
  {
    for (int i = 0; i < 512; ++i) x[i] = .2 * std::sin(2. * grid_fx::pi * 1000. * (block * 512 + i) / 48000.);
    notch.Process(x.data(), 512, c, {});
    if (block >= 50) for (double value : x) notchEnergy += value * value;
  }
  assert(notchEnergy / 5120. < 1.e-8);
  grid_roadmap::Vibe chorus, vibrato, changing;
  chorus.Reset(48000.); vibrato.Reset(48000.); changing.Reset(48000.);
  std::array<double, 512> chorusOut{}, vibratoOut{}, changingOut{};
  for (int block = 0; block < 4; ++block)
  {
    for (int i = 0; i < 512; ++i)
      chorusOut[i] = vibratoOut[i] = changingOut[i] = .2 * std::sin((block * 512 + i) * .1);
    c = {}; chorus.Process(chorusOut.data(), 512, c);
    c.vibeMode = 1.; vibrato.Process(vibratoOut.data(), 512, c);
    c.vibeMode = block == 3 ? 1. : 0.; changing.Process(changingOut.data(), 512, c);
    if (block == 3) for (int i = 0; i < 512; ++i)
    {
      assert(changingOut[i] >= std::min(chorusOut[i], vibratoOut[i]) - 1.e-12);
      assert(changingOut[i] <= std::max(chorusOut[i], vibratoOut[i]) + 1.e-12);
      if (i >= 240) assert(std::abs(changingOut[i] - vibratoOut[i]) < 1.e-12);
    }
  }
  for (double rate : {44100., 48000., 96000.})
  {
    FixedNAMGrid grid; grid.ResetStopped(rate, 512);
    std::array<FixedNAMGrid::Controls, 16> controls{};
    grid_performance::Routing route;
    std::array<double, 512> l{}, r{}, originalL{}, originalR{};
    for (int block = 0; block < 20; ++block)
    {
      route.crossover = block % 2 != 0;
      for (int i = 0; i < 512; ++i)
      {
        l[i] = originalL[i] = .2 * std::sin((block * 512 + i) * .023);
        r[i] = originalR[i] = .1 * std::cos((block * 512 + i) * .13);
      }
      allocations = deletions = 0; tracking = true;
      grid.Process(l.data(), 512, controls, .5, r.data(), route);
      tracking = false; assert(allocations == 0 && deletions == 0);
      for (int i = 0; i < 512; ++i)
      {
        assert(std::abs(l[i] - originalL[i]) < 1.e-12);
        assert(std::abs(r[i] - originalR[i]) < 1.e-12);
      }
    }
    grid.ResetStopped(rate, 512); route = {}; route.timing = 8.5 * 1000. / rate;
    l.fill(0.); r.fill(0.); l[0] = 1.; r[0] = -.5;
    grid.Process(l.data(), 512, controls, 0., r.data(), route);
    assert(std::abs(l[8] - .5) < 1.e-10 && std::abs(l[9] - .5) < 1.e-10);
    grid.ResetStopped(rate, 512); route = {}; route.soloA = true;
    l.fill(.2); r.fill(-.1); grid.Process(l.data(), 512, controls, .5, r.data(), route);
    assert(std::abs(l.back() - .1) < 1.e-12 && std::abs(r.back() + .05) < 1.e-12);
  }
  // Shared sources preserve their clock across arbitrary host block boundaries.
  const auto renderModulation = [](bool variable) {
    grid_performance::Modulation mod; mod.Reset(48000.);
    auto v = grid_performance::Defaults();
    std::vector<double> values(16384); std::array<double, 512> signal{}; signal.fill(.1);
    int block = 0;
    for (int pos = 0; pos < values.size();)
    {
      const int n = std::min(static_cast<int>(values.size()) - pos, variable ? std::array<int, 4>{1, 127, 3, 512}[block++ % 4] : 512);
      for (int offset = 0; offset < n;)
      {
        const int count = std::min(mod.Quantum(), n - offset);
        using namespace grid_performance;
        const double value = .35 * mod.Value(SourceLFO, v) + .2 * mod.Value(SourceEnvelope, v) + .1 * mod.Value(SourceSequencer, v);
        std::fill_n(values.data() + pos + offset, count, value);
        mod.Advance(signal.data(), nullptr, count, v, 120.); offset += count;
      }
      pos += n;
    }
    return values;
  };
  const auto fixed = renderModulation(false), variable = renderModulation(true);
  assert(fixed == variable && std::abs(fixed.front() - fixed.back()) > .1);
  std::cout << "Roadmap effects: variable partitions, stereo limiter, fractional timing, hold/reset and octave spectrum passed\n";
}
