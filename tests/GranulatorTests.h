#pragma once
#include "SlotGranulator.h"
#include <iostream>

namespace granulator_test
{
using Mode = effect_modes::GranulatorMode;
// Position, Size, Activity, Shape, Filter, Repeats, Space, Mix.
using Knobs = std::array<double, 8>;
struct Patch
{
  Mode mode = Mode::Cloud;
  Knobs knobs = [] {
    Knobs k{};
    for (size_t i = 0; i < k.size(); ++i) k[i] = kEffectParameters[uGranulatorPosition + i].initial;
    return k;
  }();
  Patch& Set(int control, double value) { knobs[static_cast<size_t>(control - uGranulatorPosition)] = value; return *this; }
};
// A plucked-note stand-in: a decaying harmonic tone retriggered every 600 ms.
inline double Note(int i, double rate, int ch = 0)
{
  const double t = std::fmod(i / rate, .6), f = 196. * (1. + .002 * ch);
  double x = 0.;
  for (int h = 1; h <= 6; ++h) x += std::sin(2. * grid_fx::pi * f * h * t + h) / h;
  return .25 * x * std::exp(-4. * t);
}
struct Render
{
  std::vector<double> left, right;
  double Rms(double from, double to, double rate) const
  {
    double sum = 0.;
    const auto a = static_cast<size_t>(from * rate), b = std::min(left.size(), static_cast<size_t>(to * rate));
    for (size_t i = a; i < b; ++i) sum += left[i] * left[i] + right[i] * right[i];
    return std::sqrt(sum / std::max<size_t>(1, 2 * (b - a)));
  }
  double Peak() const
  {
    double peak = 0.;
    for (size_t i = 0; i < left.size(); ++i) peak = std::max({peak, std::abs(left[i]), std::abs(right[i])});
    return peak;
  }
};
// Renders `seconds` in 128-frame blocks. `input(i, ch)` gives the signal; `freeze(i)` and
// `drain(i)` the per-block states at the block's first frame.
template <class Input, class Freeze = bool (*)(int), class Drain = bool (*)(int)>
Render Run(const Patch& patch, double seconds, double rate, Input input, bool stereo = true,
           Freeze freeze = [](int) { return false; }, Drain drain = [](int) { return false; })
{
  SlotGranulator slot;
  slot.Reset(rate);
  UtilityControls c;
  c.granulator = patch.knobs;
  c.granulatorMode = static_cast<int>(patch.mode);
  const int frames = static_cast<int>(seconds * rate);
  Render out{std::vector<double>(frames), std::vector<double>(frames)};
  for (int i = 0; i < frames; ++i) { out.left[i] = input(i, 0); out.right[i] = input(i, 1); }
  for (int pos = 0; pos < frames; pos += 128)
  {
    const int n = std::min(128, frames - pos);
    c.granulatorFreeze = freeze(pos);
    slot.Process(out.left.data() + pos, stereo ? out.right.data() + pos : nullptr, n, c, drain(pos));
  }
  if (!stereo) out.right = out.left;
  return out;
}
inline double Goertzel(const std::vector<double>& x, size_t from, size_t to, double hz, double rate)
{
  const double w = 2. * grid_fx::pi * hz / rate, k = 2. * std::cos(w);
  double s1 = 0., s2 = 0.;
  for (size_t i = from; i < to; ++i) { const double s = x[i] + k * s1 - s2; s2 = s1; s1 = s; }
  return s1 * s1 + s2 * s2 - k * s1 * s2;
}
inline Patch Wet(Mode mode)
{
  Patch p; p.mode = mode;
  return p.Set(uGranulatorRepeats, 0.).Set(uGranulatorSpace, 0.).Set(uGranulatorMix, 100.);
}
// Clock isolated Arp grains. A tone at 0.7 * rate after transposition folds onto
// the same output frequency as the reference at 0.3 * rate.
inline void TestUpwardBandwidth()
{
  for (double rate : {44100., 48000., 96000.})
    for (int step = 1; step <= 4; ++step)
    {
      constexpr double intervals[]{0., 7., 12., 19., 24.};
      const double ratio = std::exp2(intervals[step] / 12.);
      const auto level = [&](double outputFraction) {
        grid_granular::Engine engine;
        engine.Reset(rate);
        grid_granular::Settings settings;
        settings.mode = static_cast<int>(Mode::Arp);
        settings.size = 60.; settings.position = 0.; settings.repeats = settings.space = 0.;
        settings.periodSeconds = .125;
        engine.Configure(settings);
        // Second cycle: enough history for even the fastest grain.
        const int onset = static_cast<int>((8 + step) * settings.periodSeconds * rate);
        const int begin = onset + static_cast<int>(.02 * rate), end = onset + static_cast<int>(.04 * rate);
        double sum = 0.;
        for (int i = 0; i < end; ++i)
        {
          const double input = .3 * std::sin(2. * grid_fx::pi * outputFraction / ratio * i);
          const auto wet = engine.Tick({input, input}, true, false, 1.);
          if (i >= begin) sum += wet[0] * wet[0] + wet[1] * wet[1];
        }
        return std::sqrt(sum / (2. * (end - begin)));
      };
      const double pass = level(.3), folded = level(.7);
      assert(pass > .02 && folded < .01 * pass);
    }
}
} // namespace granulator_test

void TestGranulator()
{
  using namespace granulator_test;
  for (double hz : {31., 41., 62.})
  {
    grid_granular::PickDetector detector;
    detector.Reset(48000.);
    int picks = 0;
    for (int i = 0; i < 24000; ++i)
    {
      const double t = i / 48000.;
      const double pluck = .3 * std::min(1., t / .06) * std::exp(-4. * t) * std::sin(2. * grid_fx::pi * hz * t);
      picks += detector.Tick(std::abs(pluck));
    }
    assert(picks == 1);
  }
  {
    grid_granular::Engine engine;
    engine.Reset(48000.);
    grid_granular::Settings s;
    s.size = 20.;
    s.mode = static_cast<int>(Mode::Blocks);
    engine.Configure(s);
    engine.Tick({.2, .2}, true, false, 1.);
    s.mode = static_cast<int>(Mode::PickBurst);
    engine.Configure(s);
    for (int i = 0; i < 16000; ++i)
    {
      const double note = .2 * std::cos(2. * grid_fx::pi * 110. * i / 48000.);
      engine.Tick({note, note}, true, false, 1.);
      assert(engine.Active() == 0);
    }
  }
  // Dry right-channel attacks play a three-grain response while the source is frozen.
  // A held note launches nothing further. A fresh pick restarts the burst clock.
  {
    grid_granular::Engine engine;
    constexpr int rate = 48000;
    engine.Reset(rate);
    grid_granular::Settings s;
    s.mode = static_cast<int>(Mode::PickBurst);
    s.position = s.repeats = s.space = 0.;
    s.size = 20.; s.periodSeconds = .1;
    engine.Configure(s);
    std::vector<int> starts;
    int previous = 0;
    double frozenEnergy = 0.;
    for (int i = 0; i < rate * 2; ++i)
    {
      const bool frozen = i >= rate / 2;
      const double tone = .25 * std::cos(2. * grid_fx::pi * 220. * i / rate);
      const double right = (i >= 36000 && i < 38200) || i >= 42000 ? tone : 0.;
      const auto wet = engine.Tick({frozen ? 0. : tone, right}, true, frozen, 1.);
      const int active = engine.Active();
      if (frozen && active > previous) starts.push_back(i);
      if (frozen) frozenEnergy += wet[0] * wet[0];
      previous = active;
    }
    constexpr int expected[]{36000, 40800, 42000, 46800, 51600};
    assert(starts.size() == std::size(expected) && frozenEnergy > 1.);
    for (size_t i = 0; i < starts.size(); ++i) assert(std::abs(starts[i] - expected[i]) <= 2);
    for (const double period : {0., .1, 8.})
    {
      s.periodSeconds = period;
      const auto burst = grid_granular::PreviewPhrase(s);
      assert(burst.size() == 3 && burst[0].start == 0.);
      assert(burst[2].start == 2. * (period > 0. ? period : .13));
    }
  }
  TestUpwardBandwidth();
  // Eighth-note grain onsets follow the host clock independently of Size and Activity.
  for (const auto mode : {Mode::Blocks, Mode::Arp, Mode::Haze})
    for (const double size : {20., 70.})
    {
      SlotGranulator slot;
      constexpr double rate = 44100.;
      slot.Reset(rate);
      UtilityControls c;
      c.sync = true; c.tempo = 137.; c.division = 3;
      c.granulatorMode = static_cast<int>(mode);
      c.granulator[uGranulatorSize - uGranulatorPosition] = size;
      c.granulator[uGranulatorActivity - uGranulatorPosition] = size;
      const double period = rate * grid_tempo::Seconds(c.tempo, c.division);
      int previous = 0, starts = 0;
      for (int i = 0; i < 5.5 * period; ++i)
      {
        double sample = 0.;
        slot.Process(&sample, nullptr, 1, c);
        const int active = slot.ActiveGrains();
        if (active > previous)
        {
          assert(std::abs(i - starts * period) <= 1.);
          ++starts;
        }
        previous = active;
      }
      assert(starts == 6);
    }
  const auto note = [](double rate) { return [rate](int i, int ch) { return Note(i, rate, ch); }; };
  const auto sine = [](double hz) { return [hz](int i, int) { return .3 * std::sin(2. * grid_fx::pi * hz * i / 48000.); }; };
  // Mix 0 passes the input through exactly, in every mode, mono and stereo.
  for (int m = 0; m < effect_modes::granulatorCount; ++m)
    for (const bool stereo : {false, true})
    {
      Patch p; p.mode = static_cast<Mode>(m); p.Set(uGranulatorMix, 0.);
      const auto out = Run(p, .3, 48000., note(48000.), stereo);
      for (int i = 0; i < 14400; ++i) assert(out.left[i] == Note(i, 48000., 0));
      if (stereo) for (int i = 0; i < 14400; ++i) assert(out.right[i] == Note(i, 48000., 1));
    }
  // Every mode stays near the input level across Activity and Shape: never louder than
  // +2 dB, within 5.5 dB once grains overlap, and only quieter (gaps) when sparse.
  for (const double rate : {44100., 48000., 96000.})
  {
    Render input{std::vector<double>(static_cast<size_t>(6. * rate)), std::vector<double>(static_cast<size_t>(6. * rate))};
    for (size_t i = 0; i < input.left.size(); ++i)
    { input.left[i] = Note(static_cast<int>(i), rate, 0); input.right[i] = Note(static_cast<int>(i), rate, 1); }
    for (int m = 0; m < effect_modes::granulatorCount; ++m)
      for (const double activity : {15., 60., 100.})
        for (const double shape : {0., 50., 100.})
        {
          if (rate != 48000. && shape != 50.) continue;
          const auto mode = static_cast<Mode>(m);
          auto p = Wet(mode); p.Set(uGranulatorActivity, activity).Set(uGranulatorShape, shape);
          const auto wet = Run(p, 6., rate, note(rate));
          const double level = 20. * std::log10(wet.Rms(2., 6., rate) / input.Rms(2., 6., rate));
          const double peak = wet.Peak() / input.Peak();
          // Pick Burst falls silent after three grains even at its fastest rate.
          const bool dense = mode != Mode::PickBurst && grid_granular::Overlap(grid_granular::Voice(m), activity * .01) >= 3.;
          const bool ok = level < 2. && peak < 2. && (dense ? level > -5.5 : level > -22.);
          if (!ok)
            std::cerr << "Granulator level: " << effect_modes::granulatorNames[m] << " rate " << rate << " activity "
                      << activity << " shape " << shape << ": " << level << " dB, peak x" << peak << '\n';
          assert(ok);
        }
  }
  // Pitched modes: Shimmer adds the octave above, Tunnel the octave below, Arp the fifth.
  {
    const auto energy = [&](Mode mode, double hz) {
      auto p = Wet(mode); p.Set(uGranulatorPosition, 0.).Set(uGranulatorActivity, 80.);
      const auto out = Run(p, 3., 48000., sine(220.));
      return Goertzel(out.left, 48000, 144000, hz, 48000.) / Goertzel(out.left, 48000, 144000, 220., 48000.);
    };
    assert(energy(Mode::Shimmer, 440.) > .1);
    assert(energy(Mode::Tunnel, 110.) > .3);
    assert(energy(Mode::Arp, 220. * std::exp2(7. / 12.)) > .05);
    assert(energy(Mode::Cloud, 440.) < .01 && energy(Mode::Cloud, 110.) < .01);
  }
  // Reverse plays grains backwards: on a slow rising ramp the output mostly falls, where
  // Blocks (forward) mostly rises. Shape 50% keeps the window's own slope symmetric.
  {
    const auto falling = [&](Mode mode) {
      auto p = Wet(mode); p.Set(uGranulatorActivity, 0.).Set(uGranulatorSize, 400.).Set(uGranulatorShape, 50.);
      const auto out = Run(p, 4., 48000., [](int i, int) { return std::fmod(i / 48000., 2.) * .25; });
      int down = 0, total = 0;
      for (size_t i = 48000; i + 1 < out.left.size(); ++i)
        if (std::abs(out.left[i]) > .05) { ++total; down += out.left[i + 1] < out.left[i]; }
      return static_cast<double>(down) / std::max(1, total);
    };
    assert(falling(Mode::Reverse) > .6);
    assert(falling(Mode::Blocks) < .4);
  }
  // Filter: left of center darkens the grains, right of center thins them.
  {
    const auto level = [&](double filter, double hz) {
      auto p = Wet(Mode::Cloud); p.Set(uGranulatorPosition, 0.).Set(uGranulatorActivity, 80.).Set(uGranulatorFilter, filter);
      return Run(p, 2., 48000., sine(hz)).Rms(1., 2., 48000.);
    };
    assert(level(-100., 5000.) < .02 * level(0., 5000.));
    assert(level(100., 110.) < .02 * level(0., 110.));
    assert(level(0., 1000.) > .7 * level(0., 110.));
  }
  // Freeze keeps making grains from the held buffer after the input stops; releasing it
  // records the silence, and the grains die away once the buffer has refilled.
  {
    auto p = Wet(Mode::Cloud);
    const int stop = 48000 * 2, release = 48000 * 8;
    const auto out = Run(p, 15., 48000., [&](int i, int ch) { return i < stop ? Note(i, 48000., ch) : 0.; }, true,
                         [&](int i) { return i >= stop - 4800 && i < release; });
    assert(out.Rms(6., 8., 48000.) > .3 * out.Rms(1., 2., 48000.));
    assert(out.Rms(13.5, 15., 48000.) < 1.e-9);
  }
  // A bypassed slot drains with Freeze still on: the frozen buffer is overwritten, so
  // nothing old returns when the slot plays again.
  {
    auto p = Wet(Mode::Haze); p.Set(uGranulatorRepeats, 100.).Set(uGranulatorSpace, 100.);
    const int stop = 48000 * 2, enable = 48000 * 18;
    const auto out = Run(p, 20., 48000., [&](int i, int ch) { return i < stop ? Note(i, 48000., ch) : 0.; }, true,
                         [&](int i) { return i >= stop - 4800; }, [&](int i) { return i >= stop && i < enable; });
    assert(out.Rms(1.8, 2., 48000.) > .001);
    assert(out.Rms(18., 20., 48000.) < 1.e-6);
  }
  // Full Repeats and Space on loud input stay bounded and die away without input, in every mode.
  for (int m = 0; m < effect_modes::granulatorCount; ++m)
  {
    auto p = Wet(static_cast<Mode>(m)); p.Set(uGranulatorPosition, 5.).Set(uGranulatorActivity, 100.)
                                          .Set(uGranulatorRepeats, 100.).Set(uGranulatorSpace, 100.);
    grid_granular::Random noise;
    // Give the pick-triggered processor a second attack once there is audio at Position.
    const auto out = Run(p, 40., 48000., [&](int i, int) {
      const bool gap = p.mode == Mode::PickBurst && i >= 40000 && i < 52000;
      return i < 48000 * 2 && !gap ? 4. * (noise.Uniform() - .5) : 0.;
    });
    const bool ok = std::isfinite(out.Peak()) && out.Peak() < 4. && out.Rms(35., 40., 48000.) < .1 * out.Rms(2., 4., 48000.);
    if (!ok)
      std::cerr << "Granulator repeats: " << effect_modes::granulatorNames[m] << " peak " << out.Peak() << " tail "
                << out.Rms(35., 40., 48000.) / out.Rms(2., 4., 48000.) << '\n';
    assert(ok);
  }
  // Settled rigs render identically from the same input; the mono path stays finite and level.
  for (int m = 0; m < effect_modes::granulatorCount; ++m)
  {
    Patch p; p.mode = static_cast<Mode>(m);
    const auto a = Run(p, 2., 48000., note(48000.)), b = Run(p, 2., 48000., note(48000.));
    assert(a.left == b.left && a.right == b.right);
    const auto mono = Run(p, 2., 48000., [](int i, int) { return Note(i, 48000.); }, false);
    assert(std::isfinite(mono.Peak()) && mono.Peak() < 2.);
  }
  // The display's sketch uses the processor's scheduler and choices: even modes space
  // grains evenly, Arp steps through its fifths and octaves, Reverse reverses every grain.
  {
    grid_granular::Settings blocks; blocks.mode = static_cast<int>(Mode::Blocks); blocks.activity = 1.;
    const auto even = grid_granular::PreviewCloud(blocks, 2.);
    assert(even.size() > 10);
    for (size_t i = 2; i < even.size(); ++i)
      assert(std::abs((even[i].start - even[i - 1].start) - (even[1].start - even[0].start)) < 1.e-9);
    grid_granular::Settings arp = blocks; arp.mode = static_cast<int>(Mode::Arp);
    const auto steps = grid_granular::PreviewCloud(arp, 2.);
    constexpr double sequence[]{0., 7., 12., 19., 24., 19., 12., 7.};
    arp.periodSeconds = 8.;
    const auto phrase = grid_granular::PreviewPhrase(arp);
    assert(phrase.size() == std::size(sequence));
    for (size_t i = 0; i < phrase.size(); ++i) assert(phrase[i].pitch == sequence[i] && phrase[i].start == 8. * i);
    assert(steps.size() > 8);
    for (size_t i = 0; i < steps.size(); ++i) assert(steps[i].pitch == sequence[i % 8]);
    grid_granular::Settings reverse; reverse.mode = static_cast<int>(Mode::Reverse);
    for (const auto& grain : grid_granular::PreviewCloud(reverse, 2.)) assert(grain.reverse);
  }
}
