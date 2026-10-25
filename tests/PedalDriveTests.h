#pragma once
#include "SlotPedalDrive.h"

namespace pedal_test
{
// Deliberately direct, zero-stuffed FIR reference. This checks phase, ring wraps
// and roundoff independently of the production polyphase/vectorized loop layout.
class ReferenceOversampler
{
public:
  ReferenceOversampler()
  {
    double sum = 0.;
    for (int i = 0; i < 257; ++i)
    {
      const double x = i - 128.;
      const double sinc = x == 0. ? .225 : std::sin(.225 * grid_pedal::pi * x) / (grid_pedal::pi * x);
      kernel[i] =
        sinc * (.42 - .5 * std::cos(2. * grid_pedal::pi * i / 256.) + .08 * std::cos(4. * grid_pedal::pi * i / 256.));
      sum += kernel[i];
    }
    for (double& x : kernel)
      x /= sum;
  }
  double Tick(double input)
  {
    double result = 0.;
    for (int phase = 0; phase < 4; ++phase)
    {
      std::move_backward(up.begin(), up.end() - 1, up.end());
      up[0] = phase == 0 ? 4. * input : 0.;
      double value = 0.;
      for (int tap = 0; tap < 257; ++tap)
        value += kernel[tap] * up[tap];
      std::move_backward(down.begin(), down.end() - 1, down.end());
      down[0] = grid_pedal::Flush(std::tanh(8. * value));
      if (phase == 0)
        for (int tap = 0; tap < 257; ++tap)
          result += kernel[tap] * down[tap];
    }
    return grid_pedal::Flush(result);
  }

private:
  std::array<double, 257> kernel{}, up{}, down{};
};

inline double Harmonic(const std::vector<double>& x, int start, int count, int bin)
{
  double re = 0., im = 0.;
  for (int i = 0; i < count; ++i)
  {
    const double phase = 2. * grid_fx::pi * bin * i / count;
    re += x[start + i] * std::cos(phase);
    im += x[start + i] * std::sin(phase);
  }
  return 2. * std::hypot(re, im) / count;
}
inline std::vector<double> Render(double rate, UtilityControls controls, bool tiny, bool automate = false)
{
  SlotPedalDrive drive;
  drive.Reset(rate);
  const int count = automate ? 16384 : 4096;
  std::vector<double> audio(count);
  for (int i = 0; i < count; ++i)
    audio[i] = .3 * std::sin(.071 * i) + .1 * std::cos(.027 * i);
  for (int position = 0, block = 0; position < count;)
  {
    if (automate)
    {
      const int section = position / 512;
      controls.pedal = {double(section < 12 ? section % effect_modes::pedalCount : 2),
                        section % 2 ? 36. : 0.,
                        section % 2 ? 200. : 12000.,
                        section % 2 ? -6. : 6.,
                        section % 2 ? 50. : 500.,
                        section % 2 ? 100. : 0.,
                        -3.,
                        75.,
                        section % 2 ? -100. : 100.};
    }
    const int n =
      std::min({count - position, 512 - position % 512, tiny ? std::array<int, 4>{1, 3, 64, 127}[block++ % 4] : 512});
    allocations = deletions = 0;
    tracking = true;
    Eigen::internal::set_is_malloc_allowed(false);
    drive.Process(audio.data() + position, n, controls);
    Eigen::internal::set_is_malloc_allowed(true);
    tracking = false;
    assert(allocations == 0 && deletions == 0);
    position += n;
  }
  return audio;
}
} // namespace pedal_test

void TestPedalDrive()
{
  // The resampling filters must have the advertised phase delay and unity DC gain.
  grid_pedal::Oversampler4x oversampler;
  oversampler.Reset();
  std::array<double, 256> impulse{};
  for (int i = 0; i < impulse.size(); ++i)
    impulse[i] = oversampler.Tick(i == 0 ? 1. : 0., [](double x) { return x; });
  assert(std::max_element(impulse.begin(), impulse.end()) - impulse.begin() == SlotPedalDrive::latency);
  double sum = 0.;
  for (double x : impulse)
    sum += x;
  assert(std::abs(sum - 1.) < 1.e-5);

  // Compare the optimized FIRs with a direct high-rate reference, including
  // nonlinear clipping. The allowed difference is double-precision roundoff.
  pedal_test::ReferenceOversampler reference;
  oversampler.Reset();
  unsigned seed = 1337;
  for (int i = 0; i < 2048; ++i)
  {
    seed = 1664525u * seed + 1013904223u;
    const double input = .7 * (double(seed) / 4294967296. - .5) + .2 * std::sin(.093 * i);
    const double actual = oversampler.Tick(input, [](double x) { return std::tanh(8. * x); });
    assert(std::abs(actual - reference.Tick(input)) < 1.e-12);
  }

  // Measure a fifth-harmonic alias independently using a coherent high sine.
  constexpr int n = 8192, fundamental = 1093, alias = n - 5 * fundamental;
  std::vector<double> native(n), filtered(2 * n);
  oversampler.Reset();
  for (int i = 0; i < 2 * n; ++i)
  {
    const double x = 4. * std::sin(2. * grid_fx::pi * fundamental * i / n);
    if (i >= n)
      native[i - n] = std::tanh(x);
    filtered[i] = oversampler.Tick(x, [](double value) { return std::tanh(value); });
  }
  const double nativeAlias = pedal_test::Harmonic(native, 0, n, alias);
  const double filteredAlias = pedal_test::Harmonic(filtered, n, n, alias);
  assert(nativeAlias > .03 && filteredAlias < nativeAlias * .01);

  for (double rate : {44100., 48000., 96000., 192000.})
  {
    std::array<std::vector<double>, effect_modes::pedalCount> voices;
    for (int style = 0; style < effect_modes::pedalCount; ++style)
    {
      UtilityControls c;
      c.pedal[0] = style;
      c.pedal[6] = 0.;
      voices[style] = pedal_test::Render(rate, c, false);
      for (double x : voices[style])
        assert(std::isfinite(x) && std::abs(x) < 8.);
      for (int other = 0; other < style; ++other)
      {
        double difference = 0.;
        for (int i = 1024; i < voices[style].size(); ++i)
          difference += std::abs(voices[style][i] - voices[other][i]);
        assert(difference > 1.);
      }
      // Each style's character changes its sound in both directions, while
      // the center is the default. Exercise extremes at every supported rate.
      for (double character : {-100., 100.})
      {
        c.pedal[8] = character;
        const auto changed = pedal_test::Render(rate, c, false);
        double difference = 0.;
        for (int i = 0; i < changed.size(); ++i)
        {
          assert(std::isfinite(changed[i]) && std::abs(changed[i]) < 8.);
          if (i >= 1024)
            difference += std::abs(changed[i] - voices[style][i]);
        }
        assert(difference > .1);
      }
      SlotPedalDrive dry;
      dry.Reset(rate);
      c.pedal[7] = 0.;
      std::array<double, 256> x{}, original{};
      for (int i = 0; i < x.size(); ++i)
        x[i] = original[i] = .1 * std::sin(i * .1);
      dry.Process(x.data(), static_cast<int>(x.size()), c);
      for (int i = 0; i < x.size(); ++i)
        assert(x[i] == (i < SlotPedalDrive::latency ? 0. : original[i - SlotPedalDrive::latency]));
    }
    const auto large = pedal_test::Render(rate, {}, false, true);
    const auto tiny = pedal_test::Render(rate, {}, true, true);
    for (int i = 0; i < large.size(); ++i)
      assert(std::isfinite(tiny[i]) && std::abs(large[i] - tiny[i]) < 1.e-12);
    UtilityControls noSplit;
    noSplit.pedal[4] = 50.;
    const auto lowSplit = pedal_test::Render(rate, noSplit, false);
    noSplit.pedal[4] = 500.;
    assert(lowSplit == pedal_test::Render(rate, noSplit, false)); // Low keep zero disables local splitting.
  }
  // Strong drive at low B: Low keep retains a near-unity clean fundamental.
  constexpr int lowCount = 16384, lowBin = 11;
  std::array<double, 2> amplitude{};
  for (int keep = 0; keep < 2; ++keep)
  {
    SlotPedalDrive drive;
    drive.Reset(48000.);
    UtilityControls c;
    c.pedal = {0., 24., 200., 0., 500., keep ? 100. : 0., 0., 100.};
    std::vector<double> audio(2 * lowCount);
    for (int i = 0; i < audio.size(); ++i)
      audio[i] = .1 * std::sin(2. * grid_fx::pi * lowBin * i / lowCount);
    drive.Process(audio.data(), static_cast<int>(audio.size()), c);
    amplitude[keep] = pedal_test::Harmonic(audio, lowCount, lowCount, lowBin);
  }
  assert(amplitude[1] > .08 && amplitude[1] < .14);
  assert(std::abs(amplitude[1] - .1) < std::abs(amplitude[0] - .1) * .25);

  for (int style = 0; style < effect_modes::pedalCount; ++style)
  {
    SlotPedalDrive drive;
    drive.Reset(48000.);
    UtilityControls c;
    c.pedal[0] = style;
    c.pedal[1] = 36.;
    c.pedal[6] = 0.;
    std::vector<double> dc(48000, .5);
    drive.Process(dc.data(), static_cast<int>(dc.size()), c);
    assert(std::abs(dc.back()) < 1.e-8);
    drive.Reset(48000.);
    dc.assign(1024, 0.);
    drive.Process(dc.data(), static_cast<int>(dc.size()), c);
    for (double x : dc)
      assert(x == 0.);
  }
  // Positive Vintage Fuzz Bias intentionally gates quiet decays; the centered
  // voice retains them. This is independent of the module's outer Mix.
  std::array<double, 2> quietEnergy{};
  for (int pinched = 0; pinched < 2; ++pinched)
  {
    SlotPedalDrive drive;
    drive.Reset(48000.);
    UtilityControls c;
    c.pedal[0] = static_cast<int>(effect_modes::PedalStyle::VintageFuzz);
    c.pedal[1] = 18.;
    c.pedal[6] = 0.;
    c.pedal[8] = pinched ? 100. : 0.;
    std::array<double, 4096> quiet{};
    for (int i = 0; i < quiet.size(); ++i)
      quiet[i] = .001 * std::sin(.07 * i);
    drive.Process(quiet.data(), static_cast<int>(quiet.size()), c);
    for (double value : quiet)
      quietEnergy[pinched] += value * value;
  }
  assert(quietEnergy[0] > .001 && quietEnergy[1] < quietEnergy[0] * .001);

  // A delayed dry drive in B must reconstruct the input with the clean low A
  // lane, including when bypassed. This checks real stage and grid compensation.
  for (double rate : {44100., 48000., 96000.})
    for (bool enabled : {true, false})
    {
      FixedNAMGrid grid;
      SlotConfig config;
      config.effect = SlotEffect::PedalDrive;
      grid.stages[8].Publish(PrepareSlot(config, rate, 512).dsp);
      grid.ResetStopped(rate, 512);
      assert(grid.stages[8].Latency() == SlotPedalDrive::latency);
      std::array<FixedNAMGrid::Controls, 16> controls{};
      controls[8].enabled = enabled;
      controls[8].utility.pedal[6] = controls[8].utility.pedal[7] = 0.;
      grid_performance::Routing routing;
      routing.crossover = true;
      routing.frequency = 180.;
      std::array<double, 512> block{};
      std::vector<double> source(4096);
      for (int i = 0; i < source.size(); ++i)
        source[i] = .1 * std::sin(i * .03) + .05 * std::cos(i * .17);
      for (int offset = 0; offset < source.size(); offset += 512)
      {
        std::copy_n(source.data() + offset, 512, block.data());
        grid.Process(block.data(), 512, controls, .5, nullptr, routing);
        for (int i = 0; i < 512; ++i)
        {
          const int previous = offset + i - SlotPedalDrive::latency;
          assert(std::abs(block[i] - (previous < 0 ? 0. : source[previous])) < 1.e-12);
        }
      }
    }
  std::cout << "Pedal Drive: ten voices and centered character, alias reduction, low retention, blocks, allocation, "
               "dry/bypass and split "
               "alignment passed\n";
}
