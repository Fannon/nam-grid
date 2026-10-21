#pragma once
#include "EffectPreview.h"

void TestDualDelayMusicalControls()
{
  for (double rate : {44100., 48000., 96000., 192000.})
  {
    const int count = static_cast<int>(rate * .12);
    const auto render = [&](bool ping, bool tiny, bool automation, bool dry) {
      grid_roadmap::DualDelay delay; delay.Reset(rate);
      UtilityControls c; c.delay = {20., 50., 12000., 0., dry ? 0. : 100.};
      c.dual = {.75, 50.}; c.pingPong = ping;
      std::array<std::vector<double>, 2> audio{std::vector<double>(count), std::vector<double>(count)};
      if (automation || dry)
        for (int i = 0; i < count; ++i)
        {
          audio[0][i] = .15 * std::sin(i * .091);
          audio[1][i] = .1 * std::cos(i * .047);
        }
      else audio[0][0] = 1.;
      for (int pos = 0, block = 0; pos < count;)
      {
        // Control changes happen at the same samples under both partitions.
        const int segment = pos / 1024;
        if (automation) { c.delayOutput = segment % 2 ? -18. : 6.; c.pingPong = segment % 2; }
        const int boundary = std::min(count, (segment + 1) * 1024);
        const int n = std::min(boundary - pos, tiny ? std::array<int, 4>{1, 3, 64, 127}[block++ % 4] : 512);
        allocations = deletions = 0; tracking = true; Eigen::internal::set_is_malloc_allowed(false);
        delay.Process(audio[0].data() + pos, audio[1].data() + pos, n, c);
        Eigen::internal::set_is_malloc_allowed(true); tracking = false;
        assert(allocations == 0 && deletions == 0);
        pos += n;
      }
      return audio;
    };
    const auto independent = render(false, true, false, false);
    const auto ping = render(true, true, false, false);
    const auto first = [](const auto& audio) {
      for (int i = 0; i < audio.size(); ++i) if (std::abs(audio[i]) > 1.e-10) return i;
      return -1;
    };
    assert(std::abs(first(independent[0]) - .02 * rate) <= 1.);
    assert(first(independent[1]) == -1); // Independent taps do not invent cross-channel audio.
    assert(std::abs(first(ping[0]) - .02 * rate) <= 1.);
    assert(std::abs(first(ping[1]) - .035 * rate) <= 1.); // Left time + right time.
    UtilityControls preview; preview.delay = {20., 50., 12000., 0., 100.};
    preview.dual = {.75, 50.}; preview.pingPong = true;
    const auto echoes = effect_preview::Delay(SlotEffect::DualDelay, preview, rate);
    assert(std::abs(first(ping[0]) - echoes.taps[0].seconds * rate) <= 1.);
    assert(std::abs(first(ping[1]) - echoes.taps[1].seconds * rate) <= 1.);
    const auto large = render(false, false, true, false), partitioned = render(false, true, true, false);
    for (int ch = 0; ch < 2; ++ch)
      for (int i = 0; i < count; ++i)
      {
        assert(std::isfinite(partitioned[ch][i]) && std::abs(partitioned[ch][i]) < 2.);
        assert(std::abs(large[ch][i] - partitioned[ch][i]) < 1.e-12);
      }
    for (bool mode : {false, true})
    {
      const auto dry = render(mode, true, false, true);
      for (int i = 0; i < count; ++i)
      {
        assert(std::abs(dry[0][i] - .15 * std::sin(i * .091)) < 1.e-12);
        assert(std::abs(dry[1][i] - .1 * std::cos(i * .047)) < 1.e-12);
      }
    }
  }
  std::cout << "Dual Delay: independent/cross-feedback timing, stereo dry, automation and no allocations passed\n";
}

void TestDistortionTonePreview()
{
  // Hard clipping below its threshold is linear: compare the displayed tone,
  // DC rejection, dry/wet phase sum and output gain against actual samples.
  constexpr double rate = 48000.;
  for (double frequency : {100., 1000., 6000.})
    for (double mix : {0., 35., 100.})
    {
      SlotDistortion distortion; distortion.Reset(rate);
      UtilityControls c; c.distortion = {1., 0., 1800., 0., mix, -3.};
      double inputPower = 0., outputPower = 0.;
      for (int i = 0; i < 24000; ++i)
      {
        const double input = .01 * std::sin(2. * grid_fx::pi * frequency * i / rate);
        double output = input; distortion.Process(&output, 1, c);
        if (i >= 12000) { inputPower += input * input; outputPower += output * output; }
      }
      assert(std::abs(std::sqrt(outputPower / inputPower) - effect_preview::DistortionTone(frequency, c, rate)) < 2.e-5);
    }
}

void TestDistortionModeHarmonics()
{
  // Coherent sine: directly measure the musical distinction claimed by the
  // new styles, independently of the plot and without DC leaking into bins.
  const auto harmonic = [](effect_modes::DistortionStyle style, int order) {
    double real = 0., imaginary = 0.;
    constexpr int count = 8192;
    for (int i = 0; i < count; ++i)
    {
      const double phase = 2. * grid_fx::pi * 128. * i / count;
      const double wet = SlotDistortion::Transfer(.75 * std::sin(phase), static_cast<int>(style), 2., 0.);
      real += wet * std::cos(order * phase); imaginary += wet * std::sin(order * phase);
    }
    return 2. * std::hypot(real, imaginary) / count;
  };
  using Style = effect_modes::DistortionStyle;
  assert(harmonic(Style::Soft, 2) < 1.e-12);
  assert(harmonic(Style::Diode, 1) > .5 && harmonic(Style::Diode, 2) > .05);
  assert(harmonic(Style::Rectify, 1) < 1.e-12 && harmonic(Style::Rectify, 2) > .2);
  std::cout << "Distortion modes: asymmetric even harmonics and rectified octave passed\n";
}
