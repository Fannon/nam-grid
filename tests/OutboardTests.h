#pragma once
#include <random>
#include "SlotOperations.h"

void TestOutboard()
{
  const std::array effects{
    SlotEffect::OutboardChannel, SlotEffect::ConsoleColour, SlotEffect::ConsoleEQ, SlotEffect::ButterComp};
  for (double rate : {44100., 48000., 88200., 96000., 192000.})
    for (const auto effect : effects)
    {
      const auto render = [&](bool shortBlocks, bool automate) {
        SlotConfig config;
        config.effect = effect;
        NAMStage stage;
        stage.Publish(PrepareSlot(config, rate, 512).dsp);
        stage.ResetStopped(rate, 512);
        assert(stage.Latency() == 0);
        UtilityControls c;
        const int count = 16384;
        std::array<std::vector<double>, 2> audio{std::vector<double>(count), std::vector<double>(count)};
        for (int i = 0; i < count; ++i)
        {
          audio[0][i] = .4 * std::sin(i * .071);
          audio[1][i] = .2 * std::cos(i * .053);
        }
        for (int pos = 0, block = 0; pos < count;)
        {
          const int segment = pos / 2048;
          if (automate)
          {
            const auto extreme = [&](int p) {
              return segment % 2 ? kEffectParameters[p].maximum : kEffectParameters[p].minimum;
            };
            for (int j = 0; j < 7; ++j)
            {
              c.strip[j] = extreme(uStripLowCut + j);
              c.consoleEQ[j] = extreme(uConsoleTreble + j);
            }
            for (int j = 0; j < 3; ++j)
            {
              c.colour[j] = extreme(uColourInput + j);
              c.butter[j] = extreme(uButterCompress + j);
            }
            c.colourDesk = segment;
          }
          const int boundary = std::min(count, (segment + 1) * 2048);
          const int n =
            std::min(boundary - pos, shortBlocks ? std::array<int, 5>{1, 3, 64, 127, 256}[block++ % 5] : 512);
          allocations = deletions = 0;
          tracking = true;
          Eigen::internal::set_is_malloc_allowed(false);
          stage.Process(audio[0].data() + pos, n, true, 0., 0., {}, {}, c, audio[1].data() + pos);
          Eigen::internal::set_is_malloc_allowed(true);
          tracking = false;
          assert(allocations == 0 && deletions == 0);
          pos += n;
        }
        return audio;
      };
      for (bool automate : {false, true})
      {
        const auto normal = render(false, automate), varied = render(true, automate);
        for (int ch = 0; ch < 2; ++ch)
          for (size_t i = 0; i < varied[ch].size(); ++i)
          {
            assert(std::isfinite(varied[ch][i]) && std::abs(varied[ch][i]) < 64.);
            assert(std::abs(normal[ch][i] - varied[ch][i]) < 1.e-12);
          }
      }
      // Exact dry semantics include true digital black and above-unity input.
      SlotOutboard outboard;
      outboard.Reset(rate, effect);
      UtilityControls c;
      c.colour = {18., 18., 0.};
      c.strip[5] = c.butter[1] = 0.;
      c.strip[6] = c.butter[2] = 0.;
      std::array<double, 5> left{0., .1, -.5, 2., -1.5}, right{-.1, 0., .4, -2., 1.5};
      const auto originalL = left, originalR = right;
      outboard.Process(left.data(), right.data(), 5, c);
      assert(left == originalL && right == originalR);

      // Dual-mono cores must not feed left input into a silent right channel.
      outboard.Reset(rate, effect);
      c = {};
      std::array<double, 512> l{}, r{};
      l.fill(.2);
      outboard.Process(l.data(), r.data(), 512, c);
      for (double x : r)
        assert(std::abs(x) < 1.e-5); // native denormal noise only
    }

  // Direct CStrip2 filter paths are identical at its 44.1 kHz reference rate;
  // high cut open / low cut off / flat EQ bypasses bit-exactly in the wrapper.
  SlotOutboard neutral;
  neutral.Reset(48000., SlotEffect::OutboardChannel);
  UtilityControls c;
  c.strip = {0., 100., 0., 0., 0., 0., 100.};
  std::array<double, 4> l{.2, -.2, .6, -.6};
  const auto dry = l;
  neutral.Process(l.data(), nullptr, 4, c);
  assert(l == dry);

  // PR 5: Mono-lane parity for all 4 SlotOutboard effects + BussColors4 ring buffer across 8 consoles.
  for (const auto effect : effects)
    for (double rate : {44100., 48000., 192000.})
    {
      const int desks = effect == SlotEffect::ConsoleColour ? 8 : 1;
      for (int desk = 0; desk < desks; ++desk)
      {
        SlotOutboard monoFx, stereoFx;
        monoFx.Reset(rate, effect);
        stereoFx.Reset(rate, effect);
        UtilityControls uc;
        uc.colourDesk = desk;
        uc.strip = {60., 8000., 35., 45., 25., 0., 100.};
        uc.consoleEQ = {4., -2., 3., 600., 100., 10000., 40.};
        uc.butter = {65., 0., 100.};
        uc.colour = {6., 0., 100.};
        std::array<double, 512> mL{}, sL{}, sR{};
        for (int i = 0; i < 512; ++i)
          mL[i] = sL[i] = sR[i] = .25 * std::sin(i * .071) + .05 * std::cos(i * .19);
        allocations = deletions = 0; tracking = true;
        Eigen::internal::set_is_malloc_allowed(false);
        monoFx.Process(mL.data(), nullptr, 512, uc);
        Eigen::internal::set_is_malloc_allowed(true);
        tracking = false;
        assert(allocations == 0 && deletions == 0);
        stereoFx.Process(sL.data(), sR.data(), 512, uc);
        for (int i = 0; i < 512; ++i)
          assert(std::isfinite(mL[i]) && mL[i] == sL[i]);
      }
    }
  // BussColors4 direct TickMono parity across all 8 consoles & 44.1..192 kHz.
  for (double rate : {44100., 48000., 192000.})
  {
    std::array<double, 8> consoleSums{};
    for (int desk = 0; desk < 8; ++desk)
    {
      airwindows_outboard::BussColors4::Core monoCore, stereoCore;
      monoCore.Reset(rate);
      stereoCore.Reset(rate);
      const std::array<double, 4> ctrl{desk / 7., .6, .5, 1.};
      monoCore.Set(ctrl);
      stereoCore.Set(ctrl);
      for (int i = 0; i < 512; ++i)
      {
        double m = .2 * std::sin(i * .083);
        double sL = m, sR = -.15 * std::cos(i * .041);
        monoCore.TickMono(m);
        stereoCore.Tick(sL, sR);
        assert(std::isfinite(m) && m == sL);
        consoleSums[desk] += m * m;
      }
      for (int prev = 0; prev < desk; ++prev)
        assert(std::abs(consoleSums[desk] - consoleSums[prev]) > 1.e-5);
    }
  }
  // BussColors4 slowdyn sag integrator must keep the same real-time decay (~22.7 ms)
  // across 44.1..192 kHz rather than decaying 4x too fast at 192 kHz.
  {
    auto sagDecayRatio = [](double r) {
      airwindows_outboard::BussColors4::Core primed, quiet;
      primed.Reset(r); quiet.Reset(r);
      const std::array<double, 4> ctrl{2. / 7., .5, .5, 1.}; // Lush (Neve)
      primed.Set(ctrl); quiet.Set(ctrl);
      const int burst = static_cast<int>(r * .005);
      const int w1a = burst + static_cast<int>(r * .003), w1b = burst + static_cast<int>(r * .005);
      const int w2a = burst + static_cast<int>(r * .023), w2b = burst + static_cast<int>(r * .025);
      double e1 = 0., e2 = 0.;
      for (int i = 0; i < w2b; ++i)
      {
        const double probe = .0003 * std::sin(2. * grid_fx::pi * 1000. * i / r);
        double pL = i < burst ? .6 * std::sin(2. * grid_fx::pi * 1000. * i / r) : probe, pR = pL;
        double qL = probe, qR = probe;
        primed.Tick(pL, pR);
        quiet.Tick(qL, qR);
        const double d = pL - qL;
        if (i >= w1a && i < w1b) e1 += d * d;
        if (i >= w2a && i < w2b) e2 += d * d;
      }
      return std::sqrt((e2 / (w2b - w2a)) / std::max(1.e-30, e1 / (w1b - w1a)));
    };
    const double ref = sagDecayRatio(44100.);
    assert(ref > .3 && ref < .65);
    for (double r : {48000., 96000., 192000.})
      assert(std::abs(sagDecayRatio(r) - ref) < .06);
  }
  std::cout << "Outboard: five rates, stereo, extremes, variable blocks, exact dry, mono-lane, BussColors4 ring, no allocation\n";
}

void TestAirDynamics()
{
  std::array<std::array<double, 5>, 2> closure{};
  int rateIndex = 0;
  for (double rate : {44100., 48000., 88200., 96000., 192000.})
  {
    for (auto effect : {SlotEffect::DigitalBlack})
      for (int mode = 0; mode < 2; ++mode)
      {
        const auto render = [&](bool varied, bool automate) {
          SlotConfig config;
          config.effect = effect;
          NAMStage stage;
          stage.Publish(PrepareSlot(config, rate, 512).dsp);
          stage.ResetStopped(rate, 512);
          assert(stage.Latency() == 0);
          UtilityControls c;
          c.blackMode = mode;
          std::array<std::vector<double>, 2> audio{std::vector<double>(16384), std::vector<double>(16384)};
          for (int i = 0; i < 16384; ++i)
          {
            audio[0][i] = .9 * std::sin(i * .071);
            audio[1][i] = .2 * std::cos(i * .053);
          }
          for (int pos = 0, block = 0; pos < 16384;)
          {
            const int segment = pos / 2048;
            if (automate)
            {
              for (int j = 0; j < 3; ++j)
              {
                const auto& b = kEffectParameters[uBlackThreshold + j];
                c.black[j] = segment % 2 ? b.maximum : b.minimum;
              }
              c.blackMode = segment % 2;
            }
            const int n =
              std::min((segment + 1) * 2048 - pos, varied ? std::array<int, 5>{1, 3, 64, 127, 256}[block++ % 5] : 512);
            allocations = deletions = 0;
            tracking = true;
            Eigen::internal::set_is_malloc_allowed(false);
            stage.Process(audio[0].data() + pos, n, true, 0., 0., {}, {}, c, audio[1].data() + pos);
            Eigen::internal::set_is_malloc_allowed(true);
            tracking = false;
            assert(allocations == 0 && deletions == 0);
            pos += n;
          }
          return audio;
        };
        for (bool automate : {false, true})
        {
          const auto a = render(false, automate), b = render(true, automate);
          for (int ch = 0; ch < 2; ++ch)
            for (size_t i = 0; i < a[ch].size(); ++i)
            {
              assert(std::isfinite(a[ch][i]) && std::abs(a[ch][i]) < 16.);
              assert(std::abs(a[ch][i] - b[ch][i]) < 1.e-12);
            }
        }
        SlotAirDynamics dry;
        dry.Reset(rate, effect);
        UtilityControls c;
        c.blackMode = mode;
        c.black = {-55., 0., 0.};
        std::array<double, 4> l{0., .1, -.2, 1.2}, r{-.1, 0., .3, -1.1};
        const auto beforeL = l, beforeR = r;
        dry.Process(l.data(), r.data(), 4, c);
        assert(l == beforeL && r == beforeR);
      }
    // A low-level tail must become true digital silence in both gate modes.
    // Normalized counters keep closing times stable at the same physical pitch.
    for (int mode = 0; mode < 2; ++mode)
    {
      SlotAirDynamics gate;
      gate.Reset(rate, SlotEffect::DigitalBlack);
      UtilityControls c;
      c.blackMode = mode;
      int last = -1;
      const int count = static_cast<int>(rate * 1.2);
      for (int pos = 0; pos < count;)
      {
        std::array<double, 128> l{}, r{};
        const int n = std::min(128, count - pos);
        for (int i = 0; i < n; ++i)
        {
          const double t = (pos + i) / rate;
          l[i] = r[i] = (t < .1 ? .2 : .0001) * std::sin(2. * grid_fx::pi * 220. * t);
        }
        gate.Process(l.data(), r.data(), n, c);
        for (int i = 0; i < n; ++i)
          if (std::abs(l[i]) > 1.e-12)
            last = pos + i;
        pos += n;
      }
      closure[mode][rateIndex] = last / rate - .1;
      assert(closure[mode][rateIndex] > .015 && closure[mode][rateIndex] < .8);
    }
    ++rateIndex;
  }
  for (const auto& times : closure)
    assert(*std::max_element(times.begin(), times.end()) - *std::min_element(times.begin(), times.end()) < .015);
  // Hold (a NAM Grid addition) stops chatter on noise near Threshold and delays the
  // closing by about its own length; 0 ms keeps the native gates.
  for (int mode = 0; mode < 2; ++mode)
  {
    const auto run = [&](double hold, bool noise) {
      constexpr double rate = 48000.;
      SlotAirDynamics gate;
      gate.Reset(rate, SlotEffect::DigitalBlack);
      UtilityControls c;
      c.blackMode = mode;
      c.blackHold = hold;
      std::mt19937 random(11);
      std::normal_distribution<double> gaussian;
      int switches = 0, last = -1;
      bool open = false;
      const int count = static_cast<int>(rate * 1.2);
      for (int pos = 0; pos < count;)
      {
        std::array<double, 128> l{}, r{};
        const int n = std::min(128, count - pos);
        for (int i = 0; i < n; ++i)
        {
          const double t = (pos + i) / rate;
          // Noise whose peaks just reach the -55 dB threshold, or a note ending at 0.1 s.
          l[i] = r[i] = noise ? .0007 * gaussian(random) : (t < .1 ? .2 : .0001) * std::sin(2. * grid_fx::pi * 220. * t);
        }
        gate.Process(l.data(), r.data(), n, c);
        for (int i = 0; i < n; ++i)
        {
          const bool sounding = std::abs(l[i]) > 1.e-12;
          if (sounding != open) { open = sounding; ++switches; }
          if (sounding) last = pos + i;
        }
        pos += n;
      }
      return noise ? static_cast<double>(switches) : last / rate - .1;
    };
    const double chatter = run(0., true), held = run(50., true);
    // Zero crossing reopens on every noise peak (about 150 switches here); Hysteresis already re-arms.
    assert(held <= chatter && (mode == 1 || (chatter > 20. && held < chatter * .25)));
    const double native = run(0., false), delayed = run(100., false);
    assert(delayed - native > .08 && delayed - native < .13);
  }
  // PR 5: Mono-lane parity for SlotAirDynamics (both modes) under zero audio allocations.
  for (int mode = 0; mode < 2; ++mode)
  {
    SlotAirDynamics monoFx, stereoFx;
    monoFx.Reset(48000., SlotEffect::DigitalBlack);
    stereoFx.Reset(48000., SlotEffect::DigitalBlack);
    UtilityControls uc;
    uc.blackMode = mode;
    uc.black = {-40., 100., 0.};
    std::array<double, 512> mL{}, sL{}, sR{};
    for (int i = 0; i < 512; ++i)
      mL[i] = sL[i] = sR[i] = (i < 256 ? .25 : .0002) * std::sin(i * .11);
    allocations = deletions = 0; tracking = true;
    Eigen::internal::set_is_malloc_allowed(false);
    monoFx.Process(mL.data(), nullptr, 512, uc);
    Eigen::internal::set_is_malloc_allowed(true);
    tracking = false;
    assert(allocations == 0 && deletions == 0);
    stereoFx.Process(sL.data(), sR.data(), 512, uc);
    for (int i = 0; i < 512; ++i) assert(mL[i] == sL[i]);
  }
  std::cout << "Air dynamics: five rates, both gate modes, closing-time compensation, exact dry, mono-lane, no "
               "allocation and hold chatter/release tests passed\n";
}
