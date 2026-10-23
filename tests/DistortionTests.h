#pragma once
#include "SlotOperations.h"

void TestDistortionAndMonoMeters()
{
  for (int style = 0; style < effect_modes::distortionCount; ++style)
  {
    assert(SlotDistortion::Transfer(0., style, 10., .4) == 0.);
    for (double x : {-.1, .1})
      assert(std::abs(SlotDistortion::Transfer(x, style, 10., 0.) - SlotDistortion::Transfer(x * 10., style, 1., 0.)) < 1.e-12);
  }
  assert(SlotDistortion::Transfer(.2, 1, 10., 0.) == 1.);
  assert(SlotDistortion::Transfer(.2, 1, 1., 0.) == .2);
  for (double rate : {44100., 48000., 96000., 192000.})
  {
    constexpr int count = effect_modes::distortionCount * 1024;
    std::array<std::vector<double>, effect_modes::distortionCount> styles;
    for (int style = 0; style < effect_modes::distortionCount; ++style)
    {
      SlotDistortion effect; effect.Reset(rate);
      auto& audio = styles[style]; audio.resize(count);
      UtilityControls c; c.distortion[0] = style;
      for (int i = 0; i < count; ++i) audio[i] = .7 * std::sin(i * .073);
      effect.Process(audio.data(), count, c);
      for (double x : audio) assert(std::isfinite(x) && std::abs(x) < 4.);
      for (int other = 0; other < style; ++other)
      {
        double difference = 0.;
        for (int i = 0; i < count; ++i) difference += std::abs(audio[i] - styles[other][i]);
        assert(difference > 1.); // Each style produces a distinct output.
      }
      effect.Reset(rate); c.distortion[1] = 36.; c.distortion[3] = 100.;
      std::vector<double> constant(static_cast<int>(rate), .5);
      effect.Process(constant.data(), static_cast<int>(constant.size()), c);
      assert(std::abs(constant.back()) < 1.e-10); // Bias must not leave steady DC.
    }
    const auto render = [&](bool tinyBlocks) {
      SlotDistortion effect; effect.Reset(rate);
      std::vector<double> audio(count);
      for (int i = 0; i < count; ++i) audio[i] = .8 * std::sin(i * .071) + .2 * std::cos(i * .031);
      UtilityControls c;
      for (int segment = 0; segment < effect_modes::distortionCount; ++segment)
      {
        c.distortion = {double(segment), segment % 2 ? 36. : 0., segment % 2 ? 200. : 12000.,
                        segment % 2 ? 100. : -100., 100., 6.};
        for (int pos = segment * 1024, block = 0; pos < (segment + 1) * 1024;)
        {
          const int n = std::min((segment + 1) * 1024 - pos,
                                tinyBlocks ? std::array<int, 4>{1, 3, 64, 127}[block++ % 4] : 512);
          allocations = deletions = 0; tracking = true; Eigen::internal::set_is_malloc_allowed(false);
          effect.Process(audio.data() + pos, n, c);
          Eigen::internal::set_is_malloc_allowed(true); tracking = false;
          assert(allocations == 0 && deletions == 0);
          pos += n;
        }
      }
      return audio;
    };
    const auto large = render(false), tiny = render(true);
    for (int i = 0; i < count; ++i)
    {
      assert(std::isfinite(tiny[i]) && std::abs(tiny[i]) < 8.);
      assert(std::abs(tiny[i] - large[i]) < 1.e-12);
    }
    SlotDistortion dry; dry.Reset(rate);
    UtilityControls c; c.distortion[4] = c.distortion[5] = 0.;
    std::array<double, 4> samples{.5, -.75, .125, 0.}, expected = samples;
    dry.Process(samples.data(), 4, c);
    assert(samples == expected);

    // Measure the signal's mono sum, rather than taking the louder channel.
    SlotConfig config; config.effect = SlotEffect::Utility;
    NAMStage meter; meter.Publish(PrepareSlot(config, rate, 64).dsp); meter.ResetStopped(rate, 64);
    std::array<double, 64> l{}, r{};
    for (const auto& levels : std::array<std::pair<double, double>, 3>{{{.5, .5}, {.5, -.5}, {.5, 0.}}})
    {
      l.fill(levels.first); r.fill(levels.second);
      meter.Process(l.data(), 64, true, 0., 0., {}, {}, {}, r.data());
      const auto reading = meter.TakeMeter();
      const double mono = std::abs(.5 * (levels.first + levels.second));
      assert(std::abs(reading.input - mono) < 1.e-6 && std::abs(reading.output - mono) < 1.e-6);
    }
  }
}
