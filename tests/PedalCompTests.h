#pragma once
#include "SlotPedalComp.h"
#include "ModulePresets.h"

namespace pedal_comp_test
{
struct Result
{
  std::vector<double> left, right, reduction; // reduction: one value per block
};
inline UtilityControls Controls(double sustain, bool slow, int pickup, double focus = 0., double blend = 100.,
                                double level = 0.)
{
  UtilityControls c;
  c.pedalComp = {sustain, slow ? 1. : 0., focus, blend, level};
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
  // Sustain at noon: a steady tone at the reference level is compressed by about 8 dB and,
  // with makeup read 2.5 dB higher for played notes, ends up about 2 dB above bypass. Quiet
  // tails come up and loud peaks go down.
  {
    const auto noon = Controls(50., true, humbucker);
    const double reference = Settled(rate, 440., grid_pedal_comp::nominal, noon) - grid_pedal_comp::nominal;
    assert(reference > 1. && reference < 3.);
    const auto run = Run(rate, noon, Sine(rate, 440., grid_pedal_comp::nominal, 2.5));
    assert(run.reduction.back() > 6.5 && run.reduction.back() < 9.5);
    const double quietGain = Settled(rate, 440., -40., noon) + 40.;
    const double loudGain = Settled(rate, 440., -6., noon) + 6.;
    assert(quietGain > 8. && loudGain < -5.);
    // More Sustain evens out more: the spread between quiet and loud notes shrinks.
    const auto spread = [&](double sustain) {
      const auto c = Controls(sustain, true, humbucker);
      return Settled(rate, 440., -6., c) - Settled(rate, 440., -36., c);
    };
    assert(spread(0.) > 24. && spread(50.) < spread(0.) - 8. && spread(100.) < spread(50.) - 4.);
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
      const size_t blocks = static_cast<size_t>((burstSeconds + .6) * rate / 48.);
      return run.reduction[blocks];
    };
    const double shortPeak = after(.04), heldNote = after(2.);
    assert(shortPeak < 1.5 && heldNote > shortPeak + 2.5);
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
  // Blend 0% with Level at zero is exactly dry, whatever Body / Pick says.
  {
    const auto input = Sine(rate, 440., -6., .5);
    const auto run = Run(rate, Controls(100., false, singleCoil, 100., 0.), input);
    assert(run.left == input);
  }
  // Body / Pick changes what is compressed, never the tone: Pick compresses a bright tone more
  // than a low one, Body the reverse, and the middle treats them about equally.
  {
    const auto reduction = [&](double focus, double hz) {
      return Run(rate, Controls(60., true, humbucker, focus), Sine(rate, hz, -12., 2.5)).reduction.back();
    };
    assert(reduction(100., 3000.) > reduction(100., 200.) + 3.);
    assert(reduction(-100., 200.) > reduction(-100., 3000.) + 3.);
    assert(std::abs(reduction(0., 3000.) - reduction(0., 200.)) < 1.5);
    // Below the threshold both extremes give identical output with one gain for every
    // frequency: no EQ on the audio.
    double gain = 0.;
    for (double hz : {100., 3000., 10000.})
    {
      const auto input = Sine(rate, hz, -50., .3);
      const auto body = Run(rate, Controls(0., true, humbucker, -100.), input).left;
      assert(Run(rate, Controls(0., true, humbucker, 100.), input).left == body);
      const double measured = Tail(body, rate) + 50.;
      if (gain == 0.) gain = measured;
      assert(std::abs(measured - gain) < .01);
    }
  }
  // A host rate change recomputes every rate-dependent coefficient: a reused processor equals a fresh one.
  {
    const auto c = Controls(60., true, humbucker, 100.);
    SlotPedalComp reused;
    reused.Reset(48000.);
    auto warm = Sine(48000., 2000., -12., .2);
    reused.Process(warm.data(), nullptr, static_cast<int>(warm.size()), c);
    reused.Reset(96000.);
    auto input = Sine(96000., 2000., -12., .5);
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
    const auto mono = Run(rate, Controls(80., false, humbucker, 40.), loud);
    const auto twin = Run(rate, Controls(80., false, humbucker, 40.), loud, loud);
    assert(mono.left == twin.left && twin.left == twin.right);
  }
  // Block-size independent, including single frames and odd sizes.
  {
    std::vector<double> input(24000);
    for (size_t i = 0; i < input.size(); ++i)
      input[i] = (i < 12000 ? .5 : .02) * std::sin(.031 * i) + .05 * std::sin(.0021 * i);
    const auto c = Controls(65., false, acoustic, -50., 70., -2.);
    const auto reference = Run(rate, c, input, {}, 512).left;
    for (int block : {1, 3, 64, 127}) assert(Run(rate, c, input, {}, block).left == reference);
  }
  // Extreme input and every setting stay finite.
  for (int pickup : {singleCoil, humbucker, acoustic})
    for (bool slow : {false, true})
    {
      std::vector<double> input(4800);
      for (size_t i = 0; i < input.size(); ++i) input[i] = (i % 2 ? 1000. : -1000.) * (i < 2400 ? 1. : 1.e-20);
      const auto run = Run(rate, Controls(100., slow, pickup, 100., 50., 12.), input);
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

void TestUserModulePresets()
{
  using namespace module_presets;
  using json = nlohmann::json;
  // Every built-in module's controls survive the file format, in any slot.
  for (int e = 1; e <= static_cast<int>(SlotEffect::Last); ++e)
  {
    const auto effect = static_cast<SlotEffect>(e);
    if (effect == SlotEffect::CabinetIR) continue;
    auto values = SlotDefaultValues(effect);
    for (int i = 0; i < static_cast<int>(values.size()); ++i)
      if (ModuleControl(effect, i))
      {
        const auto range = RangeOf(i);
        const int p = i - parameter_layout::standardControls;
        const double middle = .5 * (range.minimum + range.maximum);
        values[i] = p >= 0 && kEffectParameters[p].step == 1. ? std::floor(middle) : middle;
      }
    std::vector<Value> parsed;
    assert(Parse(json::parse(ToJson(effect, values).dump()), effect, parsed).empty());
    auto other = SlotDefaultValues(effect);
    other[parameter_layout::kSlotEnabled] = 0.;
    const auto applied = Apply(effect, parsed, other);
    for (int i = 0; i < static_cast<int>(values.size()); ++i)
      assert(applied[i] == (ModuleControl(effect, i) ? values[i] : other[i]));
    assert(Matches(effect, parsed, applied));
  }
  // Invalid files are rejected with a reason; older files gain later controls' defaults.
  const auto base = ToJson(SlotEffect::PedalComp, SlotDefaultValues(SlotEffect::PedalComp));
  const auto rejected = [&](const auto& change) {
    auto document = base;
    change(document);
    std::vector<Value> values;
    return !Parse(document, SlotEffect::PedalComp, values).empty();
  };
  assert(rejected([](json& d) { d["format"] = "nam-grid-preset"; }));
  assert(rejected([](json& d) { d["version"] = kUserVersion + 1; }));
  assert(rejected([](json& d) { d["schema"] = kEffectStateSchema + 1; }));
  assert(rejected([](json& d) { d["schema"] = 1.5; }));
  assert(rejected([](json& d) { d["module"] = static_cast<int>(SlotEffect::SmoothComp); }));
  assert(rejected([](json& d) { d["values"]["Smooth Ratio"] = 2.; }));
  assert(rejected([](json& d) { d["values"]["Pedal Comp Sustain"] = 101.; }));
  assert(rejected([](json& d) { d["values"]["Pedal Comp Sustain"] = "50"; }));
  assert(rejected([](json& d) { d["values"]["Pedal Comp Pickup"] = 1.5; }));
  assert(rejected([](json& d) { d.erase("values"); }));
  auto older = base;
  older["schema"] = kInitialEffectStateSchema;
  older["values"] = json{{"Pedal Comp Sustain", 70.}};
  std::vector<Value> partial;
  assert(Parse(older, SlotEffect::PedalComp, partial).empty() && partial.size() == 1);
  auto current = SlotDefaultValues(SlotEffect::PedalComp);
  current[U(uPedalCompPickup)] = 2.;
  current[U(uPedalCompBlend)] = 10.;
  const auto applied = Apply(SlotEffect::PedalComp, partial, current);
  assert(applied[U(uPedalCompSustain)] == 70. && applied[U(uPedalCompBlend)] == kEffectParameters[uPedalCompBlend].initial);
  assert(applied[U(uPedalCompPickup)] == 2.); // A file without the pickup keeps the player's setup.
  // Files: one folder per module under "Module Presets", listed by name across preset
  // folders. Other modules' files are skipped; unreadable ones are counted.
  const auto root = std::filesystem::temp_directory_path() / ("nam-grid-module-presets-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  std::filesystem::remove_all(root);
  const auto first = root / "Presets", second = root / "More presets";
  auto squash = SlotDefaultValues(SlotEffect::PedalComp);
  squash[U(uPedalCompSustain)] = 80.;
  squash[U(uPedalCompPickup)] = 0.;
  SaveUser(UserFolder(first, SlotEffect::PedalComp) / "b squash.json", SlotEffect::PedalComp, squash);
  SaveUser(UserFolder(second, SlotEffect::PedalComp) / "A light.json", SlotEffect::PedalComp, SlotDefaultValues(SlotEffect::PedalComp));
  SaveUser(UserFolder(first, SlotEffect::SmoothComp) / "Smooth.json", SlotEffect::SmoothComp, SlotDefaultValues(SlotEffect::SmoothComp));
  std::ofstream(UserFolder(first, SlotEffect::PedalComp) / "broken.json") << "{ not json";
  auto newer = ToJson(SlotEffect::PedalComp, squash);
  newer["schema"] = kEffectStateSchema + 1;
  std::ofstream(UserFolder(first, SlotEffect::PedalComp) / "future.json") << newer.dump();
  std::ofstream(UserFolder(first, SlotEffect::PedalComp) / "notes.txt") << "ignored";
  assert(UserFolder(first, SlotEffect::Freeze).filename() == "Freeze - Hold"); // No slash in a folder name.
  int unreadable = -1;
  const auto listed = LoadUser({first, second, root / "missing"}, SlotEffect::PedalComp, &unreadable);
  assert(listed.size() == 2 && listed[0].name == "A light" && listed[1].name == "b squash" && unreadable == 2);
  const auto restored = Apply(SlotEffect::PedalComp, listed[1].values, SlotDefaultValues(SlotEffect::PedalComp));
  assert(restored[U(uPedalCompSustain)] == 80. && restored[U(uPedalCompPickup)] == 0.);
  assert(LoadUser({first}, SlotEffect::SmoothComp).size() == 1 && LoadUser({first}, SlotEffect::Chorus).empty());
  std::filesystem::remove_all(root);
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
        {
          assert(value.value >= kEffectParameters[p].minimum && value.value <= kEffectParameters[p].maximum);
          if (!EffectChoices(p).empty()) assert(value.value == std::floor(value.value));
        }
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
    assert(!For(effect).empty());
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
  TestUserModulePresets();
}
