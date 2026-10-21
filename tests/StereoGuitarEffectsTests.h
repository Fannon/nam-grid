#pragma once
#include "SlotOperations.h"

void TestStereoGuitarEffects()
{
  auto stageFor = [](SlotEffect effect, double rate) {
    SlotConfig cfg;
    cfg.effect = effect;
    auto stage = std::make_unique<NAMStage>();
    stage->Publish(PrepareSlot(cfg, rate, 512).dsp);
    stage->ResetStopped(rate, 512);
    assert(stage->Latency() == 0);
    return stage;
  };
  auto process = [](NAMStage& stage, double* l, double* r, int n, const UtilityControls& c, bool on = true) {
    stage.Process(l, n, on, 0, 0, false, 0, 0, 1, {}, {}, c, r);
  };
  const std::array effects{SlotEffect::DigitalDelay, SlotEffect::AnalogDelay,  SlotEffect::TapeDelay,
                           SlotEffect::Chorus,       SlotEffect::RoomReverb,   SlotEffect::PlateReverb,
                           SlotEffect::CloudReverb,  SlotEffect::HallReverb, SlotEffect::SpringReverb, SlotEffect::Wah};
  for (double rate : {44100., 48000., 88200., 96000., 192000.})
    for (auto effect : effects)
    {
      auto stage = stageFor(effect, rate), reference = stageFor(effect, rate);
      UtilityControls c;
      c.delay[0] = 40.;
      c.pingPong = true;
      c.spring[0] = .5;
      const int length = static_cast<int>(rate * .25);
      std::vector<double> l(length), r(length), expectedL(length), expectedR(length);
      for (int i = 0; i < length; ++i)
      {
        l[i] = expectedL[i] = .15 * std::sin(2. * grid_fx::pi * 223. * i / rate);
        r[i] = expectedR[i] = .1 * std::cos(2. * grid_fx::pi * 347. * i / rate);
      }
      for (int pos = 0; pos < length; pos += 512)
        process(*reference, expectedL.data() + pos, expectedR.data() + pos, std::min(512, length - pos), c);
      int block = 0;
      for (int pos = 0; pos < length;)
      {
        const int n = std::min(length - pos, std::array<int, 6>{1, 3, 127, 64, 256, 512}[block++ % 6]);
        allocations = deletions = 0;
        tracking = true;
        Eigen::internal::set_is_malloc_allowed(false);
        process(*stage, l.data() + pos, r.data() + pos, n, c);
        Eigen::internal::set_is_malloc_allowed(true);
        tracking = false;
        assert(allocations == 0 && deletions == 0);
        pos += n;
      }
      for (int i = 0; i < length; ++i)
        assert(std::isfinite(l[i]) && std::isfinite(r[i]) && std::abs(l[i] - expectedL[i]) < 1.e-9
               && std::abs(r[i] - expectedR[i]) < 1.e-9);
      std::array<double, 512> left{}, right{};
      // Live extremes, including switches, remain finite and allocation-free.
      for (int b = 0; b < 24; ++b)
      {
        const bool high = b % 2;
        for (int j = 0; j < 5; ++j)
        {
          c.spring[j] =
            high ? kEffectParameters[uSpringDecay + j].maximum : kEffectParameters[uSpringDecay + j].minimum;
          c.wah[j] = high ? kEffectParameters[uWahPosition + j].maximum : kEffectParameters[uWahPosition + j].minimum;
        }
        c.stereoReverb = high;
        c.pingPong = high;
        c.chorusSpread = high ? 100. : 0.;
        c.reverb[2] = high ? 12000. : 800.;
        left.fill(.2);
        right.fill(-.1);
        allocations = deletions = 0;
        tracking = true;
        Eigen::internal::set_is_malloc_allowed(false);
        process(*stage, left.data(), right.data(), 512, c);
        Eigen::internal::set_is_malloc_allowed(true);
        tracking = false;
        assert(allocations == 0 && deletions == 0);
        for (int i = 0; i < 512; ++i)
          assert(std::isfinite(left[i]) && std::isfinite(right[i]) && std::abs(left[i]) < 12.
                 && std::abs(right[i]) < 12.);
      }
      for (int b = 0; b < 8; ++b)
      {
        left.fill(.123);
        right.fill(-.25);
        process(*stage, left.data(), right.data(), 512, c, false);
      }
      for (int i = 0; i < 512; ++i)
        assert(left[i] == .123 && right[i] == -.25);
      stage->ResetStopped(rate, 1);
      left[0] = .1;
      right[0] = -.1;
      process(*stage, left.data(), right.data(), 1, c);
      assert(std::isfinite(left[0]) && std::isfinite(right[0]));
    }
  // Ping-pong's first three repeats alternate sides. Cross-feedback is sample based,
  // independent of host partitioning, and retains the dry stereo signal at Mix=0.
  for (double rate : {44100., 48000., 96000.})
  {
    auto stage = stageFor(SlotEffect::DigitalDelay, rate);
    UtilityControls c;
    c.pingPong = true;
    c.delay = {40., 50., 12000., 0., 100.};
    const int delay = static_cast<int>(rate * .04), length = delay * 4;
    std::vector<double> l(length), r(length);
    l[0] = r[0] = 1.;
    for (int pos = 0; pos < length; pos += 127)
      process(*stage, l.data() + pos, r.data() + pos, std::min(127, length - pos), c);
    auto energy = [&](const auto& x, int start) {
      double sum = 0.;
      for (int i = start; i < start + 32; ++i)
        sum += x[i] * x[i];
      return sum;
    };
    assert(energy(l, delay) > .01 && energy(r, delay) < 1.e-15);
    assert(energy(r, 2 * delay) > .001 && energy(l, 2 * delay) < 1.e-15);
    assert(energy(l, 3 * delay) > .0001 && energy(r, 3 * delay) < 1.e-15);
  }
  // Both chorus and reverbs create side energy from a mono source; a centered
  // chorus and stereo-off reverb return equal wet channels.
  for (auto effect : {SlotEffect::Chorus, SlotEffect::RoomReverb, SlotEffect::PlateReverb, SlotEffect::CloudReverb,
                      SlotEffect::HallReverb, SlotEffect::SpringReverb})
    for (bool stereo : {false, true})
    {
      auto stage = stageFor(effect, 48000.);
      UtilityControls c;
      c.stereoReverb = stereo;
      c.chorusSpread = stereo ? 100. : 0.;
      c.reverb[4] = c.spring[3] = 100.;
      std::array<double, 512> l{}, r{};
      double side = 0., total = 0.;
      for (int b = 0; b < 100; ++b)
      {
        for (int i = 0; i < 512; ++i)
          l[i] = r[i] =
            effect == SlotEffect::Chorus ? .1 * std::sin((b * 512 + i) * .07) : (b == 0 && i == 0 ? 1. : 0.);
        process(*stage, l.data(), r.data(), 512, c);
        for (int i = 0; i < 512; ++i)
        {
          side += (l[i] - r[i]) * (l[i] - r[i]);
          total += l[i] * l[i] + r[i] * r[i];
        }
      }
      assert(total > 1.e-7);
      if (stereo)
        assert(side > 1.e-6);
      else
        assert(side < 1.e-20);
    }
  // Utility's requested centered scale is exact in meaning; widening preserves
  // the mono fold and does not invent stereo from equal channels.
  for (double stereo : {-100., 0., 100.})
  {
    auto stage = stageFor(SlotEffect::Utility, 48000.);
    UtilityControls c;
    c.stereo = stereo;
    double l = .3, r = .1;
    process(*stage, &l, &r, 1, c);
    const double side = .1 * (1. + stereo * .01);
    assert(std::abs(l - (.2 + side)) < 1.e-12 && std::abs(r - (.2 - side)) < 1.e-12);
  }
  // Wah position moves the resonance across the guitar range; zero Mix is dry.
  auto magnitude = [&](double position, double hz, double mix) {
    auto stage = stageFor(SlotEffect::Wah, 48000.);
    UtilityControls c;
    c.wah[0] = position;
    c.wah[3] = mix;
    std::array<double, 512> l{}, r{};
    double energy = 0.;
    for (int b = 0; b < 40; ++b)
    {
      for (int i = 0; i < 512; ++i)
        l[i] = r[i] = .1 * std::sin(2. * grid_fx::pi * hz * (b * 512 + i) / 48000.);
      process(*stage, l.data(), r.data(), 512, c);
      if (b >= 20)
        for (auto x : l)
          energy += x * x;
    }
    return energy;
  };
  assert(magnitude(0., 354., 100.) > magnitude(100., 354., 100.) * 4.);
  assert(magnitude(100., 2404., 100.) > magnitude(0., 2404., 100.) * 4.);
  assert(std::abs(magnitude(0., 1000., 0.) - magnitude(100., 1000., 0.)) < 1.e-12);
  // Drip changes the impulse response, and the short tank setting decays to silence.
  std::array<std::vector<double>, 2> springIR;
  for (int variant = 0; variant < 2; ++variant)
  {
    auto stage = stageFor(SlotEffect::SpringReverb, 48000.);
    UtilityControls c;
    c.spring = {.3, 0., variant ? 100. : 0., 100., 0.};
    auto& x = springIR[variant];
    x.assign(48000 * 4, 0.);
    x[0] = 1.;
    for (int pos = 0; pos < x.size(); pos += 512)
      stage->Process(
        x.data() + pos, std::min(512, static_cast<int>(x.size()) - pos), true, 0, 0, false, 0, 0, 1, {}, {}, c);
    double early = 0., late = 0.;
    for (int i = 2400; i < 24000; ++i)
      early += x[i] * x[i];
    for (int i = 48000 * 3; i < x.size(); ++i)
      late += x[i] * x[i];
    assert(early > 1.e-8 && late < early * .001);
  }
  double difference = 0.;
  for (int i = 0; i < springIR[0].size(); ++i)
    difference += std::abs(springIR[0][i] - springIR[1][i]);
  assert(difference > .01);
  std::cout << "Stereo echo/chorus/reverb, dispersive spring, Wah and centered Utility: "
               "rate/block/bypass/automation/allocation checks passed\n";
}
