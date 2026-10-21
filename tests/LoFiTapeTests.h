#pragma once
#include "SlotOperations.h"

namespace lofi_test
{
inline std::vector<double> Render(double rate, UtilityControls c, double hz, double seconds = 1.)
{
  SlotLoFiTape tape; tape.Reset(rate);
  std::vector<double> audio(static_cast<size_t>(rate * seconds));
  for (size_t i = 0; i < audio.size(); ++i)
    audio[i] = .3 * std::sin(2. * grid_fx::pi * hz * i / rate);
  tape.Process(audio.data(), nullptr, static_cast<int>(audio.size()), c);
  return audio;
}
inline double RMS(const std::vector<double>& audio)
{
  double sum = 0.;
  for (size_t i = audio.size() / 2; i < audio.size(); ++i) sum += audio[i] * audio[i];
  return std::sqrt(sum / (audio.size() - audio.size() / 2));
}
}

void TestLoFiTape()
{
  for (double rate : {44100., 48000., 88200., 96000., 192000.})
  {
    UtilityControls c;
    c.lofi = {30., 0., 0., 0., 0., 100., 0.};
    // Speed changes high-frequency loss while keeping bass and mean pitch intact.
    const auto fast = lofi_test::Render(rate, c, 8000.);
    c.lofi[0] = 1.875;
    const auto slow = lofi_test::Render(rate, c, 8000.);
    assert(lofi_test::RMS(fast) > 3. * lofi_test::RMS(slow));
    assert(lofi_test::RMS(lofi_test::Render(rate, c, 200.)) > .19);

    // Each transport control creates sidebands rather than only dulling a tone.
    c.lofi = {7.5, 0., 0., 0., 0., 100., 0.};
    const auto steady = lofi_test::Render(rate, c, 1000., 2.);
    {
      SlotLoFiTape meter; meter.Reset(rate);
      std::vector<double> silence(static_cast<size_t>(rate * 3.), 0.);
      meter.Process(silence.data(), nullptr, static_cast<int>(silence.size()), c);
      assert(meter.History().sequence >= 299 && meter.History().sequence <= 300);
      for (float value : meter.History().wow) assert(value == 0.f);
      for (float value : meter.History().flutter) assert(value == 0.f);
      c.lofi[1] = 100.; c.lofi[2] = 100.;
      meter.Reset(rate);
      meter.Process(silence.data(), nullptr, static_cast<int>(silence.size()), c);
      double wowPeak = 0., flutterPeak = 0.;
      for (float value : meter.History().wow) wowPeak = std::max(wowPeak, std::abs(static_cast<double>(value)));
      for (float value : meter.History().flutter) flutterPeak = std::max(flutterPeak, std::abs(static_cast<double>(value)));
      assert(wowPeak > 10. && wowPeak < 30. && flutterPeak > 15. && flutterPeak < 45.);
      c.lofi[1] = c.lofi[2] = 0.;
    }
    const auto coherent = [&](const std::vector<double>& audio) {
      double real = 0., imag = 0., energy = 0.;
      const size_t start = static_cast<size_t>(rate);
      for (size_t i = start; i < audio.size(); ++i)
      {
        const double phase = 2. * grid_fx::pi * 1000. * i / rate;
        real += audio[i] * std::cos(phase); imag += audio[i] * std::sin(phase);
        energy += audio[i] * audio[i];
      }
      return 2. * (real * real + imag * imag) / ((audio.size() - start) * energy);
    };
    assert(coherent(steady) > .99);
    for (int transport : {1, 2})
    {
      c.lofi[transport] = 100.;
      assert(coherent(lofi_test::Render(rate, c, 1000., 2.)) < .7);
      c.lofi[transport] = 0.;
    }

    // Wear produces smooth dropouts even at a low tone where bandwidth loss is small.
    const auto fresh = lofi_test::Render(rate, c, 200., 3.);
    c.lofi[4] = 100.;
    const auto worn = lofi_test::Render(rate, c, 200., 3.);
    double minimumRatio = 1.;
    const int window = static_cast<int>(rate * .02);
    for (size_t pos = static_cast<size_t>(rate / 2.); pos + window < worn.size(); pos += window)
    {
      double cleanEnergy = 0., wornEnergy = 0.;
      for (int i = 0; i < window; ++i)
      {
        cleanEnergy += fresh[pos + i] * fresh[pos + i];
        wornEnergy += worn[pos + i] * worn[pos + i];
      }
      minimumRatio = std::min(minimumRatio, wornEnergy / cleanEnergy);
    }
    assert(minimumRatio < .5);

    // Noise is explicitly controllable, stereo independent and repeatable on Reset.
    SlotLoFiTape tape; tape.Reset(rate);
    std::array<double, 4096> left{}, right{};
    c.lofi = {7.5, 100., 100., 0., 100., 100., 0.};
    tape.Process(left.data(), right.data(), static_cast<int>(left.size()), c);
    for (size_t i = 0; i < left.size(); ++i) assert(left[i] == 0. && right[i] == 0.);
    tape.Reset(rate); c.lofi[3] = 100.;
    tape.Process(left.data(), right.data(), static_cast<int>(left.size()), c);
    double noiseEnergy = 0., difference = 0.;
    for (size_t i = 0; i < left.size(); ++i)
    { noiseEnergy += left[i] * left[i]; difference += std::abs(left[i] - right[i]); }
    assert(noiseEnergy > .001 && difference > .1);
    const auto expected = left;
    left.fill(0.); right.fill(0.); tape.Reset(rate);
    tape.Process(left.data(), right.data(), static_cast<int>(left.size()), c);
    assert(left == expected);

    // Zero Mix is exact dry, independent of transport, wear and noise settings.
    tape.Reset(rate); c.lofi[5] = 0.;
    for (size_t i = 0; i < left.size(); ++i) left[i] = .3 * std::sin(i * .1);
    const auto dry = left;
    tape.Process(left.data(), nullptr, static_cast<int>(left.size()), c);
    assert(left == dry);

    // Stereo modulation stays mono-compatible without noise; no crossfeed from L to R.
    tape.Reset(rate); c.lofi[3] = 0.; c.lofi[5] = 100.;
    left = right = dry;
    tape.Process(left.data(), right.data(), static_cast<int>(left.size()), c);
    assert(left == right);
    tape.Reset(rate); left = dry; right.fill(0.);
    tape.Process(left.data(), right.data(), static_cast<int>(left.size()), c);
    for (double value : right) assert(value == 0.);

    // Parameter steps and random modulation are independent of host block partitioning.
    const auto renderBlocks = [&](bool tiny) {
      SlotLoFiTape effect; effect.Reset(rate);
      std::vector<double> audio(static_cast<size_t>(rate));
      for (size_t i = 0; i < audio.size(); ++i) audio[i] = .8 * std::sin(i * .07);
      for (int segment = 0; segment < 4; ++segment)
      {
        UtilityControls settings;
        settings.lofi = {segment % 2 ? 1.875 : 30., segment % 2 ? 100. : 0., 100., 100., 100., 100., 6.};
        const size_t end = audio.size() * (segment + 1) / 4;
        for (size_t pos = audio.size() * segment / 4, block = 0; pos < end;)
        {
          const int n = static_cast<int>(std::min<size_t>(end - pos, tiny ? std::array<int, 4>{1, 3, 64, 127}[block++ % 4] : 512));
          allocations = deletions = 0; tracking = true; Eigen::internal::set_is_malloc_allowed(false);
          effect.Process(audio.data() + pos, nullptr, n, settings);
          Eigen::internal::set_is_malloc_allowed(true); tracking = false;
          assert(allocations == 0 && deletions == 0);
          pos += n;
        }
      }
      return audio;
    };
    const auto large = renderBlocks(false), tiny = renderBlocks(true);
    for (size_t i = 0; i < tiny.size(); ++i)
      assert(std::isfinite(tiny[i]) && std::abs(tiny[i]) < 3. && std::abs(tiny[i] - large[i]) < 1.e-12);

    // Exercise actual slot dispatch, silent bypass, allocation-free draining and re-enable.
    for (bool stereo : {false, true})
    {
      SlotConfig config; config.effect = SlotEffect::LoFiTape;
      NAMStage stage; stage.Publish(PrepareSlot(config, rate, 512).dsp); stage.ResetStopped(rate, 512);
      assert(stage.Latency() == 0);
      UtilityControls settings; settings.lofi = {1.875, 100., 100., 100., 100., 100., 0.};
      std::array<double, 512> l{}, r{};
      for (int block = 0; block < static_cast<int>(rate * 1.2 / 512.) + 1; ++block)
      {
        if (block == 1) settings.lofi[3] = 0.; // Let the smoothed Noise knob settle during drain.
        l.fill(block == 0 ? .5 : 0.); r = l;
        allocations = deletions = 0; tracking = true; Eigen::internal::set_is_malloc_allowed(false);
        stage.Process(l.data(), 512, block == 0, 0., 0., false, 0., 0, 1., {}, {}, settings, stereo ? r.data() : nullptr);
        Eigen::internal::set_is_malloc_allowed(true); tracking = false;
        assert(allocations == 0 && deletions == 0);
        const auto meter = stage.TakeMeter();
        if (block > 1) assert(meter.lofi.sequence == 0); // Bypass clears transport telemetry.
        if (block > 3) for (double value : l) assert(value == 0.);
      }
      settings.lofi[3] = 0.; l.fill(0.); r.fill(0.);
      stage.Process(l.data(), 512, true, 0., 0., false, 0., 0, 1., {}, {}, settings, stereo ? r.data() : nullptr);
      for (size_t i = 0; i < l.size(); ++i) assert(std::abs(l[i]) < 1.e-20 && std::abs(r[i]) < 1.e-20);
    }
  }
  std::cout << "LoFi tape: speed loss, transport sidebands, wear/dropouts, noise, stereo, block invariance and bypass at 44.1-192 kHz\n";
}
