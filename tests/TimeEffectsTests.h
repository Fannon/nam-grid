#pragma once
#include "SlotOperations.h"

// Exercise the actual slot wrapper, including its preparation, bypass and
// replacement path, rather than testing isolated mathematical helper functions.
void TestTimeEffects()
{
  const auto process = [](NAMStage& stage, double* x, int n, const UtilityControls& c, bool on = true) {
    stage.Process(x, n, on, 0., 0., false, 0., 0, 1., {}, {}, c);
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
    for (int id = static_cast<int>(SlotEffect::SynthFilter); id <= static_cast<int>(SlotEffect::Tape); ++id)
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
          assert(std::isfinite(v) && std::abs(v) < 8.);
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
  // Reverbs are genuinely distinct impulse responses, with finite decaying tails.
  std::array<std::vector<double>, 3> responses;
  for (int variant = 0; variant < 3; ++variant)
  {
    auto stage = prepareEffect(static_cast<SlotEffect>(static_cast<int>(SlotEffect::RoomReverb) + variant), 48000.);
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
  for (int a = 0; a < 3; ++a)
    for (int b = a + 1; b < 3; ++b)
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
  std::cout << "14 new effects: variable blocks, automation, bypass/reset, no allocation; 44.1/48/88.2/96/192 kHz; "
               "echo timing and distinct reverb tails\n";
}
