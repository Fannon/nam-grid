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

inline Audio Render(E effect, UtilityControls controls, double rate, int block, const Audio& source,
                    void (*automate)(UtilityControls&, int) = nullptr)
{
  SlotExpandedDelay delay;
  delay.Reset(rate, block, effect);
  auto result = source;
  const int before = allocations, destroyed = deletions;
  tracking = true;
  for (int first = 0, frames = static_cast<int>(source[0].size()); first < frames; first += block)
  {
    if (automate) automate(controls, first);
    delay.Process(result[0].data() + first, result[1].data() + first, std::min(block, frames - first), controls, false);
  }
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
  {
    constexpr double rate = 4000.;
    expanded_delay::TapeTransport transport;
    for (const double start : {300., 600.})
    {
      const double end = 900. - start;
      transport.Reset(rate);
      transport.Tick(start, 0.);
      const int frames = static_cast<int>((end > start ? 1.89 : 1.07) * rate);
      for (int i = 0; i < frames; ++i) transport.Tick(end, 0.);
      assert(std::abs((transport.TimeMs() - end) / (start - end) - std::exp(-1.)) < .0001);
      assert(transport.Tap() == transport.TimeMs() * .001 * rate);
    }
    // At 420 ms the periodic components complete whole cycles in 100 seconds.
    // An additional displacement here comes from the slow transport drift.
    transport.Reset(rate);
    transport.Tick(420., 1.);
    const double first = transport.Tap();
    for (int i = 0; i < 100 * rate; ++i) transport.Tick(420., 1.);
    assert(std::abs(transport.Tap() - first) > .0001 * rate);
  }
  {
    constexpr int rate = 48000;
    Audio tone{std::vector<double>(120000), std::vector<double>(120000)};
    for (int i = 0; i < 30000; ++i)
      tone[0][i] = tone[1][i] = .25 * std::sin(2. * grid_fx::pi * 111. * i / rate);
    UtilityControls c;
    c.delay = {50., 0., 12000., 0., 100.};
    Option(c, uEchoShiftBlend, 100.);
    Option(c, uEchoInterval, 0.);
    const auto automated = Render(E::PitchDelay, c, rate, 64, tone, [](UtilityControls& controls, int frame) {
      if (frame == 24000) Option(controls, uEchoInterval, 1.);
    });
    for (int i = 24000; i < 25000; ++i)
      assert(std::abs(automated[0][i] - automated[0][i - 1]) < .015);

    Option(c, uEchoInterval, 12.);
    const auto resumed = Render(E::PitchDelay, c, rate, 64, tone, [](UtilityControls& controls, int frame) {
      if (frame == 24000) Option(controls, uPitchDelayMode, 1.);
      if (frame == 96000) Option(controls, uPitchDelayMode, 0.);
    });
    for (const auto& channel : resumed) assert(Peak(channel, rate, 1.9, 2.25) < 1.e-9);

    Option(c, uPitchDelayMode, 1.);
    const auto reverse = Render(E::PitchDelay, c, rate, 64, tone);
    Option(c, uEchoDetune, 25.);
    const auto detuned = Render(E::PitchDelay, c, rate, 64, tone);
    double difference = 0., stereo = 0.;
    for (int i = 4800; i < 24000; ++i)
    {
      difference = std::max(difference, std::abs(detuned[0][i] - reverse[0][i]));
      stereo = std::max(stereo, std::abs(detuned[0][i] - detuned[1][i]));
    }
    assert(difference > .01 && stereo > .01);
  }
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
        if (effect != E::SpaceTapeEcho) assert(Peak(a[0], rate, 1.05, 1.24) < .15); // Space Tape runs away by design.
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
  {
    // Spring only plays no tape heads: silent without the spring, and no echo at the heads with it.
    const double rate = 48000.;
    const int count = static_cast<int>(rate * .6);
    Audio impulse{std::vector<double>(count), std::vector<double>(count)};
    impulse[0][0] = impulse[1][0] = .25;
    UtilityControls c;
    c.delay = {300., 60., 10000., 0., 100.};
    c.delayMotion = 0.;
    Option(c, uSpaceHeads, 7.);
    Option(c, uEchoSpring, 0.);
    const auto silent = Render(E::SpaceTapeEcho, c, rate, 64, impulse);
    assert(Peak(silent[0], rate, 0., .6) == 0. && Peak(silent[1], rate, 0., .6) == 0.);
    Option(c, uEchoSpring, 100.);
    assert(Peak(Render(E::SpaceTapeEcho, c, rate, 64, impulse)[0], rate, .047, .099) > .005);
    // The longest grains, reversed and an octave up at a long Time, stay inside the
    // history and bounded, independently of callback partitioning.
    UtilityControls grains;
    grains.delay = {4000., 60., 9000., 0., 100.};
    for (const auto& [p, v] : {std::pair{uGrainLength, 1000.}, {uGrainDensity, 0.}, {uGrainScatter, 100.}, {uEchoInterval, 12.}})
      Option(grains, p, v);
    const int longCount = static_cast<int>(rate * 9.);
    Audio phrase{std::vector<double>(longCount), std::vector<double>(longCount)};
    for (int i = 0; i < static_cast<int>(rate * .5); ++i)
      phrase[0][i] = phrase[1][i] = .3 * std::sin(2. * grid_fx::pi * 330. * i / rate);
    for (const int mode : {1, 2, 3})
    {
      Option(grains, uGrainDelayMode, mode);
      const auto a = Render(E::GranularDelay, grains, rate, 61, phrase), b = Render(E::GranularDelay, grains, rate, 512, phrase);
      assert(a == b && Peak(a[0], rate, 3., 6.) > .001);
      for (const auto& channel : a) for (const double sample : channel) assert(std::isfinite(sample) && std::abs(sample) < 1.2);
    }
    // Granular Delay Shimmer: every repeat carries an octave layer that is shifted
    // again on each pass, so a 220 Hz note climbs to 880 Hz within a few repeats.
    // Scatter at Interval 0 keeps the note's pitch.
    {
      UtilityControls shimmer;
      shimmer.delay = {420., 60., 7000., 0., 100.};
      Option(shimmer, uEchoInterval, 0.); // The shared parameter starts at Pitch Delay's +12.
      // Scattered starts smear the phase that a narrow-band probe relies on.
      Option(shimmer, uGrainScatter, 0.);
      const int count = static_cast<int>(rate * 3.);
      Audio note{std::vector<double>(count), std::vector<double>(count)};
      for (int i = 0; i < static_cast<int>(rate * .4); ++i)
        note[0][i] = note[1][i] = .25 * std::sin(2. * grid_fx::pi * 220. * i / rate);
      // Band levels of the first two repeats (0.4-1.3 s) relative to the note's pitch.
      const auto climb = [&](int mode, double hz) {
        Option(shimmer, uGrainDelayMode, mode);
        const auto out = Render(E::GranularDelay, shimmer, rate, 128, note);
        return Band(out[0], rate, .4, 1.3, hz) / Band(out[0], rate, .4, 1.3, 220.);
      };
      assert(climb(3, 440.) > .4 && climb(3, 880.) > .1);
      assert(climb(0, 440.) < .05 && climb(0, 880.) < .05);
    }
    // Space Tape Echo: at 82% Feedback the repeats sustain (self-oscillation) yet stay
    // bounded by the tape; at 60% they still die away.
    {
      UtilityControls tape;
      tape.delay = {200., 82., 6000., 20., 100.};
      tape.delayMotion = 0.;
      Option(tape, uSpaceHeads, 2.);
      Option(tape, uEchoSpring, 0.);
      const int seconds = static_cast<int>(rate * 4.);
      Audio pluck{std::vector<double>(seconds), std::vector<double>(seconds)};
      for (int i = 0; i < static_cast<int>(rate * .03); ++i)
        pluck[0][i] = pluck[1][i] = .3 * std::sin(2. * grid_fx::pi * 440. * i / rate);
      const auto runaway = Render(E::SpaceTapeEcho, tape, rate, 64, pluck);
      assert(Peak(runaway[0], rate, 3.4, 4.) > .05 && Peak(runaway[0], rate, 0., 4.) < .9);
      tape.delay[1] = 60.;
      assert(Peak(Render(E::SpaceTapeEcho, tape, rate, 64, pluck)[0], rate, 3.4, 4.) < .01);
    }
    // A stereo quadrature tone encodes read direction. A slowing tape must
    // keep advancing even across a jump to several seconds of delay.
    {
      UtilityControls tape;
      tape.delay = {140., 0., 12000., 0., 100.};
      tape.delayMotion = 100.;
      Option(tape, uSpaceHeads, 2.);
      Option(tape, uEchoSpring, 0.);
      const int frames = static_cast<int>(rate), jump = frames / 2;
      Audio tone{std::vector<double>(frames), std::vector<double>(frames)};
      for (int i = 0; i < frames; ++i)
      {
        const double phase = 2. * grid_fx::pi * 440. * i / rate;
        tone[0][i] = .05 * std::cos(phase);
        tone[1][i] = .05 * std::sin(phase);
      }
      SlotExpandedDelay motor;
      motor.Reset(rate, 64, E::SpaceTapeEcho);
      for (int first = 0; first < frames; first += 64)
      {
        if (first >= jump) tape.delay[0] = 4000.;
        motor.Process(tone[0].data()+first, tone[1].data()+first, std::min(64, frames-first), tape, false);
      }
      for (int i = jump + 128; i < frames; ++i)
        assert(tone[0][i-1] * tone[1][i] - tone[1][i-1] * tone[0][i] > 0.);
    }
    // The transport follows a Time jump like a motor: well after the old 20 ms glide, a steady
    // tone still comes back pitch-shifted. Bass also drops on the first repeat.
    {
      UtilityControls tape;
      tape.delay = {300., 0., 12000., 0., 100.};
      tape.delayMotion = 0.;
      Option(tape, uSpaceHeads, 2.);
      Option(tape, uEchoSpring, 0.);
      SlotExpandedDelay delay;
      delay.Reset(rate, 64, E::SpaceTapeEcho);
      const int frames = static_cast<int>(rate * 1.6), jump = static_cast<int>(rate * 1.);
      Audio tone{std::vector<double>(frames), std::vector<double>(frames)};
      for (int i = 0; i < frames; ++i) tone[0][i] = tone[1][i] = .1 * std::sin(2. * grid_fx::pi * 1000. * i / rate);
      for (int first = 0; first < frames; first += 64)
      {
        if (first >= jump) tape.delay[0] = 600.;
        delay.Process(tone[0].data() + first, tone[1].data() + first, std::min(64, frames - first), tape, false);
      }
      const double steady = Band(tone[0], rate, .6, .9, 1000.), gliding = Band(tone[0], rate, 1.15, 1.45, 1000.);
      assert(gliding < .3 * steady);
      const auto echo = [&](double frequency) {
        Audio note{std::vector<double>(count), std::vector<double>(count)};
        for (int i = 0; i < static_cast<int>(rate * .2); ++i)
          note[0][i] = note[1][i] = .1 * std::sin(2. * grid_fx::pi * frequency * i / rate);
        UtilityControls quiet = tape;
        quiet.delay[0] = 300.;
        return Peak(Render(E::SpaceTapeEcho, quiet, rate, 64, note)[0], rate, .4, .5) / .1;
      };
      assert(echo(60.) < .7 && echo(1000.) > .85);
    }
    // Tape saturation never folds back: a louder note never records a quieter echo.
    Option(c, uSpaceHeads, 0.);
    Option(c, uEchoSpring, 0.);
    c.delay = {300., 0., 12000., 100., 100.};
    double previous = 0.;
    for (const double level : {.1, .2, .3, .5, .7, .9})
    {
      Audio note{std::vector<double>(count), std::vector<double>(count)};
      for (int i = 0; i < static_cast<int>(rate * .05); ++i)
        note[0][i] = note[1][i] = level * std::sin(2. * grid_fx::pi * 220. * i / rate);
      const double echo = Peak(Render(E::SpaceTapeEcho, c, rate, 64, note)[0], rate, .1, .2); // Head 1 at 100 ms.
      assert(echo >= previous - 1.e-4);
      previous = echo;
    }
  }
  std::cout << "Expanded delays: tap timing, parallel spring, spring parking, Spring only, monotonic tape saturation, octave regeneration, bounded tails and partition identity passed\n";
}
