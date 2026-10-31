#pragma once
#include "EffectPresentation.h"
#include "ModuleFactoryPresets.h"
#include "ParameterLayout.h"
#include "SlotDefaults.h"
#include "RigPreset.h"
#include "NoisePrintState.h"
#include <optional>
#include <algorithm>
#include <cctype>
#include <cmath>
#include <initializer_list>
#include <span>
#include <string>
#include <string_view>
#include <vector>

// Starting points for one module, chosen from the module editor: read-only factory presets
// in code, and user presets as one JSON file each in the preset folders. Unlike rig presets
// they change only that slot's module controls through ordinary host parameter changes.
// Smart Gate user presets also restore a learned profile by preparing that slot anew.
// A project stores the resulting values and profile, not the preset.
namespace module_presets
{
inline bool Supported(SlotEffect effect)
{ return IsValidSlotEffect(effect) && effect != SlotEffect::NAM && effect != SlotEffect::CabinetIR && effect != SlotEffect::SignalMonitor; }
// Slot controls the module's editor can show: a preset sets these and nothing else.
inline bool ModuleControl(SlotEffect effect, int control)
{
  using namespace parameter_layout;
  if (control >= kSlotCompThreshold && control <= kSlotCompMakeup)
    return effect == SlotEffect::SmoothComp || effect == SlotEffect::PunchComp;
  if (control >= kSlotGateThreshold && control <= kSlotGateReduction) return effect == SlotEffect::NoiseGate;
  if (control >= kSlotEQBass && control <= kSlotEQTreble) return effect == SlotEffect::ToneEQ;
  if (control == kSlotEQHighPass)
    return effect == SlotEffect::CutEQ || effect == SlotEffect::ParametricEQ
           || effect == SlotEffect::Boost || effect == SlotEffect::TubeBoost;
  if (control == kSlotEQLowPass) return effect == SlotEffect::CutEQ || effect == SlotEffect::ParametricEQ;
  if (control == kSlotEQPresence) return effect == SlotEffect::CutEQ;
  const int p = control - standardControls;
  if (p < 0 || p >= kUtilityControls) return false;
  for (bool sync : {false, true})
    for (bool alternate : {false, true})
      if (effect_presentation::Visible(effect, p, sync, alternate, alternate, alternate)) return true;
  return false;
}
// The player's own setup, kept across presets that do not name it.
inline bool KeepsSetup(SlotEffect effect, int control)
{
  if (effect == SlotEffect::PedalComp) return control == U(uPedalCompPickup);
  if (effect == SlotEffect::Multiband)
    return control == U(uMultibandThreshold) || control == U(uMultibandLowSplit) || control == U(uMultibandHighSplit);
  return false;
}
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
inline constexpr int kUserVersion = 2;
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
inline nlohmann::json ToJson(SlotEffect effect, const SlotValues& values, const noise_print::Profile& profile = {})
{
  auto stored = nlohmann::json::object();
  for (int i = 0; i < static_cast<int>(values.size()); ++i)
    if (ModuleControl(effect, i))
      stored[ControlName(i)] = effect == SlotEffect::NoisePrint && i == U(uNoiseLearn) ? 0. : values[i];
  auto document = nlohmann::json{{"format", kUserFormat}, {"version", kUserVersion}, {"schema", kEffectStateSchema},
          {"module", static_cast<int>(effect)}, {"moduleName", SlotEffectName(effect)}, {"values", stored}};
  if (effect == SlotEffect::NoisePrint) document["noiseProfile"] = noise_print::Save(profile);
  return document;
}
// Validates a user preset for this module. Returns an error message, or empty with values.
inline std::string Parse(const nlohmann::json& document, SlotEffect effect, std::vector<Value>& values,
                         std::optional<noise_print::Profile>* profile = nullptr)
{
  values.clear();
  if (profile) profile->reset();
  const auto integer = [&](const char* key) {
    return document.contains(key) && document[key].is_number_integer() ? document[key].get<int64_t>() : int64_t{-1};
  };
  if (!document.is_object() || document.value("format", "") != kUserFormat || (integer("version") < 1 || integer("version") > kUserVersion))
    return "This is not a NAM Grid module preset.";
  const auto schema = integer("schema");
  if (schema > kEffectStateSchema) return "This preset was saved by a newer NAM Grid.";
  if (schema < kInitialEffectStateSchema) return "This preset has an invalid schema.";
  if (integer("module") != static_cast<int>(effect)) return "This preset is for another module.";
  if (!document.contains("values") || !document["values"].is_object()) return "This preset has no values.";
  // Version 1 files omitted profiles and must keep their controls-only behavior.
  // Version 2 Smart Gate files explicitly save either a profile or null (untrained).
  if (effect == SlotEffect::NoisePrint && integer("version") >= 2 && !document.contains("noiseProfile"))
    return "This preset has no noise profile.";
  if (document.contains("noiseProfile"))
  {
    if (effect != SlotEffect::NoisePrint) return "This module cannot use a noise profile.";
    try
    {
      const auto loaded = noise_print::Load(document["noiseProfile"]);
      if (profile) *profile = loaded;
    }
    catch (const std::exception& error) { return error.what(); }
  }
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
    // Match enum/bool registration in the plugin. A continuous knob's step is
    // its display precision, not a requirement that saved values be integral.
    const bool discrete = p >= 0 && (!EffectChoices(p).empty()
      || (range.minimum == 0. && range.maximum == 1. && kEffectParameters[p].step == 1.));
    if (discrete && number != std::floor(number))
      return "\"" + key + "\" must be a whole number.";
    // Older files could save Learn while it was held on. Recording is transient,
    // so recalling either format must leave Learn off without replacing a legacy profile.
    values.push_back({control, effect == SlotEffect::NoisePrint && control == U(uNoiseLearn) ? 0. : number});
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
  std::optional<noise_print::Profile> noiseProfile; // Absent in legacy controls-only files.
};
inline bool MatchesUser(SlotEffect effect, const UserPreset& preset, const SlotValues& current,
                        const noise_print::Profile& profile)
{
  return Matches(effect, preset.values, current) && (!preset.noiseProfile || *preset.noiseProfile == profile);
}
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
// A preset name is a filename stem, never a path chosen by a native file dialog.
inline std::filesystem::path NamedUserFile(const std::filesystem::path& folder, std::string name)
{
  if (folder.empty()) throw std::runtime_error("Add a preset folder in Settings before saving.");
  const auto first = name.find_first_not_of(" \t\r\n");
  if (first == std::string::npos) throw std::runtime_error("Enter a name for the preset.");
  name = name.substr(first, name.find_last_not_of(" \t\r\n") - first + 1);
  if (Lower(name).ends_with(".json")) name.resize(name.size() - 5);
  if (name.empty() || name == "." || name == ".." || name.back() == '.' || name.back() == ' ')
    throw std::runtime_error("Enter a name without a trailing dot or space.");
  if (name.size() > 128) throw std::runtime_error("Use a shorter preset name.");
  if (std::any_of(name.begin(), name.end(), [](unsigned char c) {
        return c < 32 || std::string_view("/\\<>:\"|?*").find(c) != std::string_view::npos;
      }))
    throw std::runtime_error("Preset names cannot contain paths or / \\ < > : \" | ? *.");
  const auto base = Lower(name.substr(0, name.find('.')));
  if (base == "con" || base == "prn" || base == "aux" || base == "nul"
      || (base.size() == 4 && (base.starts_with("com") || base.starts_with("lpt")) && base[3] >= '1' && base[3] <= '9'))
    throw std::runtime_error("Choose another preset name; this name is reserved on Windows.");
  return folder / std::filesystem::u8path(name + ".json");
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
        if (!Parse(document, effect, preset.values, &preset.noiseProfile).empty()) throw std::runtime_error("invalid");
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
inline void SaveUser(const std::filesystem::path& file, SlotEffect effect, const SlotValues& values,
                     const noise_print::Profile& profile = {})
{
  std::filesystem::create_directories(file.parent_path());
  rig_preset::WriteAtomically(file, ToJson(effect, values, profile).dump(2));
}
} // namespace module_presets
