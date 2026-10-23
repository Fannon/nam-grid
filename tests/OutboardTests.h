#pragma once
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
  std::cout << "Outboard: five rates, stereo, extremes, variable blocks, exact dry, no allocation\n";
}

void TestAirDynamics()
{
  std::array<std::array<double, 5>, 2> closure{};
  std::array<double, 5> recovery{};
  int rateIndex = 0;
  for (double rate : {44100., 48000., 88200., 96000., 192000.})
  {
    for (auto effect : {SlotEffect::DigitalBlack, SlotEffect::Pressure})
      for (int mode = 0; mode < (effect == SlotEffect::DigitalBlack ? 2 : 1); ++mode)
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
                const auto& p = kEffectParameters[uPressureCompress + j];
                c.black[j] = segment % 2 ? b.maximum : b.minimum;
                c.pressure[j] = segment % 2 ? p.maximum : p.minimum;
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
        c.pressure = {80., 0., 0.};
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
    SlotAirDynamics pressure;
    pressure.Reset(rate, SlotEffect::Pressure);
    UtilityControls pc;
    double energy = 0.;
    int measured = 0;
    for (int pos = 0; pos < static_cast<int>(rate);)
    {
      std::array<double, 128> l{}, r{};
      const int n = std::min(128, static_cast<int>(rate) - pos);
      for (int i = 0; i < n; ++i)
      {
        const double t = (pos + i) / rate;
        l[i] = .05 * std::sin(2. * grid_fx::pi * 220. * t);
        r[i] = t < .15 ? .3 * std::sin(2. * grid_fx::pi * 330. * t) : 0.;
      }
      pressure.Process(l.data(), r.data(), n, pc);
      for (int i = 0; i < n; ++i)
        if ((pos + i) / rate >= .5 && (pos + i) / rate < .8)
        {
          energy += l[i] * l[i];
          ++measured;
        }
      pos += n;
    }
    recovery[rateIndex] = std::sqrt(energy / measured);
    ++rateIndex;
  }
  for (const auto& times : closure)
    assert(*std::max_element(times.begin(), times.end()) - *std::min_element(times.begin(), times.end()) < .015);
  assert(*std::max_element(recovery.begin(), recovery.end()) / *std::min_element(recovery.begin(), recovery.end())
         < 1.03);

  // A loud right channel must reduce a quiet left probe through the linked
  // detector. Running Pressure as two dual-mono instances would fail this.
  const auto probe = [](bool hotRight) {
    SlotAirDynamics pressure;
    pressure.Reset(48000., SlotEffect::Pressure);
    UtilityControls c;
    double energy = 0.;
    for (int block = 0; block < 400; ++block)
    {
      std::array<double, 128> l{}, r{};
      for (int i = 0; i < 128; ++i)
      {
        l[i] = .05 * std::sin((block * 128 + i) * .04);
        r[i] = hotRight ? .35 * std::sin((block * 128 + i) * .057) : 0.;
      }
      pressure.Process(l.data(), r.data(), 128, c);
      if (block > 200)
        for (double x : l)
          energy += x * x;
    }
    return energy;
  };
  assert(probe(true) < probe(false) * .8);
  std::cout << "Air dynamics: five rates, both gate modes, closing-time compensation, linked detector, exact dry, no "
               "allocation\n";
}
