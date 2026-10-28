#pragma once
#include "SlotOperations.h"

// Exercise the actual slot wrapper, including its preparation, bypass and
// replacement path, rather than testing isolated mathematical helper functions.
void TestTimeEffects()
{
  const auto process = [](NAMStage& stage, double* x, int n, const UtilityControls& c, bool on = true) {
    stage.Process(x, n, on, 0., 0., {}, {}, c);
  };
  const auto prepareEffect = [](SlotEffect effect, double rate) {
    SlotConfig config;
    config.effect = effect;
    auto stage = std::make_unique<NAMStage>();
    stage->Publish(PrepareSlot(config, rate, 512).dsp);
    stage->ResetStopped(rate, 512);
    assert(stage->Latency() == 0);
    return stage;
  };
  for (double rate : {44100., 48000., 88200., 96000., 192000.})
  {
    for (int id : {10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, static_cast<int>(SlotEffect::HallReverb)})
    {
      const auto effect = static_cast<SlotEffect>(id);
      auto stage = prepareEffect(effect, rate), reference = prepareEffect(effect, rate);
      UtilityControls c;
      c.delay[0] = 45.;
      c.reverb[0] = .7;
      c.reverb[3] = 0.;
      // Identical continuous input, different block partitions: catches block-rate
      // envelopes, LFO resets, delay-pointer errors and accidental buffer latency.
      const int count = static_cast<int>(rate * .35);
      std::vector<double> expected(count), actual(count);
      for (int i = 0; i < count; ++i)
        expected[i] = actual[i] = .15 * std::sin(2. * grid_fx::pi * 223. * i / rate);
      for (int offset = 0; offset < count; offset += 512)
        process(*reference, expected.data() + offset, std::min(512, count - offset), c);
      int block = 0;
      for (int offset = 0; offset < count;)
      {
        const int n = std::min(std::array<int, 6>{1, 3, 64, 127, 256, 512}[block++ % 6], count - offset);
        allocations = deletions = 0;
        tracking = true;
        Eigen::internal::set_is_malloc_allowed(false);
        process(*stage, actual.data() + offset, n, c);
        Eigen::internal::set_is_malloc_allowed(true);
        tracking = false;
        assert(allocations == 0 && deletions == 0);
        offset += n;
      }
      double energy = 0.;
      for (int i = 0; i < count; ++i)
      {
        assert(std::isfinite(actual[i]) && std::abs(actual[i]) < 4.);
        assert(std::abs(actual[i] - expected[i]) < 1.e-9);
        energy += actual[i] * actual[i];
      }
      assert(energy > 1.e-5);
      // Automate all family controls between their extremes; no callback allocation.
      std::array<double, 512> x{};
      for (int b = 0; b < 40; ++b)
      {
        const bool high = b % 2 != 0;
        for (int j = 0; j < 5; ++j)
        {
          const auto extremum = [&](int base) {
            const auto& p = kEffectParameters[base + j];
            return high ? p.maximum : p.minimum;
          };
          c.delay[j] = extremum(uDelayTime);
          c.reverb[j] = extremum(uReverbDecay);
          c.modulation[j] = extremum(uModRate);
        }
        c.delayMotion = high ? 100. : 0.;
        c.delayOutput = high ? 6. : -18.;
        c.reverbMotion = high ? 100. : 0.;
        c.reverbHighPass = high ? 500. : 0.;
        c.tube = high ? std::array<double, 4>{24., 100., 100., 18.} : std::array<double, 4>{0., 0., 0., -18.};
        c.cutoff = high ? 12000. : 40.;
        c.resonance = high ? 4. : .5;
        c.envelope = high ? 4. : 0.;
        c.rise = high ? 1200. : 10.;
        c.release = high ? 800. : 20.;
        c.threshold = high ? -12. : -72.;
        c.tape = high ? std::array<double, 4>{12., 100., 100., 6.} : std::array<double, 4>{-12., 0., 0., -18.};
        for (int i = 0; i < 512; ++i)
          x[i] = .5 * std::sin((b * 512 + i) * .07);
        allocations = deletions = 0;
        tracking = true;
        process(*stage, x.data(), 512, c);
        tracking = false;
        assert(allocations == 0 && deletions == 0);
        for (double v : x)
          assert(std::isfinite(v) && std::abs(v) < 16.);
      }
      for (int b = 0; b < 6; ++b)
      {
        x.fill(.123);
        process(*stage, x.data(), 512, c, false);
      }
      for (double v : x)
        assert(v == .123);
      stage->ResetStopped(rate, 1);
      x[0] = .1;
      process(*stage, x.data(), 1, c);
      assert(std::isfinite(x[0]));
    }
    // A fully wet, zero-feedback echo must begin at the requested time in seconds.
    for (auto effect : {SlotEffect::DigitalDelay, SlotEffect::AnalogDelay, SlotEffect::TapeDelay})
    {
      auto stage = prepareEffect(effect, rate);
      UtilityControls c;
      c.delay = {100., 0., 12000., 0., 100.};
      c.delayMotion = 0.;
      const int onset = static_cast<int>(std::round(.1 * rate));
      std::vector<double> x(onset + 200, 0.);
      x[0] = 1.;
      for (size_t offset = 0; offset < x.size(); offset += 512)
        process(*stage, x.data() + offset, static_cast<int>(std::min<size_t>(512, x.size() - offset)), c);
      for (int i = 0; i < onset; ++i)
        assert(std::abs(x[i]) < 1.e-12);
      assert(std::abs(x[onset]) > .001);
    }
  }
  // Boost is exactly clean at neutral voicing, and positive Tilt brightens.
  {
    SlotBoost boost;
    boost.Reset(48000.);
    UtilityControls c;
    c.boost = 0.;
    std::array<double, 512> x{}, original{};
    for (int i = 0; i < 512; ++i)
      x[i] = original[i] = .1 * std::sin(.1 * i);
    allocations = deletions = 0;
    tracking = true;
    boost.Process(x.data(), 512, c, {});
    tracking = false;
    assert(allocations == 0 && deletions == 0);
    for (int i = 0; i < 512; ++i)
      assert(std::abs(x[i] - original[i]) < 1.e-12);
    std::array<double, 2> energy{};
    for (int band = 0; band < 2; ++band)
    {
      boost.Reset(48000.);
      c.tilt = 6.;
      for (int b = 0; b < 40; ++b)
      {
        for (int i = 0; i < 512; ++i)
          x[i] = .1 * std::sin(2. * grid_fx::pi * (band ? 5000. : 100.) * (b * 512 + i) / 48000.);
        boost.Process(x.data(), 512, c, {});
        if (b > 30)
          for (double v : x)
            energy[band] += v * v;
      }
    }
    assert(energy[1] > energy[0] * 3.);
  }
  // Tube bias adds even harmonics; the neutral-bias shaper remains odd. Dry
  // blend and DC removal are checked independently of the waveshaper formula.
  for (double rate : {44100., 48000., 88200., 96000., 192000.})
  {
    std::array<double, 2> harmonic{};
    for (int bias = 0; bias < 2; ++bias)
    {
      auto stage = prepareEffect(SlotEffect::TubeBoost, rate);
      UtilityControls c;
      c.tube = {12., bias ? 100. : 0., 100., 0.};
      std::array<double, 512> x{};
      double sum = 0., secondSin = 0., secondCos = 0.;
      int observed = 0;
      for (int offset = 0; offset < static_cast<int>(rate); offset += 512)
      {
        const int count = std::min(512, static_cast<int>(rate) - offset);
        for (int i = 0; i < count; ++i)
          x[i] = .2 * std::sin(2. * grid_fx::pi * 1000. * (offset + i) / rate);
        process(*stage, x.data(), count, c);
        for (int i = 0; i < count; ++i)
          if (offset + i >= static_cast<int>(rate / 2.))
          {
            const double angle = 4. * grid_fx::pi * 1000. * (offset + i) / rate;
            secondSin += x[i] * std::sin(angle);
            secondCos += x[i] * std::cos(angle);
            sum += x[i];
            ++observed;
          }
      }
      harmonic[bias] = 2. * std::hypot(secondSin, secondCos) / observed;
      assert(std::abs(sum / observed) < 1.e-4);
      c.tube[2] = 0.;
      for (int b = 0; b < 100; ++b)
      {
        x.fill(.123);
        process(*stage, x.data(), 512, c);
      }
      for (double v : x)
        assert(std::abs(v - .123) < 1.e-12);
    }
    assert(harmonic[0] < 1.e-4 && harmonic[1] > .01);
  }
  // Reverbs are genuinely distinct impulse responses, with finite decaying tails.
  std::array<std::vector<double>, 4> responses;
  constexpr std::array reverbs{SlotEffect::RoomReverb, SlotEffect::PlateReverb, SlotEffect::CloudReverb, SlotEffect::HallReverb};
  for (int variant = 0; variant < responses.size(); ++variant)
  {
    auto stage = prepareEffect(reverbs[variant], 48000.);
    UtilityControls c;
    c.reverb = {.5, 100., 5500., 0., 100.};
    auto& x = responses[variant];
    x.assign(48000 * 4, 0.);
    x[0] = 1.;
    for (size_t offset = 0; offset < x.size(); offset += 512)
      process(*stage, x.data() + offset, static_cast<int>(std::min<size_t>(512, x.size() - offset)), c);
    double early = 0., late = 0.;
    for (int i = 4800; i < 24000; ++i)
      early += x[i] * x[i];
    for (int i = 48000 * 3; i < 48000 * 4; ++i)
      late += x[i] * x[i];
    assert(early > 1.e-8 && late < early * .001);
  }
  for (int a = 0; a < responses.size(); ++a)
    for (int b = a + 1; b < responses.size(); ++b)
    {
      double difference = 0.;
      for (size_t i = 0; i < responses[a].size(); ++i)
        difference += std::abs(responses[a][i] - responses[b][i]);
      assert(difference > .01);
    }
  // Bypass advances delay history with silence instead of freezing an old note.
  {
    auto stage = prepareEffect(SlotEffect::DigitalDelay, 48000.);
    UtilityControls c;
    c.delay = {20., 0., 12000., 0., 100.};
    std::array<double, 512> x{};
    x[0] = 1.;
    process(*stage, x.data(), 1, c);
    for (int b = 0; b < 10; ++b)
    {
      x.fill(0.);
      process(*stage, x.data(), 512, c, false);
    }
    double energy = 0.;
    for (int b = 0; b < 10; ++b)
    {
      x.fill(0.);
      process(*stage, x.data(), 512, c);
      for (double v : x)
        energy += v * v;
    }
    assert(energy < 1.e-20);
  }
  // Maximum feedback and an eight-second synced echo must suspend after a
  // bounded bypass drain, with no old note returning when re-enabled. Exercise
  // mono and stereo/ping-pong through the actual slot dispatch without allocation.
  for (auto effect : {SlotEffect::DigitalDelay, SlotEffect::AnalogDelay, SlotEffect::TapeDelay})
    for (bool stereo : {false, true})
    {
      constexpr double rate = 48000.;
      SlotConfig config;
      config.effect = effect;
      auto prepared = PrepareSlot(config, rate, 512);
      auto* history = prepared.dsp.get();
      NAMStage stage;
      stage.Publish(std::move(prepared.dsp));
      stage.ResetStopped(rate, 512);
      assert(history->drainLimit == static_cast<int64_t>(rate * 9.));
      UtilityControls c;
      c.delay = {1500., 82., 150., 100., 100.};
      c.sync = true;
      c.tempo = 30.;
      c.division = 9;
      c.pingPong = stereo;
      c.delayMotion = 100.;
      std::array<double, 512> left{}, right{};
      auto render = [&](bool enabled, double input) {
        left.fill(input);
        right.fill(input * .5);
        allocations = deletions = 0;
        tracking = true;
        stage.Process(left.data(), 512, enabled, 0., 0., {}, {}, c, stereo ? right.data() : nullptr);
        tracking = false;
        assert(allocations == 0 && deletions == 0);
      };
      render(true, .2);
      for (int block = 0; block < 850; ++block)
        render(false, 0.);
      assert(history->drainFrames >= history->drainLimit);
      const auto drained = history->drainFrames;
      render(false, .123);
      assert(history->drainFrames == drained);
      for (double value : left)
        assert(value == .123);
      if (stereo)
        for (double value : right)
          assert(value == .123 * .5);
      // Shortening the delay after re-enable must not uncover stale ring contents.
      c.sync = false;
      c.delay[0] = 20.;
      double energy = 0.;
      for (int block = 0; block < 100; ++block)
      {
        render(true, 0.);
        for (double value : left)
          energy += value * value;
        if (stereo)
          for (double value : right)
            energy += value * value;
      }
      assert(energy < 1.e-18);
    }
  // Swell suppresses the pick and reaches unity on a sustained phrase.
  {
    auto stage = prepareEffect(SlotEffect::AutoSwell, 48000.);
    UtilityControls c;
    c.rise = 100.;
    std::array<double, 512> x{};
    x.fill(.2);
    process(*stage, x.data(), 512, c);
    assert(x[0] < .001 && x[511] < .05);
    for (int b = 0; b < 12; ++b)
    {
      x.fill(.2);
      process(*stage, x.data(), 512, c);
    }
    assert(std::abs(x[511] - .2) < 1.e-10);
  }
  // Detectors settle while bypassed: re-enabling neither resumes a stale gain
  // reduction nor skips a swell, even when the slowest release was selected.
  for (const auto effect : {SlotEffect::SmoothComp, SlotEffect::AutoSwell})
  {
    constexpr double rate = 48000.;
    auto stage = prepareEffect(effect, rate);
    DynamicsControls dynamics;
    dynamics.compThreshold = -40.;
    dynamics.compRelease = 600.;
    UtilityControls c;
    c.release = 800.;
    std::array<double, 512> x{};
    const auto render = [&](bool enabled, double input) {
      x.fill(input);
      allocations = deletions = 0;
      tracking = true;
      stage->Process(x.data(), 512, enabled, 0., 0., dynamics, {}, c);
      tracking = false;
      assert(allocations == 0 && deletions == 0);
    };
    for (int block = 0; block < 200; ++block) render(true, .5);
    if (effect == SlotEffect::SmoothComp) assert(x[511] < .1); // Deep reduction at a sustained level.
    else assert(std::abs(x[511] - .5) < 1.e-6);                  // Fully swelled.
    for (int block = 0; block < 300; ++block) render(false, 0.);
    // A new quiet phrase below the compressor threshold; the swell starts again.
    render(true, .003);
    if (effect == SlotEffect::SmoothComp) assert(x[511] > .0029);
    else assert(x[511] < .0015);
  }
  std::cout << "15 new effects: variable blocks, automation, bypass/reset, no allocation; 44.1/48/88.2/96/192 kHz; "
               "echo timing and distinct reverb tails\n";
}
