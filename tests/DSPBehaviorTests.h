#pragma once
#include "HQEffectsTests.h"
#include "SlotOperations.h"

namespace dsp_behavior_test
{
// Schroeder integration in a broad 1 kHz band, with a -5 to -25 dB slope
// extrapolated to RT60. The filter and measurement do not use reverb internals.
inline double DecayTime(std::vector<double> x, double rate)
{
  const double w = 2. * grid_fx::pi * 1000. / rate;
  const double alpha = std::sin(w) / 2., a0 = 1. + alpha, a1 = -2. * std::cos(w);
  double x1 = 0., x2 = 0., y1 = 0., y2 = 0.;
  for (double& value : x)
  {
    const double y = (alpha * (value - x2) - a1 * y1 - (1. - alpha) * y2) / a0;
    x2 = x1; x1 = value; y2 = y1; y1 = y; value = y;
  }
  std::vector<double> energy(x.size() + 1);
  for (size_t i = x.size(); i-- > 0;) energy[i] = energy[i + 1] + x[i] * x[i];
  assert(energy[0] > 1.e-10);
  size_t first = 0, last = 0;
  for (size_t i = 0; i < x.size(); ++i)
  {
    if (!first && energy[i] <= energy[0] * std::pow(10., -.5)) first = i;
    if (energy[i] <= energy[0] * std::pow(10., -2.5)) { last = i; break; }
  }
  assert(first > 0 && last > first);
  return 3. * (last - first) / rate;
}

inline void Modulation()
{
  for (double rate : {44100., 48000., 96000.})
    for (auto effect : {SlotEffect::Phaser, SlotEffect::Flanger})
    {
      std::array<int, 3> notchCounts{};
      int setting = 0;
      for (double depth : {0., 50., 100.})
      {
        SlotModulation mod; mod.Reset(rate);
        UtilityControls c; c.modulation = {1., depth, 0., 0., 50.};
        const int count = static_cast<int>(2.5 * rate), window = static_cast<int>(.002 * rate);
        std::vector<double> input(count), output(count);
        for (int i = 0; i < count; ++i)
          input[i] = output[i] = .1 * std::sin(2. * grid_fx::pi * 1100. * i / rate);
        mod.Process(output.data(), count, effect, c);
        const auto gain = [&](int start) {
          double dry = 0., wet = 0.;
          for (int i = start; i < start + window; ++i) { dry += input[i] * input[i]; wet += output[i] * output[i]; }
          return std::sqrt(wet / dry);
        };
        double low = 2., high = 0., periodicError = 0.;
        bool inNotch = false;
        const int period = static_cast<int>(rate), start = period / 2;
        for (int i = start; i + window <= start + period; i += window / 2)
        {
          const double value = gain(i);
          low = std::min(low, value); high = std::max(high, value);
          periodicError = std::max(periodicError, std::abs(value - gain(i + period)));
          if (!inNotch && value < .25) { ++notchCounts[setting]; inNotch = true; }
          else if (inNotch && value > .5) inNotch = false;
        }
        if (depth == 0.) assert(high - low < .1);
        else
        {
          // Sweeping deep notches and recoveries exclude a static filter or a no-op.
          assert(low < .1 && high > .9 && high < 1.1);
          assert(periodicError < .02); // Envelope repeats at the requested 1 Hz.
          assert(notchCounts[setting] >= 2);
        }
        ++setting;
      }
      // A wider sweep crosses more cancellation frequencies; ignores preview math.
      assert(notchCounts[2] > notchCounts[1] && notchCounts[0] == 0);
    }
}

inline void Reverbs()
{
  for (double rate : {44100., 48000., 96000.})
    for (auto effect : {SlotEffect::RoomReverb, SlotEffect::PlateReverb, SlotEffect::CloudReverb, SlotEffect::HallReverb})
    {
      double shortDecay = 0.;
      for (double decay : {.5, 2.})
      {
        SlotReverb reverb; reverb.Reset(rate, effect);
        UtilityControls c; c.reverb = {decay, 100., 12000., 0., 100.}; c.reverbMotion = 0.; c.reverbHighPass = 0.;
        const int count = static_cast<int>(rate * (decay * 1.5 + .5));
        std::vector<double> left(count), right(count); left[0] = right[0] = 1.;
        reverb.Process(left.data(), right.data(), count, c);
        const double measured = DecayTime(left, rate);
        assert(measured > .8 * decay && measured < 1.2 * decay);
        if (decay == .5) shortDecay = measured;
        else assert(measured > 1.8 * shortDecay); // All tanks must respond to Decay.
      }
    }
}

inline double TankDecay(SlotEffect effect, double rate, double size, double decay)
{
  SlotReverb reverb; reverb.Reset(rate, effect);
  UtilityControls c; c.reverb = {decay, size, 12000., 0., 100.};
  c.reverbMotion = c.reverbHighPass = 0.;
  const int count = static_cast<int>(rate * (2. * decay + 1.));
  std::vector<double> left(count), right(count); left[0] = right[0] = 1.;
  reverb.Process(left.data(), right.data(), count, c);
  return DecayTime(left, rate);
}

inline void TankCalibration()
{
  // Size changes spacing, not the time energy may survive inside the allpasses.
  // Measure audio at three rates and sizes, including a short tail in a large tank.
  for (auto effect : {SlotEffect::PlateReverb, SlotEffect::CloudReverb})
  {
    for (double rate : {44100., 48000., 96000.})
      for (double size : {50., 100., 200.})
        for (double decay : {.5, 2., 6.})
        {
          const double measured = TankDecay(effect, rate, size, decay);
          assert(measured > .8 * decay && measured < 1.2 * decay);
        }
    // Long tails must not lose energy on every loop pass beyond Decay and the
    // brightest Damping (Cloud's interpolated reads gave ~.78x at 15 s).
    for (double size : {100., 200.})
    {
      const double measured = TankDecay(effect, 48000., size, 15.);
      assert(measured > .85 * 15. && measured < 1.1 * 15.);
    }
  }
}

inline void Octaver()
{
  for (double rate : {44100., 48000., 96000.})
    for (int voice : {1, 2})
    {
      grid_roadmap::Octaver octaver; octaver.Reset(rate);
      UtilityControls c; c.octaver = {0., voice == 1 ? 100. : 0., voice == 2 ? 100. : 0., 400., 0.};
      std::vector<double> output(static_cast<size_t>(2.4 * rate));
      for (size_t i = 0; i < output.size(); ++i) output[i] = .2 * std::sin(2. * grid_fx::pi * 110. * i / rate);
      octaver.Process(output.data(), nullptr, static_cast<int>(output.size()), c);
      const size_t start = static_cast<size_t>(.4 * rate); // Two seconds: integer cycles of both voices.
      const double wantedHz = voice == 1 ? 55. : 27.5, otherHz = voice == 1 ? 27.5 : 55.;
      const double wanted = hq_test::ToneEnergy(output, start, wantedHz, rate);
      assert(wanted > .08 && wanted < .15); // Measured .115-.117, not a near-silent subharmonic.
      assert(wanted > 20. * hq_test::ToneEnergy(output, start, 110., rate));
      assert(wanted > 20. * hq_test::ToneEnergy(output, start, otherHz, rate));
    }
}

inline void Stutter()
{
  for (double rate : {44100., 48000., 96000.})
  {
    grid_roadmap::Capture stutter; stutter.Reset(rate);
    UtilityControls c; c.stutter = {10., 50., 1., 100.}; c.extra.stutterHold = 1.;
    const int period = static_cast<int>(rate * .01), count = period * 8;
    std::vector<double> output(count);
    for (int i = 0; i < count; ++i) output[i] = .2 * std::sin(2. * grid_fx::pi * 317. * i / rate);
    stutter.Process(output.data(), nullptr, count, SlotEffect::Stutter, c);
    double energy = 0., dryDifference = 0.;
    for (int i = period * 4; i < period * 6; ++i)
    {
      assert(std::abs(output[i] - output[i + period]) < 1.e-9);
      energy += output[i] * output[i];
      dryDifference += std::abs(output[i] - .2 * std::sin(2. * grid_fx::pi * 317. * i / rate));
    }
    assert(energy / (2 * period) > .001 && dryDifference / (2 * period) > .03);
  }
}

inline void Doubler()
{
  for (double rate : {44100., 48000., 96000.})
    for (double cents : {9., 25.})
    {
      const auto render = [&](double width, double mix) {
        grid_roadmap::Doubler doubler; doubler.Reset(rate);
        UtilityControls c; c.doubler = {cents, 12., width, mix, 0.};
        c.extra.doubleHumanize = 0.; c.extra.doubleHighCut = 20000.;
        std::array<std::vector<double>, 2> out{std::vector<double>(static_cast<size_t>(2. * rate)), {}};
        out[1].resize(out[0].size());
        for (size_t i = 0; i < out[0].size(); ++i)
          out[0][i] = out[1][i] = .2 * std::sin(2. * grid_fx::pi * 440. * i / rate);
        doubler.Process(out[0].data(), out[1].data(), static_cast<int>(out[0].size()), c);
        return out;
      };
      const auto wide = render(100., 100.), mono = render(0., 100.), dry = render(100., 0.);
      const size_t start = static_cast<size_t>(rate / 2.);
      for (int channel = 0; channel < 2; ++channel)
      {
        const double wantedHz = 440. * std::exp2((channel ? -cents : cents) / 1200.);
        const double oppositeHz = 440. * std::exp2((channel ? cents : -cents) / 1200.);
        const double wanted = hq_test::ToneEnergy(wide[channel], start, wantedHz, rate);
        assert(wanted > .06 && wanted < .12);
        assert(wanted > 5. * hq_test::ToneEnergy(wide[channel], start, 440., rate));
        assert(wanted > 20. * hq_test::ToneEnergy(wide[channel], start, oppositeHz, rate));
        for (size_t i = start; i < wide[channel].size(); ++i)
        {
          assert(std::abs(mono[channel][i] - .5 * (wide[0][i] + wide[1][i])) < 1.e-12);
          assert(std::abs(dry[channel][i] - .2 * std::sin(2. * grid_fx::pi * 440. * i / rate)) < 1.e-12);
        }
      }
    }
}
} // namespace dsp_behavior_test

inline void TestDSPBehavior()
{
  dsp_behavior_test::Modulation();
  dsp_behavior_test::Reverbs();
  dsp_behavior_test::TankCalibration();
  dsp_behavior_test::Octaver();
  dsp_behavior_test::Doubler();
  dsp_behavior_test::Stutter();
  std::cout << "DSP behavior: independent notch sweeps, decay times, octave voices, stereo detuning and stutter passed\n";
}
