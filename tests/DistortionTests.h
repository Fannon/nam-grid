#pragma once
#include "SlotOperations.h"
#include "SpectralAnalysis.h"
#include <cstring>

void TestDistortionAndMonoMeters()
{
  for (int style = 0; style < effect_modes::distortionCount; ++style)
  {
    assert(SlotDistortion::Transfer(0., style, 10., .4) == 0.);
    for (double x : {-.1, .1})
      assert(std::abs(SlotDistortion::Transfer(x, style, 10., 0.) - SlotDistortion::Transfer(x * 10., style, 1., 0.)) < 1.e-12);
  }
  // Numerical interval averaging is independent of the analytic primitives.
  // Exercise intervals crossing corners and multiple foldback periods, with bias.
  for (int style = 0; style < effect_modes::distortionCount; ++style)
    for (const auto interval : {std::pair{-1.2, 1.7}, std::pair{.12, .13}, std::pair{-.4, .8}})
    {
      constexpr int steps = 65536;
      double integral = 0.;
      for (int i = 0; i < steps; ++i)
        integral += SlotDistortion::Transfer(interval.first + (interval.second - interval.first) * (i + .5) / steps,
                                             style, 10., .4);
      const double actual = SlotDistortion::Antialiased(interval.second, interval.first, style, 10., .4);
      assert(std::abs(actual - integral / steps) < 2.e-7);
      for (double x : {0., 1.e-12, -.2, 1.e5})
      {
        const double settled = SlotDistortion::Antialiased(x, x, style, 63., .4);
        assert(std::isfinite(settled));
        assert(std::abs(settled - SlotDistortion::Transfer(x, style, 63., .4)) < 1.e-12);
      }
    }
  // Compare the complete processors against the uncached analytic path while
  // curve coefficients and active styles move, including tiny/odd callbacks.
  // This protects the cache invalidation rules independently of spectral ratios.
  for (double rate : {44100., 48000., 88200., 96000., 176400., 192000.})
  {
    SlotDistortion cached, reference; cached.Reset(rate); reference.Reset(rate);
    std::vector<double> actual(8192), expected(actual.size());
    for (int i = 0; i < actual.size(); ++i)
      actual[i] = expected[i] = .7 * std::sin(i * .071) + .1 * std::cos(i * .037);
    UtilityControls c;
    for (int position = 0, block = 0; position < actual.size();)
    {
      const int segment = position / 256;
      c.distortion = {double(segment % effect_modes::distortionCount), double((segment % 4) * 12),
                      segment % 2 ? 200. : 12000., double((segment % 3 - 1) * 100),
                      double((segment % 3) * 50), segment % 2 ? -6. : 6.};
      const int n = std::min({static_cast<int>(actual.size()) - position, 256 - position % 256,
                             std::array<int, 4>{1, 3, 37, 128}[block++ % 4]});
      allocations = deletions = 0; tracking = true; Eigen::internal::set_is_malloc_allowed(false);
      cached.Process(actual.data() + position, n, c);
      reference.Process<false>(expected.data() + position, n, c);
      Eigen::internal::set_is_malloc_allowed(true); tracking = false;
      assert(allocations == 0 && deletions == 0);
      position += n;
    }
    assert(std::memcmp(actual.data(), expected.data(), actual.size() * sizeof(double)) == 0);
  }
  assert(SlotDistortion::Transfer(.2, 1, 10., 0.) == 1.);
  assert(SlotDistortion::Transfer(.2, 1, 1., 0.) == .2);
  for (double rate : {44100., 48000., 88200., 96000., 176400., 192000.})
  {
    assert(SlotDistortion::FactorForRate(rate) == (rate < 88200. ? 4 : rate < 176400. ? 2 : 1));
    SlotDistortion selected; selected.Reset(rate);
    assert(selected.OversamplingFactor() == SlotDistortion::FactorForRate(rate));
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
