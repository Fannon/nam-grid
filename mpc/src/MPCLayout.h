#pragma once
// NAM Grid for MPC: the module list and how a slot's generic MPC controls map onto
// NAM Grid slot controls. Plugin-framework free, so the desktop CTest suite can check it.
//
// A section exposes generic host parameters: Type, On, Folder, File, options and
// knobs. Their meaning follows the module chosen in the section. Knob places 0-3 and
// 4-7 are the desktop editor's top and bottom rows (effect_presentation::Position);
// header controls (tempo sync, styles, Learn) become the options.
#include "EffectPresentation.h"
#include "ParameterLayout.h"
#include "SlotDefaults.h"
#include "TempoSync.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <string>
#include <string_view>
#include <vector>

namespace mpc_grid
{
using parameter_layout::slotControls;
using parameter_layout::standardControls;

inline constexpr int kSlots = 4;
inline constexpr int kKnobs = 8;
inline constexpr int kOptions = 4;

enum class FileKind
{
  None,
  Capture,
  IR
};

// One entry of a section's Type menu: a NAM Grid module, optionally fixed to one style
// (a Pedal Drive voice, a Cosmic Echo style). `key` is the permanent state key of the module.
struct Choice
{
  const char* key;
  const char* name; // Screen text; may change.
  SlotEffect effect;
  FileKind files = FileKind::None;
  int styleControl = -1; // Slot control set to `style` by this choice, or -1.
  int style = 0;
  // Optional knob order (slot controls; -1 leaves a place empty), replacing the desktop
  // editor's places for modules whose MPC grouping should differ. Modules with tempo sync
  // keep the desktop places, where Division takes Time's place.
  std::vector<int> knobOrder = {};
};
inline constexpr int U(int utilityParam) { return standardControls + utilityParam; }
inline bool IsEmpty(const Choice& c) { return c.effect == SlotEffect::NAM && c.files == FileKind::None; }

inline int UtilityControl(int control) { return control - standardControls; }
inline bool IsUtility(int control) { return control >= standardControls && control < slotControls; }

// Range, curve and text of one slot control, matching the desktop host parameters.
struct ControlSpec
{
  double minimum = 0., maximum = 1., step = .1;
  const char* unit = "";
  int choices = 0;      // > 0: an enumerated control with this many entries.
  bool toggle = false;  // Two states, 0 and 1.
  bool exponential = false;
  double power = 1.;
};
inline ControlSpec Spec(int control)
{
  using namespace parameter_layout;
  ControlSpec s;
  if (control < 0 || control >= slotControls) return s;
  if (!IsUtility(control))
  {
    s.minimum = slotControlRanges[control].minimum;
    s.maximum = slotControlRanges[control].maximum;
    s.toggle = control == kSlotEnabled || control == kSlotFullQuality;
    switch (control)
    {
      case kSlotGateRelease: case kSlotCompRelease: s.step = 1.; s.unit = "ms"; break;
      case kSlotEQHighPass: s.step = 1.; s.unit = "Hz"; break;
      case kSlotEQLowPass: s.step = 10.; s.unit = "Hz"; break;
      case kSlotEnabled: case kSlotFullQuality: s.step = 1.; break;
      default: s.unit = "dB"; break;
    }
    return s;
  }
  const int p = UtilityControl(control);
  const auto& d = kEffectParameters[p];
  s.minimum = d.minimum;
  s.maximum = d.maximum;
  s.step = d.step;
  s.unit = d.unit;
  s.choices = static_cast<int>(EffectChoices(p).size());
  s.toggle = s.choices == 0 && d.minimum == 0. && d.maximum == 1. && d.step == 1.;
  s.exponential = d.exponential;
  s.power = d.power;
  return s;
}

// Plain value <-> 0..1, as iPlug2 maps the desktop host parameters (linear, power
// curve or exponential; enumerations and toggles round to whole steps).
inline double FromNormalized(int control, double norm)
{
  const auto s = Spec(control);
  norm = std::clamp(std::isfinite(norm) ? norm : 0., 0., 1.);
  if (s.toggle) return norm >= .5 ? 1. : 0.;
  if (s.choices > 0) return std::round(s.minimum + norm * (s.maximum - s.minimum));
  double value;
  if (s.power != 1.)
    value = s.minimum + std::pow(norm, s.power) * (s.maximum - s.minimum);
  else if (s.exponential)
  {
    const double low = s.minimum <= 0. ? 1.e-8 : s.minimum;
    value = std::exp(std::log(low) + norm * std::log(s.maximum / low));
  }
  else
    value = s.minimum + norm * (s.maximum - s.minimum);
  return std::clamp(value, s.minimum, s.maximum);
}
inline double ToNormalized(int control, double value)
{
  const auto s = Spec(control);
  if (!std::isfinite(value)) value = s.minimum;
  value = std::clamp(value, s.minimum, s.maximum);
  if (s.maximum <= s.minimum) return 0.;
  if (s.toggle) return value >= .5 ? 1. : 0.;
  double norm;
  if (s.power != 1.)
    norm = std::pow((value - s.minimum) / (s.maximum - s.minimum), 1. / s.power);
  else if (s.exponential)
  {
    const double low = s.minimum <= 0. ? 1.e-8 : s.minimum;
    norm = (std::log(std::max(value, low)) - std::log(low)) / std::log(s.maximum / low);
  }
  else
    norm = (value - s.minimum) / (s.maximum - s.minimum);
  return std::clamp(norm, 0., 1.);
}

// The slot controls behind a module's knobs and options (-1: unused).
struct Layout
{
  std::array<int, kKnobs> knobs;
  std::array<int, kOptions> options;
};
// The module's knobs in the desktop editor's places (or its knobOrder) and its options in order.
inline Layout BaseLayout(const Choice& choice, const SlotValues& values, bool slimmable)
{
  using namespace parameter_layout;
  Layout layout;
  layout.knobs.fill(-1);
  layout.options.fill(-1);
  if (IsEmpty(choice)) return layout;
  if (choice.effect == SlotEffect::NAM)
  {
    layout.knobs = {kSlotIn, kSlotEQBass, kSlotEQMid, kSlotEQTreble, kSlotTrim, kSlotEQHighPass, kSlotEQLowPass, -1};
    if (slimmable) layout.options[0] = kSlotFullQuality;
    return layout;
  }
  const SlotEffect e = choice.effect;
  const auto flag = [&](int p) { return values[standardControls + p] >= .5; };
  const bool sync = flag(uTempoSync), notch = flag(uEQMode), vibrato = flag(uVibeMode), shelf = flag(uParamLowMode);
  // Places held by the slot's shared controls, as in the desktop editor.
  if (e == SlotEffect::NoiseGate)
    layout.knobs = {kSlotGateThreshold, kSlotGateRelease, kSlotGateReduction, -1, -1, -1, -1, -1};
  if (e == SlotEffect::ToneEQ)
  {
    layout.knobs[0] = kSlotEQBass;
    layout.knobs[1] = kSlotEQMid;
    layout.knobs[2] = kSlotEQTreble;
  }
  if (e == SlotEffect::CabinetIR) layout.knobs[effect_presentation::kOutputPosition] = kSlotIRLevel;
  if (e == SlotEffect::Boost || e == SlotEffect::TubeBoost) layout.knobs[2] = kSlotEQHighPass;
  if (!choice.knobOrder.empty())
  {
    layout.knobs.fill(-1);
    for (size_t k = 0; k < choice.knobOrder.size() && k < kKnobs; ++k) layout.knobs[k] = choice.knobOrder[k];
  }
  int option = 0;
  for (int p = 0; p < kUtilityControls; ++p)
  {
    if (standardControls + p == choice.styleControl) continue; // Chosen by the Type menu.
    if (!effect_presentation::Visible(e, p, sync, notch, vibrato, shelf) || effect_presentation::DisplayControl(p)) continue;
    if (effect_presentation::HeaderControl(p))
    {
      if (option < kOptions) layout.options[option++] = standardControls + p;
      continue;
    }
    if (!choice.knobOrder.empty()) continue;
    const int place = effect_presentation::Position(e, p);
    if (place >= 0 && place < kKnobs && layout.knobs[place] < 0) layout.knobs[place] = standardControls + p;
  }
  return layout;
}
// `optionOrder` (optional) gives every option of a section a fixed place across its choices:
// option k shows optionOrder[k] when the choice uses it, so options never change meaning.
inline Layout MakeLayout(const Choice& choice, const SlotValues& values, bool slimmable,
                         const std::vector<int>* optionOrder = nullptr)
{
  auto layout = BaseLayout(choice, values, slimmable);
  if (!optionOrder) return layout;
  const auto used = layout.options;
  layout.options.fill(-1);
  for (size_t k = 0; k < optionOrder->size() && k < kOptions; ++k)
    if (std::find(used.begin(), used.end(), (*optionOrder)[k]) != used.end()) layout.options[k] = (*optionOrder)[k];
  return layout;
}

// Screen name of a control in a module (short; the MPC screen shows about 12 characters).
inline std::string ControlLabel(const Choice& choice, int control, const SlotValues& values)
{
  using namespace parameter_layout;
  if (control < 0 || control >= slotControls) return "";
  if (!IsUtility(control))
  {
    switch (control)
    {
      case kSlotIn: return "Input";
      case kSlotTrim: return "Output";
      case kSlotEQBass: return "Bass";
      case kSlotEQMid: return "Mid";
      case kSlotEQTreble: return "Treble";
      case kSlotEQHighPass: return "Low cut";
      case kSlotEQLowPass: return "High cut";
      case kSlotEQPresence: return "Presence";
      case kSlotIRLevel: return "Output";
      case kSlotFullQuality: return "Quality";
      case kSlotGateThreshold: return "Threshold";
      case kSlotGateRelease: return "Release";
      case kSlotGateReduction: return "Reduction";
      case kSlotCompThreshold: return "Threshold";
      case kSlotCompRelease: return "Release";
      case kSlotCompMakeup: return "Makeup";
      default: return slotControlNames[control];
    }
  }
  const int p = UtilityControl(control);
  const SlotEffect e = choice.effect;
  if (p == uTempoSync) return "Tempo sync";
  const int mode = static_cast<int>(std::lround(values[standardControls
                                                       + (e == SlotEffect::PedalDrive  ? uPedalStyle
                                                          : e == SlotEffect::GraphicEQ ? uGraphicVoicing
                                                                                       : p)]));
  return effect_presentation::Label(e, p, mode);
}

inline std::string FormatNumber(double value, double step, const char* unit)
{
  const std::string_view u = unit ? unit : "";
  char buffer[48];
  if (u == "Hz" && std::abs(value) >= 1000.)
  {
    const double k = value * .001;
    std::snprintf(buffer, sizeof buffer, k >= 10. ? "%.1f kHz" : "%.2f kHz", k);
    return buffer;
  }
  if (u == "ms" && std::abs(value) >= 1000.)
  {
    std::snprintf(buffer, sizeof buffer, "%.2f s", value * .001);
    return buffer;
  }
  const int decimals = step >= 1. ? 0 : step >= .1 ? 1 : 2;
  if (u.empty())
    std::snprintf(buffer, sizeof buffer, "%.*f", decimals, value);
  else
    std::snprintf(buffer, sizeof buffer, "%.*f %s", decimals, value, unit);
  return buffer;
}

// Value text of a control for the MPC screen (about 8 characters).
inline std::string FormatValue(int control, double value)
{
  using namespace parameter_layout;
  const auto s = Spec(control);
  if (control == kSlotFullQuality) return value >= .5 ? "Full" : "Lite";
  if ((control == kSlotEQHighPass && value <= s.minimum) || (control == kSlotEQLowPass && value >= s.maximum))
    return "Off";
  if (IsUtility(control))
  {
    const int p = UtilityControl(control);
    if (s.choices > 0)
    {
      const auto names = EffectChoices(p);
      const int index = std::clamp(static_cast<int>(std::lround(value - s.minimum)), 0, s.choices - 1);
      return names[index];
    }
    if (p == uPedalCompAttack) return value >= .5 ? "Slow" : "Fast";
    if ((p == uNoiseGateThreshold || p == uDistLowPreserve) && value <= s.minimum) return "Off";
  }
  if (s.toggle) return value >= .5 ? "On" : "Off";
  return FormatNumber(value, s.step, s.unit);
}

// Controls whose change alters the knob layout (and so the knob names).
inline bool ChangesLayout(int control)
{
  return control == standardControls + uTempoSync || control == standardControls + uEQMode
         || control == standardControls + uVibeMode || control == standardControls + uParamLowMode
         || control == standardControls + uPedalStyle || control == standardControls + uGraphicVoicing;
}
// Controls that need the module rebuilt off the audio thread when they change.
inline bool NeedsPreparation(int control)
{
  return control == parameter_layout::kSlotFullQuality || control == standardControls + uIRLength;
}
} // namespace mpc_grid
