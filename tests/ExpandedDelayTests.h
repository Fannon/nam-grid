#pragma once
#include <complex>
#include "ExpandedDelay.h"
#include "SlotDefaults.h"

namespace expanded_delay_test
{
using E = SlotEffect;
using Audio = std::array<std::vector<double>, 2>;

inline void Option(UtilityControls& controls, int parameter, double value)
{ controls.expandedDelay[parameter - uSpaceHeads] = value; }

inline Audio Render(E effect, UtilityControls controls, double rate, int block, const Audio& source)
{
  SlotExpandedDelay delay;
  delay.Reset(rate, block, effect);
  auto result = source;
  const int before = allocations, destroyed = deletions;
  tracking = true;
  for (int first = 0, frames = static_cast<int>(source[0].size()); first < frames; first += block)
    delay.Process(result[0].data() + first, result[1].data() + first, std::min(block, frames - first), controls, false);
  tracking = false;
  assert(allocations == before && deletions == destroyed);
  return result;
}

inline double Peak(const std::vector<double>& signal, double rate, double start, double end)
{
  double peak = 0.;
  for (int i = static_cast<int>(rate * start); i < static_cast<int>(rate * end); ++i)
    peak = std::max(peak, std::abs(signal[i]));
  return peak;
}
inline double Band(const std::vector<double>& signal, double rate, double start, double end, double frequency)
{
  const int first = static_cast<int>(rate * start), count = static_cast<int>(rate * end) - first;
  const auto step = std::polar(1., -2. * grid_fx::pi * frequency / rate);
  std::complex<double> sum{}, phase{1., 0.};
  for (int i = 0; i < count; ++i)
  {
    const double window = .5 - .5 * std::cos(2. * grid_fx::pi * i / count);
    sum += window * signal[i + first] * phase;
    phase *= step;
  }
  return std::abs(sum);
}
}

void TestExpandedDelays()
{
  using namespace expanded_delay_test;
  for (const double rate : {44100., 48000., 96000.})
  {
    const int count = static_cast<int>(rate * 1.25);
    Audio impulse{std::vector<double>(count), std::vector<double>(count)};
    impulse[0][0] = impulse[1][0] = .25;
    UtilityControls c;
    c.delay = {300., 0., 10000., 0., 100.};
    c.delayMotion = 0.;
    Option(c, uSpaceHeads, 6.);
    Option(c, uEchoSpring, 0.);
    const auto tape = Render(E::SpaceTapeEcho, c, rate, 127, impulse);
    assert(Peak(tape[0], rate, 0., .099) == 0.);
    for (double head : {.1, .2, .3})
      assert(Peak(tape[0], rate, head - .001, head + .006) > .04);
    Option(c, uEchoSpring, 100.);
    const auto spring = Render(E::SpaceTapeEcho, c, rate, 37, impulse);
    assert(Peak(spring[0], rate, .047, .099) > .005); // Spring is excited before the first tape head.

    c.delay[0] = 400.;
    Option(c, uTapTime1, 30.);
    Option(c, uTapTime3, 64.);
    Option(c, uTapLevel2, 0.);
    Option(c, uEchoWidth, 100.);
    Option(c, uTapPan1, -100.);
    const auto pattern = Render(E::PatternDelay, c, rate, 37, impulse);
    assert(Peak(pattern[0], rate, .119, .125) > .05 && Peak(pattern[1], rate, .119, .125) == 0.);
    assert(Peak(pattern[0], rate, .199, .205) < 1.e-12);
    assert(Peak(pattern[0], rate, .255, .262) > .025);
    assert(Peak(pattern[0], rate, .399, .405) > .025);

    Audio note{std::vector<double>(count), std::vector<double>(count)};
    for (int i = 0, n = static_cast<int>(rate * .094); i < n; ++i)
      note[0][i] = note[1][i] = .23 * std::sin(2. * grid_fx::pi * 220. * i / rate) * std::sin(grid_fx::pi * i / n);
    c.delay = {200., 65., 9800., 0., 100.};
    Option(c, uEchoInterval, 12.);
    Option(c, uEchoShiftBlend, 100.);
    const auto pitch = Render(E::PitchDelay, c, rate, 37, note);
    assert(Band(pitch[0], rate, .225, .35, 440.) > 5. * Band(pitch[0], rate, .225, .35, 220.));
    assert(Band(pitch[0], rate, .445, .58, 880.) > 3. * Band(pitch[0], rate, .445, .58, 440.));

    // Every selector must produce a stable wet signal, and sample-based scheduling
    // must produce the same result in irregular and regular callback partitions.
    for (int id = static_cast<int>(E::SpaceTapeEcho); id <= static_cast<int>(E::GranularDelay); ++id)
    {
      const E effect = static_cast<E>(id);
      const int selector = ExpandedDelayModeControl(effect);
      c.delay = {140., 82., 9000., 65., 100.};
      c.delayMotion = 26.;
      Option(c, uEchoSpring, 15.);
      for (int mode = 0; mode < static_cast<int>(EffectChoices(selector).size()); ++mode)
      {
        Option(c, selector, mode);
        const auto a = Render(effect, c, rate, 37, note), b = Render(effect, c, rate, 128, note);
        assert(a == b);
        for (const auto& channel : a) for (const auto sample : channel) assert(std::isfinite(sample) && std::abs(sample) < 1.2);
        if (effect != E::FeedbackFXDelay || mode != 3) assert(Peak(a[0], rate, .025, .6) > .002);
        assert(Peak(a[0], rate, 1.05, 1.24) < .15);
      }
    }
  }
  {
    // Spring at 0% parks the parallel tank after it rings out. Raised again, it
    // must start from silence rather than replay the tail it held when parked.
    const double rate = 48000.;
    const int block = 64;
    UtilityControls c;
    c.delay = {4000., 0., 10000., 0., 100.}; // Tape heads arrive after each measured window.
    c.delayMotion = 0.;
    Option(c, uEchoSpring, 0.);
    SlotExpandedDelay delay;
    delay.Reset(rate, block, E::SpaceTapeEcho);
    Audio buffer{std::vector<double>(block), std::vector<double>(block)};
    const auto run = [&](double seconds, bool impulse) {
      double peak = 0.;
      for (int n = 0; n < static_cast<int>(seconds * rate / block); ++n)
      {
        for (auto& channel : buffer) std::fill(channel.begin(), channel.end(), 0.);
        if (impulse && n == 0) buffer[0][0] = buffer[1][0] = .25;
        delay.Process(buffer[0].data(), buffer[1].data(), block, c, false);
        for (const auto& channel : buffer) for (const double sample : channel) peak = std::max(peak, std::abs(sample));
      }
      return peak;
    };
    assert(run(.5, true) == 0. && !delay.SpringRunning()); // A Spring inserted at 0% never runs.
    Option(c, uEchoSpring, 100.);
    assert(run(.5, true) > .005 && delay.SpringRunning());
    Option(c, uEchoSpring, 0.);
    run(4.5, false);
    assert(!delay.SpringRunning());
    Option(c, uEchoSpring, 100.);
    assert(run(.3, false) < 1.e-9);
  }
  std::cout << "Expanded delays: tap timing, parallel spring, spring parking, octave regeneration, bounded tails and partition identity passed\n";
}
