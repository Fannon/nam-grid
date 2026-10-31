#pragma once
#include "ModulePresets.h"
#include "RoadmapEffects.h"

void TestModulePresetSharedControls()
{
  using namespace parameter_layout;
  using namespace module_presets;
  // Independent list of the shared knobs actually shown by these editors. A
  // round trip based only on ModuleControl would also pass when a knob is missing.
  for (const auto& [effect, controls] : std::initializer_list<std::pair<SlotEffect, std::vector<int>>>{
    {SlotEffect::ToneEQ, {kSlotEQBass, kSlotEQMid, kSlotEQTreble}},
    {SlotEffect::CutEQ, {kSlotEQHighPass, kSlotEQLowPass, kSlotEQPresence}},
    {SlotEffect::ParametricEQ, {kSlotEQHighPass, kSlotEQLowPass}},
    {SlotEffect::Boost, {kSlotEQHighPass}}, {SlotEffect::TubeBoost, {kSlotEQHighPass}}})
  {
    auto edited = SlotDefaultValues(effect);
    for (const int control : controls) edited[control] = RangeOf(control).maximum;
    const auto document = ToJson(effect, edited);
    std::vector<Value> parsed;
    assert(Parse(document, effect, parsed).empty());
    const auto restored = Apply(effect, parsed, SlotDefaultValues(effect));
    for (const int control : controls)
    {
      assert(document["values"].contains(ControlName(control)));
      assert(restored[control] == edited[control]);
    }
    // Resetting a partial preset also resets these knobs, not unrelated state.
    const auto reset = Apply(effect, std::span<const Value>{}, edited);
    for (const int control : controls) assert(reset[control] == SlotDefaultValues(effect)[control]);
  }
}

void TestModulePresetCoverage()
{
  using namespace module_presets;
  for (int id = 0; id <= static_cast<int>(SlotEffect::Last); ++id)
  {
    const auto effect = static_cast<SlotEffect>(id);
    const bool supported = effect != SlotEffect::NAM && effect != SlotEffect::CabinetIR;
    assert(Supported(effect) == supported);
    assert(!supported || !ToJson(effect, SlotDefaultValues(effect))["values"].empty());
    // Deliberate selection policy: gates need input calibration and Utility needs
    // routing/phase context. Others have factory recipes only for established uses
    // that differ from the insertion default (docs/presets.md); keep the menus short.
    const bool calibrated = effect == SlotEffect::NoiseGate || effect == SlotEffect::NoisePrint
                            || effect == SlotEffect::DigitalBlack || effect == SlotEffect::Utility;
    assert(!calibrated || For(effect).empty());
    // Cosmic Echo covers distinct networks as well as practical short/medium/long roles.
    assert(For(effect).size() <= (effect == SlotEffect::CosmicEcho ? 13u : (effect == SlotEffect::PedalDrive || effect == SlotEffect::DigitalDelay) ? 5u : 4u));
    for (const auto& preset : For(effect))
    {
      std::vector<int> seen;
      for (const auto& v : preset.values)
      {
        assert(v.control >= 0 && v.control < static_cast<int>(SlotValues{}.size()));
        assert(std::find(seen.begin(), seen.end(), v.control) == seen.end());
        seen.push_back(v.control);
        const auto range = RangeOf(v.control);
        assert(std::isfinite(v.value) && v.value >= range.minimum && v.value <= range.maximum);
      }
      const auto applied = Apply(effect, preset, SlotDefaultValues(effect));
      // Browsing factory recipes must never trigger a live capture/learn/latch.
      for (int p : {uFreezeHold, uShimmerFreeze, uStutterHold, uNoiseLearn})
        assert(applied[U(p)] == 0.);
      std::vector<Value> saved;
      assert(Parse(ToJson(effect, applied), effect, saved).empty());
      assert(Apply(effect, saved, SlotDefaultValues(effect)) == applied);
    }
  }
  // Factory recipes retain input calibration and split setup unless explicitly
  // overridden. User files still save and restore all three named controls.
  const auto multiband = SlotEffect::Multiband;
  auto calibrated = SlotDefaultValues(multiband);
  const std::array<int, 3> setup{U(uMultibandThreshold), U(uMultibandLowSplit), U(uMultibandHighSplit)};
  calibrated[setup[0]] = -18.; calibrated[setup[1]] = 90.; calibrated[setup[2]] = 4000.;
  for (const auto& preset : For(multiband))
  {
    const auto applied = Apply(multiband, preset, calibrated);
    for (int control : setup)
    {
      double expected = calibrated[control];
      for (const auto& v : preset.values) if (v.control == control) expected = v.value;
      assert(applied[control] == expected);
    }
  }
  const auto savedSetup = ToJson(multiband, calibrated);
  std::vector<Value> restoredSetup;
  assert(Parse(savedSetup, multiband, restoredSetup).empty());
  for (int control : setup) assert(savedSetup["values"].contains(ControlName(control)));
  assert(Apply(multiband, restoredSetup, SlotDefaultValues(multiband)) == calibrated);
  assert(!Supported(static_cast<SlotEffect>(-1)) && !Supported(static_cast<SlotEffect>(999)));
}

void TestFactoryVibeVibrato()
{
  using namespace module_presets;
  for (const auto& preset : For(SlotEffect::UniVibe))
  {
    const auto values = Apply(SlotEffect::UniVibe, preset, SlotDefaultValues(SlotEffect::UniVibe));
    if (values[U(uVibeMode)] < .5) continue;
    UtilityControls c;
    c.vibeMode = values[U(uVibeMode)];
    for (size_t i = 0; i < c.modulation.size(); ++i) c.modulation[i] = values[U(uModRate) + i];
    auto wet = c; wet.modulation[4] = 100.;
    grid_roadmap::Vibe actual, reference;
    actual.Reset(48000.); reference.Reset(48000.);
    // Vibrato must contain only the phase-shifted signal. A hidden 50% Mix
    // would instead reproduce chorus, even though the mode says Vibrato.
    std::array<double, 512> a{}, b{};
    for (int block = 0; block < 60; ++block)
    {
      for (int i = 0; i < 512; ++i) a[i] = b[i] = .2 * std::sin((block * 512 + i) * .071);
      actual.Process(a.data(), 512, c); reference.Process(b.data(), 512, wet);
      for (int i = 0; i < 512; ++i) assert(std::abs(a[i] - b[i]) < 1.e-12);
    }
  }
}

void TestSmartGateUserPreset()
{
  using namespace module_presets;
  auto values = SlotDefaultValues(SlotEffect::NoisePrint);
  // An untrained user preset must explicitly remember the absence of a profile.
  const auto document = ToJson(SlotEffect::NoisePrint, values);
  assert(document.contains("noiseProfile") && document["noiseProfile"].is_null());
  noise_print::Profile learned;
  learned.valid = true;
  for (int i = 0; i < noise_print::bands; ++i) learned.power[i] = 1.e-6 * (i + 1);
  values[U(uNoiseSensitivity)] = 7.7;
  std::vector<Value> parsed;
  std::optional<noise_print::Profile> profile;
  auto learning = values;
  learning[U(uNoiseLearn)] = 1.;
  const auto trained = ToJson(SlotEffect::NoisePrint, learning, learned);
  assert(trained["values"]["Noise Learn"] == 0.);
  assert(Parse(nlohmann::json::parse(trained.dump()), SlotEffect::NoisePrint, parsed, &profile).empty());
  assert(profile && *profile == learned);
  assert(Apply(SlotEffect::NoisePrint, parsed, SlotDefaultValues(SlotEffect::NoisePrint)) == values);
  UserPreset preset{"Learned", {}, parsed, learned};
  assert(MatchesUser(SlotEffect::NoisePrint, preset, values, learned));
  auto anotherProfile = learned;
  anotherProfile.power[0] *= 2.;
  assert(!MatchesUser(SlotEffect::NoisePrint, preset, values, anotherProfile));
  assert(!MatchesUser(SlotEffect::NoisePrint, preset, values, {}));
  preset.noiseProfile = noise_print::Profile{};
  assert(MatchesUser(SlotEffect::NoisePrint, preset, values, {}));
  assert(!MatchesUser(SlotEffect::NoisePrint, preset, values, learned));
  preset.noiseProfile.reset(); // Legacy files match controls regardless of the live profile.
  assert(MatchesUser(SlotEffect::NoisePrint, preset, values, learned));
  auto changedValues = values;
  changedValues[U(uNoiseSensitivity)] = 12.;
  assert(!MatchesUser(SlotEffect::NoisePrint, preset, changedValues, learned));
  assert(Parse(document, SlotEffect::NoisePrint, parsed, &profile).empty());
  assert(profile && !profile->valid); // Explicitly untrained, distinct from legacy omission.
  auto legacy = trained;
  legacy["version"] = 1;
  legacy.erase("noiseProfile");
  legacy["values"]["Noise Learn"] = 1.; // Version 1 could save a live recording toggle.
  assert(Parse(legacy, SlotEffect::NoisePrint, parsed, &profile).empty() && !profile);
  assert(Apply(SlotEffect::NoisePrint, parsed, SlotDefaultValues(SlotEffect::NoisePrint)) == values);
  const auto rejected = [&](const nlohmann::json& data) {
    return !Parse(data, SlotEffect::NoisePrint, parsed, &profile).empty();
  };
  auto invalid = trained;
  invalid.erase("noiseProfile");
  assert(rejected(invalid));
  for (const auto& bad : std::vector<nlohmann::json>{
         42, {{"version", 2}, {"power", learned.power}},
         {{"version", 1}, {"power", {1.e-6}}}})
  {
    invalid = trained;
    invalid["noiseProfile"] = bad;
    assert(rejected(invalid));
  }
  for (double bad : {-1., 0., 1.e7})
  {
    invalid = trained;
    invalid["noiseProfile"]["power"] = std::vector<double>(noise_print::bands, bad);
    assert(rejected(invalid));
  }
  auto other = ToJson(SlotEffect::PedalComp, SlotDefaultValues(SlotEffect::PedalComp), learned);
  assert(!other.contains("noiseProfile"));
  other["noiseProfile"] = trained["noiseProfile"];
  assert(!Parse(other, SlotEffect::PedalComp, parsed).empty());
  assert(For(SlotEffect::NoisePrint).empty()); // Profiles never enter factory recipes.

  const auto root = std::filesystem::current_path() / "smart-gate-user-preset-fixture";
  const auto file = UserFolder(root, SlotEffect::NoisePrint) / "Learned.json";
  SaveUser(file, SlotEffect::NoisePrint, values, learned);
  SaveUser(file.parent_path() / "Untrained.json", SlotEffect::NoisePrint, values);
  rig_preset::WriteAtomically(file.parent_path() / "Legacy.json", legacy.dump());
  rig_preset::WriteAtomically(file.parent_path() / "Invalid.json", invalid.dump());
  int unreadable = 0;
  const auto listed = LoadUser({root}, SlotEffect::NoisePrint, &unreadable);
  assert(listed.size() == 3 && unreadable == 1);
  assert(listed[0].name == "Learned" && listed[0].noiseProfile && *listed[0].noiseProfile == learned);
  assert(listed[1].name == "Legacy" && !listed[1].noiseProfile);
  assert(listed[2].name == "Untrained" && listed[2].noiseProfile && !listed[2].noiseProfile->valid);
  std::filesystem::remove_all(root);
}

void TestNamedUserPreset()
{
  using namespace module_presets;
  const auto root = std::filesystem::current_path() / "named-module-preset-fixture";
  const auto folder = UserFolder(root, SlotEffect::Bitcrusher);
  assert(NamedUserFile(folder, "  My crusher  ") == folder / "My crusher.json");
  assert(NamedUserFile(folder, "My crusher.JSON") == folder / "My crusher.json");
  assert(NamedUserFile(folder, "Warm.v2") == folder / "Warm.v2.json");
  const auto freeze = UserFolder(root, SlotEffect::Freeze);
  assert(NamedUserFile(freeze, "My " + Utf8(freeze.filename())) == freeze / "My Freeze - Hold.json");
  assert(NamedUserFile(folder, "\xc3\x89" "cho \xe6\x9d\xb1\xe4\xba\xac") == folder / std::filesystem::u8path("\xc3\x89" "cho \xe6\x9d\xb1\xe4\xba\xac.json"));
  for (const std::string name : {"", " ", ".", "..", ".json", "../outside", "/outside", "C:\\outside", "a/b", "a\\b",
                                 "a:b", "a*", "a?", "a\"", "a<", "a>", "a|", "a.", "CON", "aux.json", "COM1", "lpt9.txt"})
  {
    bool rejected = false;
    try { NamedUserFile(folder, name); } catch (const std::exception&) { rejected = true; }
    assert(rejected);
  }
  bool rejected = false;
  try { NamedUserFile({}, "My crusher"); } catch (const std::exception&) { rejected = true; }
  assert(rejected);
  const auto file = NamedUserFile(folder, "My crusher");
  const auto values = SlotDefaultValues(SlotEffect::Bitcrusher);
  SaveUser(file, SlotEffect::Bitcrusher, values);
  // The same disk read the menu performs must find the new preset without a library scan.
  const auto presets = LoadUser({root}, SlotEffect::Bitcrusher);
  assert(presets.size() == 1 && presets[0].name == "My crusher" && presets[0].path == file);
  assert(Matches(SlotEffect::Bitcrusher, presets[0].values, values));
  std::filesystem::remove_all(root);
}
