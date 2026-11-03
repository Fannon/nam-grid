#pragma once
#include "SlotOperations.h"
#include "NoisePrintState.h"
#include <iostream>

inline double NoiseSample(uint32_t& state, int sample, double rate)
{
  state = state * 1664525u + 1013904223u;
  return .006 * (double(state) / 4294967296. * 2. - 1.) + .006 * std::sin(6.283185307179586 * 60. * sample / rate);
}

void TestNoisePrintGate()
{
  for (double rate : {44100., 48000., 96000., 192000.})
  {
    noise_print::Processor processor;
    processor.Reset(rate);
    noise_print::Controls c;
    c.gateThreshold = -40.;
    c.mix = 0.; // The final gate works independently of the spectral mix/profile.
    c.gateRelease = 80.;
    std::array<double, 512> left{}, right{};
    auto render = [&](double seconds, double l, double r) {
      for (int done = 0, total = static_cast<int>(seconds * rate); done < total;)
      {
        const int n = std::min(512, total - done);
        left.fill(l);
        right.fill(r);
        allocations = deletions = 0;
        tracking = true;
        processor.Process(left.data(), right.data(), n, c);
        tracking = false;
        assert(!allocations && !deletions);
        for (int i = 0; i < n; ++i)
        {
          assert(std::isfinite(left[i]) && std::isfinite(right[i]));
          assert(std::abs(left[i]) <= std::abs(l) && std::abs(right[i]) <= std::abs(r));
          if (l == -r)
            assert(left[i] == -right[i]);
        }
        done += n;
      }
      // The final sample is valid even when the final block is short.
      const int tail = (static_cast<int>(seconds * rate) - 1) % 512;
      return std::array<double, 2>{left[tail], right[tail]};
    };
    assert((render(.001, .001, -.001) == std::array<double, 2>{0., 0.})); // No startup noise burst.
    assert((render(.2, .001, -.001) == std::array<double, 2>{0., 0.}));
    // Either side opens the linked gate. Anti-phase input must not cancel its detector.
    assert((render(.005, -.1, .1) == std::array<double, 2>{-.1, .1}));
    assert((render(.01, -.001, .001) == std::array<double, 2>{-.001, .001}));
    assert((render(.3, -.001, .001) == std::array<double, 2>{0., 0.}));
    assert((render(.005, 0., .1) == std::array<double, 2>{0., .1}));
    // -41.9 dB lies in the 3 dB hysteresis window: open stays open, closed stays closed.
    assert((render(.1, .008, -.008) == std::array<double, 2>{.008, -.008}));
    assert((render(.3, .001, -.001) == std::array<double, 2>{0., 0.}));
    assert((render(.1, .008, -.008) == std::array<double, 2>{0., 0.}));
    c.learn = true;
    assert((render(.002, .001, -.001) == std::array<double, 2>{.001, -.001}));
    c.learn = false;
    assert((render(.3, .001, -.001) == std::array<double, 2>{0., 0.}));
    c.gateThreshold = -100.;
    assert((render(.002, .001, -.001) == std::array<double, 2>{.001, -.001}));
    c.gateThreshold = -40.;
    c.gateRelease = 300.;
    render(.005, .1, -.1);
    const auto slow = render(.15, .001, -.001);
    assert(slow[0] > 0. && slow[0] < .001);
    c.gateRelease = 20.;
    render(.005, .1, -.1);
    assert((render(.15, .001, -.001) == std::array<double, 2>{0., 0.}));
    processor.Reset(rate);
    c.gateThreshold = -100.;
    assert((render(.002, .123, -.123) == std::array<double, 2>{.123, -.123}));
  }
}

void TestNoisePrintTiming()
{
  // The final gate starts Off, with a tighter independent closing time ready
  // for when it is enabled. Spectral defaults retain their separate meaning.
  const auto defaults = SlotDefaultValues(SlotEffect::NoisePrint);
  assert(defaults[17 + uNoiseGateThreshold] == -100.);
  assert(defaults[17 + uNoiseGateRelease] == 40.);
  assert(defaults[17 + uNoiseRelease] == 120.);
  noise_print::Processor fixed, modulated;
  noise_print::Controls c;
  assert(c.gateThreshold == -100. && c.gateRelease == 40. && c.release == 120.);
  c.gateThreshold = -40.;
  // There is no linked mode: varying spectral Release must never change final
  // gate closure, including at both Gate release endpoints.
  for (double time : {1., 40., 500.})
  {
    fixed.Reset(48000.); modulated.Reset(48000.);
    c.gateRelease = time;
    for (int i = 0; i < 36000; ++i)
    {
      double a = i < 2400 ? .1 : .001, b = a;
      auto varied = c;
      varied.release = 20. + (i % 481);
      fixed.Process(&a, nullptr, 1, c);
      modulated.Process(&b, nullptr, 1, varied);
      assert(a == b);
      if (i == 35999) assert(a == 0.);
    }
  }
}

void TestNoisePrintReopening()
{
  // Exercise the learned spectral path and the final gate together. The older
  // reopening checks use an untrained, dry path and cannot cover this interaction.
  for (double rate : {44100., 48000., 96000.})
    for (double mix : {50., 100.})
      for (double sensitivity : {1.5, 4.5})
        for (int articulation : {0, 1, 2})
  {
    noise_print::Processor gated, ungated;
    gated.Reset(rate); ungated.Reset(rate);
    noise_print::Controls c;
    c.mix = mix; c.sensitivity = sensitivity;
    c.gateThreshold = -48.; c.gateRelease = 40.;
    uint32_t seed = 89;
    int firstOpen = -1;
    double error = 0., reference = 0.;
    const int onset = static_cast<int>(1.5 * rate);
    for (int i = 0; i < static_cast<int>(2.5 * rate); ++i)
    {
      const double t = i / rate, age = std::max(0., (i - onset) / rate);
      double note = 0.;
      if (i >= onset && age < .25)
      {
        const double frequency = articulation == 0 ? 82.4 : articulation == 1 ? 146.83 : 110.;
        const double attack = articulation == 1 ? .008 : .001;
        const double decay = articulation == 2 ? .035 : .25;
        for (int harmonic = 1; harmonic <= 6; ++harmonic)
          note += .025 * std::min(age / attack, 1.) * std::exp(-age / decay)
                  * std::sin(6.283185307179586 * frequency * harmonic * age) / std::pow(double(harmonic), 1.5);
      }
      seed = seed * 1664525u + 1013904223u;
      const double noise = .0015 * (double(seed) / 4294967296. * 2. - 1.)
                           + .0015 * std::sin(6.283185307179586 * 150. * t);
      double a = note + noise, b = a;
      c.learn = t < 1.;
      auto dryGate = c; dryGate.gateThreshold = -100.;
      gated.Process(&a, nullptr, 1, c);
      ungated.Process(&b, nullptr, 1, dryGate);
      if (t >= 1.35 && i < onset) assert(a == 0.);
      if (i >= onset && firstOpen < 0 && a != 0.) firstOpen = i - onset;
      if (age >= .020 && age < .050)
      {
        error += (a - b) * (a - b);
        reference += b * b;
      }
      if (t > 2.3) assert(a == 0.);
    }
    assert(gated.GetProfile().valid);
    assert(firstOpen >= 0 && firstOpen < .015 * rate);
    assert(reference > 0. && error < 1.e-12 * reference);
  }
}

void TestNoisePrint()
{
  TestNoisePrintGate();
  TestNoisePrintTiming();
  TestNoisePrintReopening();
  // Short picked notes need to open before their energy has already decayed.
  // Require <4% error energy in the first 20 ms of each high-SNR chug. The
  // previous fixed opening coefficient gives about 8.5% on this signal.
  {
    noise_print::Processor processor;
    processor.Reset(48000.);
    noise_print::Controls c;
    uint32_t seed = 42;
    double error = 0., wanted = 0.;
    for (int sample = 0; sample < 7 * 48000; ++sample)
    {
      const double t = sample / 48000.;
      const double age = std::fmod(t, .5);
      double clean = 0.;
      if (t >= 2. && t < 6.)
        for (int h = 1; h <= 8; ++h)
          clean += .1 * std::min(age / .001, 1.) * std::exp(-age / .035)
                   * std::sin(6.283185307179586 * 110. * h * age) / std::pow(double(h), 1.5);
      seed = seed * 1664525u + 1013904223u;
      const double noise = .002 * (double(seed) / 4294967296. * 2. - 1.)
                           + .002 * std::sin(6.283185307179586 * 60. * t);
      double output = clean + noise;
      c.learn = t < 1.;
      processor.Process(&output, nullptr, 1, c);
      if (t >= 2. && t < 6. && age < .020)
      {
        error += (output - clean) * (output - clean);
        wanted += clean * clean;
      }
    }
    assert(error < .04 * wanted);
  }
  // The final gate must key from the denoised signal, not the louder raw noise.
  {
    noise_print::Processor processor;
    processor.Reset(48000.);
    noise_print::Controls c;
    c.learn = true;
    c.gateThreshold = -40.;
    c.release = 20.;
    uint32_t seed = 42;
    std::array<double, 128> samples{};
    for (int block = 0; block < 1500; ++block)
    {
      c.learn = block < 600;
      for (int i = 0; i < 128; ++i)
        samples[i] = 3. * NoiseSample(seed, block * 128 + i, 48000.);
      processor.Process(samples.data(), nullptr, 128, c);
    }
    assert(processor.GetProfile().valid);
    assert(processor.CleanPeak() > 0. && processor.CleanPeak() < .01);
    for (double sample : samples) assert(sample == 0.);
    // Gate release can be tightened without shortening spectral Release.
    processor.Reset(48000.);
    c.release = 500.;
    c.gateRelease = 20.;
    samples.fill(.1);
    processor.Process(samples.data(), nullptr, 128, c);
    for (int block = 0; block < 60; ++block)
    {
      samples.fill(.001);
      processor.Process(samples.data(), nullptr, 128, c);
    }
    for (double sample : samples) assert(sample == 0.);
  }
  std::cout << std::unitbuf;
  noise_print::Profile learned;
  for (double rate : {44100., 48000., 96000., 192000.})
  {
    noise_print::Processor processor;
    processor.Reset(rate);
    noise_print::Controls c;
    std::array<double, 512> left{}, right{};
    uint32_t seed = 15;
    int position = 0;
    double fundamental = 440.;
    auto render = [&](int samples, bool learn, bool signal, double& inputError, double& outputError) {
      c.learn = learn;
      for (int done = 0; done < samples;)
      {
        const int n = std::min(512, samples - done);
        std::array<double, 512> clean{}, original{};
        for (int i = 0; i < n; ++i)
        {
          const double phase = 6.283185307179586 * fundamental * (position + i) / rate;
          clean[i] = signal ? .2 * std::sin(phase) + .05 * std::sin(2. * phase) : 0.;
          original[i] = left[i] = clean[i] + NoiseSample(seed, position + i, rate);
          right[i] = left[i];
        }
        allocations = deletions = 0;
        tracking = true;
        processor.Process(left.data(), right.data(), n, c);
        tracking = false;
        assert(allocations == 0 && deletions == 0);
        for (int i = 0; i < n; ++i)
        {
          assert(std::isfinite(left[i]) && left[i] == right[i]);
          if (learn || !processor.GetProfile().valid)
            assert(left[i] == original[i]);
          inputError += (original[i] - clean[i]) * (original[i] - clean[i]);
          outputError += (left[i] - clean[i]) * (left[i] - clean[i]);
        }
        position += n;
        done += n;
      }
    };
    double a = 0., b = 0.;
    render(static_cast<int>(rate * 1.5), true, false, a, b);
    render(static_cast<int>(rate), false, false, a, b);
    assert(processor.GetProfile().valid);
    learned = processor.GetProfile();
    a = b = 0.;
    render(static_cast<int>(rate), false, false, a, b);
    const double reduction = 10. * std::log10(a / b);
    std::cout << "Noise Print " << rate << " Hz: noise-only reduction " << reduction << " dB\n";
    assert(reduction > 12.);
    for (double frequency : {82.4, 220., 440., 1760.})
    {
      fundamental = frequency;
      render(static_cast<int>(rate * .4), false, true, a, b);
      a = b = 0.;
      render(static_cast<int>(rate), false, true, a, b);
      const double improvement = 10. * std::log10(a / b);
      std::cout << "  " << frequency << " Hz harmonic tone + noise error improvement " << improvement << " dB\n";
      assert(improvement > 0.); // Includes wanted-signal amplitude and phase damage.
    }
    const auto saved = processor.GetProfile();
    processor.Reset(rate * .5); // Reset/re-rate preserves a completed snapshot.
    assert(processor.GetProfile() == saved);
    c.learn = true;
    for (int i = 0; i < 100; ++i)
    {
      left.fill(0.);
      processor.Process(left.data(), nullptr, 512, c);
    }
    c.learn = false;
    processor.Process(left.data(), nullptr, 1, c);
    assert(processor.GetProfile() == saved); // Silence never replaces a useful print.
    assert(noise_print::Load(noise_print::Save(saved)) == saved);
  }
  // Exact block partitioning, stereo linking, dry path and short-learn rejection.
  for (double gateRelease : {1., 45., 500.})
  {
    noise_print::Processor whole, split;
    whole.Reset(48000.); split.Reset(48000.);
    whole.Restore(learned); split.Restore(learned);
    noise_print::Controls controls;
    controls.gateRelease = gateRelease;
    controls.gateThreshold = -60.;
    uint32_t seed = 12;
    std::array<double, 128> l{}, r{}, sl{}, sr{};
    for (int block = 0; block < 120; ++block)
    {
      for (int i = 0; i < 128; ++i)
      {
        l[i] = sl[i] = NoiseSample(seed, block * 128 + i, 48000.);
        r[i] = sr[i] = -l[i];
      }
      allocations = deletions = 0; tracking = true;
      whole.Process(l.data(), r.data(), 128, controls);
      for (int i = 0; i < 128; ++i) split.Process(sl.data()+i, sr.data()+i, 1, controls);
      tracking = false;
      assert(!allocations && !deletions);
      for (int i = 0; i < 128; ++i)
        assert(std::isfinite(l[i]) && l[i] == sl[i] && r[i] == sr[i] && l[i] == -r[i]);
    }
    controls.reduction = 0.; controls.gateThreshold = -100.;
    for (int block = 0; block < 10; ++block)
    {
      l.fill(.123); r.fill(-.123);
      whole.Process(l.data(), r.data(), 128, controls);
    }
    for (int i = 0; i < 128; ++i) assert(l[i] == .123 && r[i] == -.123);
  }
  noise_print::Processor single, blocked;
  single.Reset(48000.);
  blocked.Reset(48000.);
  single.Restore(learned);
  blocked.Restore(learned);
  noise_print::Controls c;
  c.gateThreshold = -35.;
  c.release = 20.;
  std::array<double, 512> a{}, b{};
  for (int block = 0; block < 80; ++block)
  {
    for (int i = 0; i < 512; ++i)
      a[i] = b[i] = (block < 40 ? .001 : .2) * std::sin(.07 * (block * 512 + i));
    blocked.Process(b.data(), nullptr, 512, c);
    for (int i = 0; i < 512; ++i)
      single.Process(&a[i], nullptr, 1, c);
    for (int i = 0; i < 512; ++i)
      assert(a[i] == b[i]);
  }
  const auto prior = blocked.GetProfile();
  c.gateThreshold = -100.;
  c.learn = true;
  for (int block = 0; block < 8; ++block) // Less than the minimum capture duration.
  {
    a.fill(.01);
    blocked.Process(a.data(), nullptr, 512, c);
  }
  c.learn = false;
  blocked.Process(a.data(), nullptr, 1, c);
  assert(blocked.GetProfile() == prior);
  // Mix/Reduction automation settles at the exact original signal, not an
  // allpass reconstruction. Invalid input cannot poison the filter histories.
  c.mix = 0.;
  for (int block = 0; block < 4; ++block)
  {
    a.fill(.123);
    blocked.Process(a.data(), nullptr, 512, c);
  }
  for (double x : a)
    assert(x == .123);
  assert(blocked.Reduction() == 0.); // Dry Mix must not report spectral gain reduction.
  c.mix = 100.;
  c.reduction = 0.;
  for (int block = 0; block < 4; ++block)
  {
    a.fill(.123);
    blocked.Process(a.data(), nullptr, 512, c);
  }
  for (double x : a)
    assert(x == .123);
  a[0] = std::numeric_limits<double>::quiet_NaN();
  blocked.Process(a.data(), nullptr, 512, c);
  for (double x : a)
    assert(std::isfinite(x));
  SlotConfig config;
  config.effect = SlotEffect::NoisePrint;
  config.noiseProfile = learned;
  auto prepared = PrepareSlot(config, 48000., 512);
  prepared.dsp->noiseEpoch = 17;
  NAMStage stage;
  stage.Publish(std::move(prepared.dsp));
  stage.ResetStopped(48000., 512);
  assert(stage.Latency() == 0);
  a.fill(.001);
  allocations = deletions = 0;
  tracking = true;
  stage.Process(a.data(), 512, true, 0., 0.);
  tracking = false;
  assert(allocations == 0 && deletions == 0);
  assert(stage.GetNoiseProfile(17, {}) == learned);
  assert(!stage.GetNoiseProfile(18, {}).valid);
  for (int block = 0; block < 4; ++block)
  {
    a.fill(.123);
    stage.Process(a.data(), 512, false, 0., 0.);
  }
  for (double x : a)
    assert(x == .123);
  for (auto mode : {SlotEditMode::Copy, SlotEditMode::Move, SlotEditMode::Swap})
  {
    auto edit = PrepareSlotEdit(config, {}, mode, 48000., 512);
    assert(edit.destination.config.noiseProfile == learned);
  }
  // Race-free bounded mailbox reads cannot mix powers from different snapshots.
  noise_print::Snapshot mailbox;
  std::atomic<bool> done{false};
  std::thread writer([&] {
    for (int i = 1; i < 3000; ++i)
    {
      noise_print::Profile p;
      p.valid = true;
      p.power.fill(i);
      mailbox.Publish(p, 7);
    }
    done.store(true);
  });
  while (!done.load())
  {
    const auto profile = mailbox.Read(7, {});
    if (profile.valid)
      for (double power : profile.power)
        assert(power == profile.power[0]);
  }
  writer.join();
}
