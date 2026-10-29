#pragma once
#include "EffectPresentation.h"
#include "ParameterLayout.h"
#include "SlotDefaults.h"
#include "RigPreset.h"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <initializer_list>
#include <span>
#include <string>
#include <vector>

// Starting points for one module, chosen from the module editor: read-only factory presets
// in code, and user presets as one JSON file each in the preset folders. Unlike rig presets
// they change only that slot's module controls, through ordinary host parameter changes, so
// they never reload the module and never touch the rest of the rig. A project stores the
// resulting control values, not the preset.
namespace module_presets
{
struct Value
{
  int control; // Slot-control index, in SlotValues order.
  double value;
};
struct Preset
{
  const char* name;
  std::initializer_list<Value> values;
};
constexpr int U(int p) { return parameter_layout::standardControls + p; }
using parameter_layout::kSlotCompThreshold;
using parameter_layout::kSlotCompRelease;
using parameter_layout::kSlotCompMakeup;

// Settings follow published pedal-manual starting points and professional plugin factory
// presets, translated to these controls; Pedal Comp's Sustain range is calibrated against a
// Ross-type pedal. Names describe the result. Pickup stays as the player set it unless a
// preset is for a particular instrument.
inline const Preset pedalComp[]{
  {"Light always-on",
   {{U(uPedalCompSustain), 30.}, {U(uPedalCompAttack), 0.}, {U(uPedalCompFocus), 20.}, {U(uPedalCompBlend), 50.}}},
  {"Snappy clean picking",
   {{U(uPedalCompSustain), 60.}, {U(uPedalCompAttack), 0.}, {U(uPedalCompFocus), -30.}, {U(uPedalCompBlend), 100.}}},
  {"Even sustain, some dry",
   {{U(uPedalCompSustain), 60.}, {U(uPedalCompAttack), 0.}, {U(uPedalCompBlend), 70.}}},
  {"Heavy squash, natural attack",
   {{U(uPedalCompSustain), 70.}, {U(uPedalCompAttack), 0.}, {U(uPedalCompBlend), 40.}}},
  {"Gentle leveling",
   {{U(uPedalCompSustain), 10.}, {U(uPedalCompAttack), 1.}, {U(uPedalCompBlend), 100.}}},
  {"Long lead notes",
   {{U(uPedalCompSustain), 75.}, {U(uPedalCompAttack), 1.}, {U(uPedalCompFocus), -20.}, {U(uPedalCompBlend), 60.}}},
  {"Acoustic strumming",
   {{U(uPedalCompPickup), 2.}, {U(uPedalCompSustain), 35.}, {U(uPedalCompAttack), 1.}, {U(uPedalCompFocus), 30.},
    {U(uPedalCompBlend), 85.}}},
  {"Acoustic fingerpicking",
   {{U(uPedalCompPickup), 2.}, {U(uPedalCompSustain), 25.}, {U(uPedalCompAttack), 1.}, {U(uPedalCompFocus), 15.},
    {U(uPedalCompBlend), 70.}}},
};
// Thresholds assume typical notes near -18 dBFS on these peak detectors (a DAW peak meter
// around -9 dBFS). Measured on a guitar DI and after a clean capture, typical notes come down
// by about 0.5-6 dB (Firm control: about 8 dB), with makeup near bypass loudness.
inline const Preset smoothComp[]{
  {"Clean guitar leveling",
   {{kSlotCompThreshold, -22.}, {kSlotCompRelease, 150.}, {kSlotCompMakeup, 4.5}, {U(uCompAttack), 8.},
    {U(uSmoothRatio), 3.5}}},
  {"Barely there",
   {{kSlotCompThreshold, -16.}, {kSlotCompRelease, 200.}, {kSlotCompMakeup, 1.}, {U(uCompAttack), 30.},
    {U(uSmoothRatio), 1.5}}},
  {"Long gentle sustain",
   {{kSlotCompThreshold, -30.}, {kSlotCompRelease, 150.}, {kSlotCompMakeup, 4.}, {U(uCompAttack), 30.},
    {U(uSmoothRatio), 1.6}}},
  {"Acoustic strumming leveler",
   {{kSlotCompThreshold, -22.}, {kSlotCompRelease, 120.}, {kSlotCompMakeup, 3.}, {U(uCompAttack), 25.},
    {U(uSmoothRatio), 3.}, {U(uCompDetectorCut), 120.}}},
  {"Even bass notes",
   {{kSlotCompThreshold, -22.}, {kSlotCompRelease, 200.}, {kSlotCompMakeup, 4.5}, {U(uCompAttack), 15.},
    {U(uSmoothRatio), 4.}}},
};
inline const Preset punchComp[]{
  {"Tight picking",
   {{kSlotCompThreshold, -22.}, {kSlotCompRelease, 40.}, {kSlotCompMakeup, 4.}, {U(uPunchAttack), 2.},
    {U(uPunchRatio), 3.6}, {U(uCompMix), 100.}}},
  {"Upfront, keeps dynamics",
   {{kSlotCompThreshold, -20.}, {kSlotCompRelease, 350.}, {kSlotCompMakeup, 3.5}, {U(uPunchAttack), 10.},
    {U(uCompMix), 80.}}},
  {"Firm control after an amp",
   {{kSlotCompThreshold, -22.}, {kSlotCompRelease, 40.}, {kSlotCompMakeup, 6.}, {U(uPunchAttack), .5},
    {U(uPunchRatio), 8.}, {U(uCompMix), 100.}}},
  {"Parallel punch",
   {{kSlotCompThreshold, -28.}, {kSlotCompRelease, 100.}, {kSlotCompMakeup, 6.}, {U(uPunchAttack), 20.},
    {U(uPunchRatio), 6.}, {U(uCompMix), 50.}}},
};
inline const Preset butterComp[]{
  {"Subtle glue", {{U(uButterCompress), 50.}, {U(uButterOutput), 0.}}},
  {"Dense and even", {{U(uButterCompress), 85.}, {U(uButterOutput), 0.}}},
  {"Thick parallel", {{U(uButterCompress), 100.}, {U(uButterOutput), 0.}, {U(uButterMix), 50.}}},
};
inline const Preset limiter[]{
  {"Transparent safety", {{U(uLimiterCeiling), -.5}, {U(uLimiterRelease), 150.}}},
  {"Louder, controlled",
   {{U(uLimiterInput), 6.}, {U(uLimiterCeiling), -1.}, {U(uLimiterRelease), 100.}, {U(uLimiterClip), 30.}}},
};
inline const Preset digitalDelay[]{
  {"Slapback",
   {{U(uDelayTime), 90.}, {U(uDelayFeedback), 0.}, {U(uDelayTone), 8000.}, {U(uDelayCharacter), 0.},
    {U(uDelayMix), 25.}}},
  {"Dotted eighth",
   {{U(uTempoSync), 1.}, {U(uTempoDivision), 4.}, {U(uDelayFeedback), 35.}, {U(uDelayTone), 6000.},
    {U(uDelayCharacter), 30.}, {U(uDelayMix), 22.}}},
  {"Ducked lead",
   {{U(uDelayTime), 420.}, {U(uDelayFeedback), 40.}, {U(uDelayTone), 7000.}, {U(uDelayCharacter), 60.},
    {U(uDelayMix), 30.}}},
};

inline std::span<const Preset> For(SlotEffect effect)
{
  switch (effect)
  {
    case SlotEffect::PedalComp: return pedalComp;
    case SlotEffect::SmoothComp: return smoothComp;
    case SlotEffect::PunchComp: return punchComp;
    case SlotEffect::ButterComp: return butterComp;
    case SlotEffect::Limiter: return limiter;
    case SlotEffect::DigitalDelay: return digitalDelay;
    default: return {};
  }
}
// Slot controls the module's editor can show: a preset sets these and nothing else.
inline bool ModuleControl(SlotEffect effect, int control)
{
  using namespace parameter_layout;
  if (control >= kSlotCompThreshold && control <= kSlotCompMakeup)
    return effect == SlotEffect::SmoothComp || effect == SlotEffect::PunchComp;
  if (control >= kSlotGateThreshold && control <= kSlotGateReduction) return effect == SlotEffect::NoiseGate;
  const int p = control - standardControls;
  if (p < 0 || p >= kUtilityControls) return false;
  for (bool sync : {false, true})
    for (bool alternate : {false, true})
      if (effect_presentation::Visible(effect, p, sync, alternate, alternate, alternate)) return true;
  return false;
}
// The player's own setup, kept across presets that do not name it.
inline bool KeepsSetup(SlotEffect effect, int control)
{ return effect == SlotEffect::PedalComp && control == U(uPedalCompPickup); }
// The complete slot values after choosing a preset: module controls the preset leaves out
// return to the module's insertion defaults (setup controls keep their current value),
// everything else keeps its current value.
inline SlotValues Apply(SlotEffect effect, std::span<const Value> values, const SlotValues& current)
{
  auto result = current;
  const auto defaults = SlotDefaultValues(effect);
  for (int i = 0; i < static_cast<int>(result.size()); ++i)
    if (ModuleControl(effect, i) && !KeepsSetup(effect, i)) result[i] = defaults[i];
  for (const auto& value : values) result[value.control] = value.value;
  return result;
}
inline std::span<const Value> Values(const Preset& preset)
{ return std::span<const Value>(preset.values.begin(), preset.values.size()); }
inline SlotValues Apply(SlotEffect effect, const Preset& preset, const SlotValues& current)
{ return Apply(effect, Values(preset), current); }
inline bool Matches(SlotEffect effect, std::span<const Value> values, const SlotValues& current)
{
  const auto expected = Apply(effect, values, current);
  for (size_t i = 0; i < expected.size(); ++i)
    if (std::abs(expected[i] - current[i]) > 1.e-6 * std::max(1., std::abs(expected[i]))) return false;
  return true;
}
// The factory preset the current values equal, or -1 once a control has moved away from it.
inline int Matching(SlotEffect effect, const SlotValues& current)
{
  const auto presets = For(effect);
  for (size_t index = 0; index < presets.size(); ++index)
    if (Matches(effect, Values(presets[index]), current)) return static_cast<int>(index);
  return -1;
}

// User presets. Values are stored under the controls' permanent state names without a slot
// prefix ("Pedal Comp Sustain", "Comp Threshold"), so a file works in any slot. Controls
// added after the file's schema load at their defaults, like a project.
inline constexpr const char* kUserFormat = "nam-grid-module-preset";
inline constexpr size_t kUserMaximumBytes = 64 * 1024, kUserMaximumFiles = 2000;
inline const char* ControlName(int control)
{
  const int p = control - parameter_layout::standardControls;
  return p < 0 ? parameter_layout::slotControlNames[control] : kEffectParameters[p].name;
}
inline parameter_layout::ControlRange RangeOf(int control)
{
  const int p = control - parameter_layout::standardControls;
  return p < 0 ? parameter_layout::slotControlRanges[control]
               : parameter_layout::ControlRange{kEffectParameters[p].minimum, kEffectParameters[p].maximum};
}
inline nlohmann::json ToJson(SlotEffect effect, const SlotValues& values)
{
  auto stored = nlohmann::json::object();
  for (int i = 0; i < static_cast<int>(values.size()); ++i)
    if (ModuleControl(effect, i)) stored[ControlName(i)] = values[i];
  return {{"format", kUserFormat}, {"version", 1}, {"schema", kEffectStateSchema},
          {"module", static_cast<int>(effect)}, {"moduleName", SlotEffectName(effect)}, {"values", stored}};
}
// Validates a user preset for this module. Returns an error message, or empty with values.
inline std::string Parse(const nlohmann::json& document, SlotEffect effect, std::vector<Value>& values)
{
  values.clear();
  const auto integer = [&](const char* key) {
    return document.contains(key) && document[key].is_number_integer() ? document[key].get<int64_t>() : int64_t{-1};
  };
  if (!document.is_object() || document.value("format", "") != kUserFormat || integer("version") != 1)
    return "This is not a NAM Grid module preset.";
  const auto schema = integer("schema");
  if (schema > kEffectStateSchema) return "This preset was saved by a newer NAM Grid.";
  if (schema < kInitialEffectStateSchema) return "This preset has an invalid schema.";
  if (integer("module") != static_cast<int>(effect)) return "This preset is for another module.";
  if (!document.contains("values") || !document["values"].is_object()) return "This preset has no values.";
  for (const auto& [key, value] : document["values"].items())
  {
    int control = -1;
    for (int i = 0; i < static_cast<int>(SlotValues{}.size()) && control < 0; ++i)
      if (ModuleControl(effect, i) && key == ControlName(i)) control = i;
    if (control < 0) return "Unknown control \"" + key + "\".";
    if (!value.is_number() || !std::isfinite(value.get<double>())) return "\"" + key + "\" is not a number.";
    const double number = value.get<double>();
    const auto range = RangeOf(control);
    if (number < range.minimum || number > range.maximum) return "\"" + key + "\" is out of range.";
    const int p = control - parameter_layout::standardControls;
    if (p >= 0 && kEffectParameters[p].step == 1. && number != std::floor(number))
      return "\"" + key + "\" must be a whole number.";
    values.push_back({control, number});
  }
  return {};
}
inline std::filesystem::path UserFolder(const std::filesystem::path& presetFolder, SlotEffect effect)
{
  std::string name = SlotEffectName(effect);
  for (auto& c : name)
    if (c == '/' || c == '\\' || c == ':') c = '-';
  return presetFolder / "Module Presets" / std::filesystem::u8path(name);
}
struct UserPreset
{
  std::string name;
  std::filesystem::path path;
  std::vector<Value> values;
};
inline std::string Utf8(const std::filesystem::path& path)
{
  const auto text = path.u8string();
  return std::string(text.begin(), text.end());
}
inline std::string Lower(std::string text)
{
  std::transform(text.begin(), text.end(), text.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  return text;
}
// Every readable preset for this module under each folder's "Module Presets", sorted by
// name. Files for other modules are skipped; unreadable ones only add to `unreadable`.
inline std::vector<UserPreset> LoadUser(const std::vector<std::filesystem::path>& presetFolders, SlotEffect effect,
                                        int* unreadable = nullptr)
{
  std::vector<UserPreset> result;
  int failed = 0;
  size_t seen = 0;
  for (const auto& folder : presetFolders)
  {
    std::error_code error;
    const auto root = folder / "Module Presets";
    if (!std::filesystem::is_directory(root, error)) continue;
    for (auto it = std::filesystem::recursive_directory_iterator(
           root, std::filesystem::directory_options::skip_permission_denied, error);
         !error && it != std::filesystem::recursive_directory_iterator() && seen < kUserMaximumFiles; it.increment(error))
    {
      if (!it->is_regular_file(error) || Lower(Utf8(it->path().extension())) != ".json") continue;
      ++seen;
      try
      {
        std::ifstream input(it->path(), std::ios::binary | std::ios::ate);
        const auto size = input ? static_cast<size_t>(input.tellg()) : size_t{0};
        if (size == 0 || size > kUserMaximumBytes) throw std::runtime_error("size");
        std::string text(size, '\0');
        input.seekg(0);
        input.read(text.data(), static_cast<std::streamsize>(size));
        const auto document = nlohmann::json::parse(text);
        // Another module's preset is not a problem with this one.
        if (document.is_object() && document.contains("module") && document["module"].is_number_integer()
            && document["module"] != static_cast<int>(effect))
          continue;
        UserPreset preset{Utf8(it->path().stem()), it->path(), {}};
        if (!Parse(document, effect, preset.values).empty()) throw std::runtime_error("invalid");
        result.push_back(std::move(preset));
      }
      catch (const std::exception&)
      {
        ++failed;
      }
    }
  }
  std::sort(result.begin(), result.end(), [](const UserPreset& a, const UserPreset& b) { return Lower(a.name) < Lower(b.name); });
  if (unreadable) *unreadable = failed;
  return result;
}
inline void SaveUser(const std::filesystem::path& file, SlotEffect effect, const SlotValues& values)
{
  std::filesystem::create_directories(file.parent_path());
  rig_preset::WriteAtomically(file, ToJson(effect, values).dump(2));
}
} // namespace module_presets
