#pragma once
#include <array>
#include "SlotOutboard.h"
#include "SlotTimeEffects.h"
#include "dsp/Antiderivative.h"

void TestDriveAntialiasing()
{
  // The split interface must exactly match the established fused FIR loop.
  const auto split = []<int Factor, int Taps>() {
    grid_oversampling::FIR<Factor, Taps> fused, separate;
    fused.Reset(); separate.Reset();
    for (int i = 0; i < 2048; ++i)
    {
      const double x = .4 * std::sin(i * .17) + .1 * std::cos(i * .013);
      const auto shape = [](double v) { return std::tanh(3. * v); };
      auto up = separate.Upsample(x);
      for (double& v : up) v = shape(v);
      assert(fused.Tick(x, shape) == separate.Downsample(up));
    }
    assert(std::abs(std::abs(separate.Response(10000., Factor * 48000.)) - 1.) < .002);
    assert(std::abs(separate.Response(28800., Factor * 48000.)) < .01);
  };
  split.operator()<2, 32>(); split.operator()<4, 32>();
  split.operator()<2, 64>(); split.operator()<4, 64>(); split.operator()<8, 64>();

  // Mono Tape computes the same L output as the original stereo algorithm,
  // including silence/denormals, while the discarded R input is independent.
  for (double rate : {44100., 48000., 88200., 96000., 192000.})
  {
    AirwindowsTape stereo, mono; stereo.Reset(rate); mono.Reset(rate);
    std::array<double, 128> l{}, r{}, expected{}, actual{};
    for (int block = 0; block < 48; ++block)
    {
      const int n = std::array<int, 4>{1, 3, 37, 128}[block % 4];
      stereo.Set((block % 4 - 1) * 6., 25.); mono.Set((block % 4 - 1) * 6., 25.);
      for (int i = 0; i < n; ++i)
      {
        l[i] = actual[i] = block % 7 == 0 ? 0. : .7 * std::sin((block * 128 + i) * .017);
        r[i] = .5 * std::cos(i * .031);
      }
      double* in[]{l.data(), r.data()}; double* out[]{expected.data(), r.data()};
      stereo.Process(in, out, n); mono.ProcessMono(actual.data(), n);
      for (int i = 0; i < n; ++i) assert(expected[i] == actual[i]);
    }
  }
  // Independent midpoint quadrature validates fixed-curve interval averages.
  for (const auto [a, b] : {std::pair{-3., 3.}, std::pair{-.3, 2.}, std::pair{-2., -.1}, std::pair{.1, .100000001}})
    for (bool clip : {false, true})
    {
      grid_adaa::Average aa;
      const auto shape = [clip](double v) { return clip ? std::clamp(v, -.8, .8) : std::tanh(v); };
      const auto integral = [clip](double v) {
        const double magnitude = std::abs(v);
        return clip ? (magnitude <= .8 ? .5 * v * v : .8 * (magnitude - .4)) : grid_adaa::LogCosh(v);
      };
      aa.Tick(a, shape, integral);
      double reference = 0.;
      constexpr int count = 32768;
      for (int i = 0; i < count; ++i)
      {
        const double x = a + (b - a) * (i + .5) / count;
        reference += shape(x);
      }
      assert(std::abs(aa.Tick(b, shape, integral) - reference / count) < 2.e-8);
    }
  // An arbitrary integration constant must cancel on the very first interval.
  {
    grid_adaa::Average average;
    const double result = average.Tick(.5, [](double x) { return std::tanh(x); },
                                      [](double x) { return grid_adaa::LogCosh(x) + 7.25; });
    assert(std::abs(result - 2. * grid_adaa::LogCosh(.5)) < 1.e-13);
  }
  // Numerically integrate the continuous slew/clip system at much finer time
  // resolution. Start each independent interval at the production endpoint.
  grid_adaa::SlewClip slew;
  double previous = 0.;
  for (int i = 0; i < 128; ++i)
  {
    const double input = 2.8 * std::sin(i * .71), limit = .03 + .5 * (i % 5) / 4.;
    double state = slew.State(), sum = 0.;
    constexpr int steps = 8192;
    for (int j = 0; j < steps; ++j)
    {
      const double target = previous + (input - previous) * (j + .5) / steps;
      state += std::clamp(target - state, -limit / steps, limit / steps);
      sum += std::clamp(state, -.8, .8);
    }
    const double actual = slew.Tick(input, limit, .8);
    assert(std::abs(actual - sum / steps) < 3. * limit / steps + 1.e-8);
    assert(std::abs(slew.State() - state) < 2. * limit / steps + 1.e-8);
    previous = input;
  }
  std::cout << "Drive antialiasing: split FIR, mono Tape, analytic intervals and continuous slew references passed\n";
}

// Partial Mix must blend after the oversampling filters. Mixing an unshifted dry
// reference with the wet path's minimum-phase group delay combs around 6.5 kHz
// at 48 kHz (measured at -20 dB for Console Colour and -32 dB for Tape).
namespace
{
constexpr double blendPi = 3.14159265358979323846;
template <class Process>
double SineGain(double rate, double frequency, double amplitude, Process&& process)
{
  constexpr int block = 64;
  const int total = static_cast<int>(rate * 2.), settle = static_cast<int>(rate);
  std::array<double, block> buffer{};
  double inPhase = 0., quadrature = 0.;
  int count = 0;
  for (int start = 0; start < total; start += block)
  {
    for (int i = 0; i < block; ++i)
      buffer[i] = amplitude * std::sin(2. * blendPi * frequency * (start + i) / rate);
    process(buffer.data(), block);
    for (int i = 0; i < block; ++i)
    {
      if (start + i < settle) continue;
      const double phase = 2. * blendPi * frequency * (start + i) / rate;
      inPhase += buffer[i] * std::sin(phase);
      quadrature += buffer[i] * std::cos(phase);
      ++count;
    }
  }
  return 2. * std::hypot(inPhase, quadrature) / count / amplitude;
}
} // namespace

void TestOversampledWetDryBlend()
{
  // The filters are transparent in this band, so a 50% blend must track the fully
  // wet response; a dry/wet comb shows up as a 20 dB or deeper dip at 6.5 kHz.
  for (double rate : {44100., 48000.})
  {
    const auto tapeGain = [&](double mix, double frequency) {
      SlotTape tape; tape.Reset(rate);
      UtilityControls c; c.tape = {0., 50., mix, 0.};
      return SineGain(rate, frequency, .05, [&](double* samples, int frames) { tape.Process(samples, frames, c); });
    };
    const auto colourGain = [&](double mix, double frequency) {
      SlotOutboard colour; colour.Reset(rate, SlotEffect::ConsoleColour);
      UtilityControls c; c.colour = {0., 0., mix}; c.colourDesk = 4;
      std::array<double, 64> right{};
      return SineGain(rate, frequency, .05, [&](double* samples, int frames) {
        std::copy_n(samples, frames, right.begin());
        colour.Process(samples, right.data(), frames, c);
      });
    };
    for (double frequency : {1000., 4000., 6500., 10000.})
      for (bool tape : {true, false})
      {
        const auto gain = [&](double mix) { return tape ? tapeGain(mix, frequency) : colourGain(mix, frequency); };
        const double wet = 20. * std::log10(gain(100.));
        const double half = 20. * std::log10(gain(50.));
        assert(std::abs(half - wet) < 1.5);
      }
    // Mix 0% is exactly dry, including after a fresh Reset.
    std::array<double, 256> input{}, output{}, right{};
    for (size_t i = 0; i < input.size(); ++i) input[i] = .3 * std::sin(i * .071) + .1 * std::cos(i * .0131);
    {
      SlotTape tape; tape.Reset(rate);
      // Output stays at 0 dB so the dry sample is not scaled; drive and Bump are
      // hot to prove the wet path cannot leak into Mix 0.
      UtilityControls c; c.tape = {12., 80., 0., 0.};
      output = input;
      tape.Process(output.data(), static_cast<int>(output.size()), c);
      assert(output == input);
    }
    {
      SlotOutboard colour; colour.Reset(rate, SlotEffect::ConsoleColour);
      UtilityControls c; c.colour = {18., -6., 0.}; c.colourDesk = 6;
      output = input; right = input;
      colour.Process(output.data(), right.data(), static_cast<int>(output.size()), c);
      assert(output == input && right == input);
    }
  }
  std::cout << "Oversampled wet/dry: partial Mix stays aligned, Mix 0 is exact dry\n";
}
