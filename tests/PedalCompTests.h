#pragma once
#include "SlotPedalComp.h"
#include "ModulePresets.h"

namespace pedal_comp_test
{
struct Result
{
  std::vector<double> left, right, reduction; // reduction: one value per block
};
inline UtilityControls Controls(double sustain, bool slow, int pickup, double tone = 0., double blend = 100.,
                                double level = 0.)
{
  UtilityControls c;
  c.pedalComp = {sustain, slow ? 1. : 0., tone, blend, level};
  c.pedalCompPickup = pickup;
  return c;
}
inline Result Run(double rate, const UtilityControls& c, std::vector<double> left, std::vector<double> right = {},
                  int block = 48)
{
  SlotPedalComp comp;
  comp.Reset(rate);
  Result result;
  const bool stereo = !right.empty();
  for (size_t position = 0; position < left.size();)
  {
    const int n = static_cast<int>(std::min<size_t>(block, left.size() - position));
    allocations = deletions = 0;
    tracking = true;
    comp.Process(left.data() + position, stereo ? right.data() + position : nullptr, n, c);
    tracking = false;
    assert(allocations == 0 && deletions == 0);
    result.reduction.push_back(comp.Reduction());
    position += n;
  }
  result.left = std::move(left);
  result.right = std::move(right);
  return result;
}
inline std::vector<double> Sine(double rate, double hz, double peakDB, double seconds)
{
  std::vector<double> x(static_cast<size_t>(rate * seconds));
  const double amplitude = std::pow(10., peakDB / 20.);
  for (size_t i = 0; i < x.size(); ++i) x[i] = amplitude * std::sin(2. * grid_fx::pi * hz * i / rate);
  return x;
}
inline double PeakDB(const std::vector<double>& x, size_t from, size_t to)
{
  double peak = 0.;
  for (size_t i = from; i < std::min(to, x.size()); ++i) peak = std::max(peak, std::abs(x[i]));
  return 20. * std::log10(std::max(peak, 1.e-12));
}
inline double Tail(const std::vector<double>& x, double rate)
{ return PeakDB(x, x.size() - static_cast<size_t>(.1 * rate), x.size()); }
// The documented curve, with the detector filters' level change at one frequency.
inline double Expected(double hz, double inputDB, double sustain, int pickup)
{
  const auto& voice = grid_pedal_comp::voices[pickup];
  const auto curve = grid_pedal_comp::CurveFor(sustain, voice.knee);
  const double lowCut = hz / std::hypot(hz, voice.detectorLowCut);
  const double highCut = voice.detectorHighCut > 0. ? voice.detectorHighCut / std::hypot(hz, voice.detectorHighCut) : 1.;
  const double detected = inputDB + 20. * std::log10(lowCut * lowCut * highCut) + voice.sensitivity;
  return inputDB + curve.makeup - SlotDynamics::CompressionDB(detected, curve.threshold, curve.ratio, curve.knee);
}
// Settled steady-tone output level for one control setting.
inline double Settled(double rate, double hz, double inputDB, const UtilityControls& c)
{ return Tail(Run(rate, c, Sine(rate, hz, inputDB, 2.5)).left, rate); }
} // namespace pedal_comp_test

void TestPedalComp()
{
  using namespace pedal_comp_test;
  constexpr double rate = 48000.;
  constexpr int singleCoil = 0, humbucker = 1, acoustic = 2;

  // The settled steady-tone response matches the documented curve for each pickup voice.
  for (int pickup : {humbucker, acoustic})
    for (double sustain : {0., 50., 100.})
      for (double input : {-40., -30., -18., -10.})
      {
        const double measured = Settled(rate, 440., input, Controls(sustain, true, pickup));
        assert(std::abs(measured - Expected(440., input, sustain, pickup)) < .15);
        // The display's curve omits the detector filters: within 1 dB at 440 Hz.
        assert(std::abs(measured - grid_pedal_comp::OutputDB(input, sustain, pickup)) < 1.1);
      }
  // Sustain at noon keeps a typical playing level at bypass loudness while compressing about
  // 6 dB; quiet tails come up and loud peaks go down.
  {
    const auto noon = Controls(50., true, humbucker);
    assert(std::abs(Settled(rate, 440., grid_pedal_comp::nominal, noon) - grid_pedal_comp::nominal) < .5);
    const auto run = Run(rate, noon, Sine(rate, 440., grid_pedal_comp::nominal, 2.5));
    assert(run.reduction.back() > 5. && run.reduction.back() < 8.);
    const double quietGain = Settled(rate, 440., -40., noon) + 40.;
    const double loudGain = Settled(rate, 440., -6., noon) + 6.;
    assert(quietGain > 5. && loudGain < -5.);
    // More Sustain evens out more: the spread between quiet and loud notes shrinks.
    const auto spread = [&](double sustain) {
      const auto c = Controls(sustain, true, humbucker);
      return Settled(rate, 440., -6., c) - Settled(rate, 440., -36., c);
    };
    assert(spread(0.) > 25. && spread(50.) < spread(0.) - 8. && spread(100.) < spread(50.) - 5.);
  }
  // Single coil is about 5 dB more sensitive than Humbucker.
  {
    const auto reduction = [&](int pickup) {
      return Run(rate, Controls(50., true, pickup), Sine(rate, 440., -18., 2.5)).reduction.back();
    };
    const double difference = reduction(singleCoil) - reduction(humbucker);
    assert(difference > 2.5 && difference < 5.);
  }
  // Acoustic ignores body boom: an 80 Hz tone barely drives its detector, a 1 kHz tone does.
  {
    const auto reduction = [&](int pickup, double hz) {
      return Run(rate, Controls(60., true, pickup), Sine(rate, hz, -12., 2.5)).reduction.back();
    };
    assert(reduction(acoustic, 80.) < reduction(singleCoil, 80.) - 3.);
    assert(std::abs(reduction(acoustic, 1000.) - reduction(singleCoil, 1000.)) < 1.5);
  }
  // Fast clamps a pick within a few milliseconds; Slow lets the attack through first.
  {
    const auto overshoot = [&](bool slow, double fromMS, double toMS) {
      auto input = Sine(rate, 1000., -40., 1.);
      const auto loud = Sine(rate, 1000., -6., 1.);
      input.insert(input.end(), loud.begin(), loud.end());
      const auto out = Run(rate, Controls(70., slow, humbucker), input).left;
      const size_t step = static_cast<size_t>(rate);
      return PeakDB(out, step + static_cast<size_t>(fromMS * rate * .001), step + static_cast<size_t>(toMS * rate * .001))
             - Tail(out, rate);
    };
    assert(overshoot(false, 8., 12.) < 1.5);
    assert(overshoot(true, 4., 8.) > 4.);
    assert(overshoot(true, 4., 8.) > overshoot(false, 4., 8.) + 3.);
  }
  // Program-dependent release: after a short peak the reduction lets go quickly, after a
  // long held note it eases back more slowly.
  {
    const auto after = [&](double burstSeconds) {
      auto input = Sine(rate, 440., -6., burstSeconds);
      const auto quiet = Sine(rate, 440., -60., 1.);
      input.insert(input.end(), quiet.begin(), quiet.end());
      const auto run = Run(rate, Controls(70., false, humbucker), input);
      const size_t blocks = static_cast<size_t>((burstSeconds + .3) * rate / 48.);
      return run.reduction[blocks];
    };
    const double shortPeak = after(.04), heldNote = after(2.);
    assert(shortPeak < 3. && heldNote > shortPeak + 6.);
  }
  // Release times scale with the host rate.
  {
    const auto halfLife = [&](double r) {
      auto input = Sine(r, 440., -6., 2.);
      const auto quiet = Sine(r, 440., -60., 2.);
      input.insert(input.end(), quiet.begin(), quiet.end());
      const auto run = Run(r, Controls(70., false, humbucker), input, {}, 1);
      const size_t drop = static_cast<size_t>(2. * r);
      size_t i = drop;
      while (i < run.reduction.size() && run.reduction[i] > .5 * run.reduction[drop - 1]) ++i;
      return (i - drop) / r;
    };
    const double reference = halfLife(44100.);
    for (double r : {48000., 96000., 192000.}) assert(std::abs(halfLife(r) / reference - 1.) < .05);
  }
  // Blend 0% with Tone and Level at zero is exactly dry.
  {
    const auto input = Sine(rate, 440., -6., .5);
    const auto run = Run(rate, Controls(100., false, singleCoil, 0., 0.), input);
    assert(run.left == input);
  }
  // Tone is a treble shelf on the whole output: +6 dB at 10 kHz, none at 100 Hz.
  {
    const auto toneGain = [&](double hz) {
      const auto c = Controls(0., true, humbucker, 6.);
      return Settled(rate, hz, -40., c) + 40.;
    };
    assert(std::abs(toneGain(10000.) - 6.) < .7 && std::abs(toneGain(100.)) < .3);
  }
  // A host rate change recomputes the Tone shelf: a reused processor equals a fresh one.
  {
    const auto c = Controls(0., true, humbucker, 6.);
    SlotPedalComp reused;
    reused.Reset(48000.);
    auto warm = Sine(48000., 2000., -40., .2);
    reused.Process(warm.data(), nullptr, static_cast<int>(warm.size()), c);
    reused.Reset(96000.);
    auto input = Sine(96000., 2000., -40., .5);
    auto fresh = input;
    reused.Process(input.data(), nullptr, static_cast<int>(input.size()), c);
    SlotPedalComp reference;
    reference.Reset(96000.);
    reference.Process(fresh.data(), nullptr, static_cast<int>(fresh.size()), c);
    assert(input == fresh);
  }
  // One linked detector: both channels receive the same gain, so the image stays put.
  {
    const auto loud = Sine(rate, 440., -6., 1.);
    auto quiet = loud;
    for (auto& x : quiet) x *= .1;
    const auto run = Run(rate, Controls(80., false, humbucker), loud, quiet);
    for (size_t i = 0; i < loud.size(); ++i)
      assert(std::abs(run.right[i] - .1 * run.left[i]) <= 1.e-12 * std::max(1., std::abs(run.left[i])));
    // A mono slot equals a stereo slot with identical channels.
    const auto mono = Run(rate, Controls(80., false, humbucker, 2.), loud);
    const auto twin = Run(rate, Controls(80., false, humbucker, 2.), loud, loud);
    assert(mono.left == twin.left && twin.left == twin.right);
  }
  // Block-size independent, including single frames and odd sizes.
  {
    std::vector<double> input(24000);
    for (size_t i = 0; i < input.size(); ++i)
      input[i] = (i < 12000 ? .5 : .02) * std::sin(.031 * i) + .05 * std::sin(.0021 * i);
    const auto c = Controls(65., false, acoustic, 3., 70., -2.);
    const auto reference = Run(rate, c, input, {}, 512).left;
    for (int block : {1, 3, 64, 127}) assert(Run(rate, c, input, {}, block).left == reference);
  }
  // Extreme input and every setting stay finite.
  for (int pickup : {singleCoil, humbucker, acoustic})
    for (bool slow : {false, true})
    {
      std::vector<double> input(4800);
      for (size_t i = 0; i < input.size(); ++i) input[i] = (i % 2 ? 1000. : -1000.) * (i < 2400 ? 1. : 1.e-20);
      const auto run = Run(rate, Controls(100., slow, pickup, 6., 50., 12.), input);
      for (double x : run.left) assert(std::isfinite(x));
    }
  // Changing the pickup or the attack while playing is click-free: the gain moves smoothly.
  {
    SlotPedalComp comp;
    comp.Reset(rate);
    auto c = Controls(80., false, humbucker);
    auto audio = Sine(rate, 440., -12., 2.);
    double previousGain = 0.;
    for (size_t position = 0; position < audio.size(); position += 48)
    {
      if (position == audio.size() / 2) { c.pedalCompPickup = singleCoil; c.pedalComp[1] = 1.; }
      std::array<double, 48> dry{};
      std::copy_n(audio.begin() + position, 48, dry.begin());
      comp.Process(audio.data() + position, nullptr, 48, c);
      for (int i = 0; i < 48; ++i)
        if (std::abs(dry[i]) > .1)
        {
          const double gain = audio[position + i] / dry[i];
          // Checked once the first attack has settled.
          if (previousGain > 0. && position > rate / 2) assert(std::abs(20. * std::log10(gain / previousGain)) < .05);
          previousGain = gain;
        }
    }
  }
}

void TestModulePresets()
{
  using namespace module_presets;
  for (int e = 0; e <= static_cast<int>(SlotEffect::Last); ++e)
  {
    const auto effect = static_cast<SlotEffect>(e);
    const auto presets = For(effect);
    auto current = SlotDefaultValues(effect);
    current[parameter_layout::kSlotEnabled] = 0.; // Presets never change bypass.
    current[parameter_layout::kSlotIn] = 7.;      // or controls the module does not use.
    for (size_t index = 0; index < presets.size(); ++index)
    {
      const auto& preset = presets[index];
      for (size_t other = 0; other < index; ++other) assert(std::string_view(preset.name) != presets[other].name);
      for (const auto& value : preset.values)
      {
        assert(ModuleControl(effect, value.control));
        const int p = value.control - parameter_layout::standardControls;
        if (p >= 0)
          assert(value.value >= kEffectParameters[p].minimum && value.value <= kEffectParameters[p].maximum);
      }
      const auto applied = Apply(effect, preset, current);
      assert(applied[parameter_layout::kSlotEnabled] == 0. && applied[parameter_layout::kSlotIn] == 7.);
      // Every preset is recognised as itself, and no two presets are the same sound.
      assert(Matching(effect, applied) == static_cast<int>(index));
      // A preset is a complete starting point: earlier edits never leak through.
      auto edited = current;
      for (int i = 0; i < static_cast<int>(edited.size()); ++i)
        if (ModuleControl(effect, i) && !KeepsSetup(effect, i)) edited[i] = kEffectParameters[std::max(0, i - parameter_layout::standardControls)].maximum;
      assert(Apply(effect, preset, edited) == applied);
    }
  }
  // Pedal Comp keeps the player's pickup unless the preset is for an instrument.
  auto current = SlotDefaultValues(SlotEffect::PedalComp);
  current[U(uPedalCompPickup)] = 1.;
  for (const auto& preset : pedalComp)
  {
    const bool instrument = std::any_of(preset.values.begin(), preset.values.end(),
                                        [](const Value& v) { return v.control == U(uPedalCompPickup); });
    assert(Apply(SlotEffect::PedalComp, preset, current)[U(uPedalCompPickup)] == (instrument ? 2. : 1.));
  }
  // The Pedal Comp, Smooth and Punch compressors and Digital Delay have presets; a moved knob
  // stops matching.
  for (auto effect : {SlotEffect::PedalComp, SlotEffect::SmoothComp, SlotEffect::PunchComp, SlotEffect::DigitalDelay})
  {
    assert(For(effect).size() >= 3);
    auto applied = Apply(effect, For(effect)[0], SlotDefaultValues(effect));
    for (int i = 0; i < static_cast<int>(applied.size()); ++i)
      if (ModuleControl(effect, i) && i >= parameter_layout::standardControls
          && EffectChoices(i - parameter_layout::standardControls).empty())
      {
        applied[i] += .5 * kEffectParameters[i - parameter_layout::standardControls].step;
        break;
      }
    assert(Matching(effect, applied) == -1);
  }
}
