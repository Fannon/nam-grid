#pragma once
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
      if (clip) aa.Clip(a, .8); else aa.Tanh(a);
      double reference = 0.;
      constexpr int count = 32768;
      for (int i = 0; i < count; ++i)
      {
        const double x = a + (b - a) * (i + .5) / count;
        reference += clip ? std::clamp(x, -.8, .8) : std::tanh(x);
      }
      const double actual = clip ? aa.Clip(b, .8) : aa.Tanh(b);
      assert(std::abs(actual - reference / count) < 2.e-8);
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
