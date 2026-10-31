#pragma once
#include "../NeuralAmpModeler/EffectPresentation.h"
#include "../NeuralAmpModeler/SlotDefaults.h"

// Read shared ranges from the real host parameters; module descriptors and
// insertion defaults provide the rest. No copied numeric documentation contract.
inline json ModuleControlMetadata(const clap_plugin* plugin, const clap_plugin_params* params)
{
  using namespace parameter_layout;
  const auto displayDefault = [&](int control, double physical) {
    double encoded = 0.;
    const auto text = json(physical).dump();
    assert(params->text_to_value(plugin, SlotParam(0, control), text.c_str(), &encoded));
    char formatted[128]{};
    assert(params->value_to_text(plugin, SlotParam(0, control), encoded, formatted, sizeof(formatted)));
    // Named endpoints such as the Smart Gate's "Off" still have a documented
    // numeric threshold. Otherwise use the plugin's actual display rounding.
    try { return std::stod(formatted); }
    catch (const std::invalid_argument&) { return physical; }
  };
  json modules = json::object();
  for (int id = 0; id <= static_cast<int>(SlotEffect::Last); ++id)
  {
    const auto effect = static_cast<SlotEffect>(id);
    if (!IsValidSlotEffect(effect)) continue;
    auto& controls = modules[SlotEffectName(effect)] = json::array();
    const auto defaults = SlotDefaultValues(effect);
    const auto shared = [&](int control, const char* label) {
      clap_param_info info{};
      assert(params->get_info(plugin, SlotParam(0, control), &info));
      const auto endpoint = [&](double value) {
        char text[128]{};
        assert(params->value_to_text(plugin, info.id, value, text, sizeof(text)));
        size_t end = 0;
        const double number = std::stod(text, &end);
        std::string unit = std::string(text).substr(end);
        unit.erase(0, unit.find_first_not_of(' '));
        return std::pair{number, unit};
      };
      const auto [minimum, unit] = endpoint(info.min_value);
      const auto [maximum, maximumUnit] = endpoint(info.max_value);
      assert(unit == maximumUnit);
      controls.push_back({{"name", info.name}, {"labels", {label}}, {"minimum", minimum}, {"maximum", maximum},
                          {"default", defaults[control]}, {"display_default", displayDefault(control, defaults[control])},
                          {"unit", unit}, {"binary", false}});
    };
    if (effect == SlotEffect::NAM)
    {
      shared(kSlotIn, "Input"); shared(kSlotTrim, "Output");
      shared(kSlotEQBass, "Bass"); shared(kSlotEQMid, "Mid"); shared(kSlotEQTreble, "Treble");
    }
    if (effect == SlotEffect::NAM || effect == SlotEffect::CutEQ || effect == SlotEffect::ParametricEQ)
    { shared(kSlotEQHighPass, "Low cut"); shared(kSlotEQLowPass, "High cut"); }
    if (effect == SlotEffect::Boost || effect == SlotEffect::TubeBoost) shared(kSlotEQHighPass, "Low cut");
    if (effect == SlotEffect::ToneEQ)
    { shared(kSlotEQBass, "Bass"); shared(kSlotEQMid, "Mid"); shared(kSlotEQTreble, "Treble"); }
    if (effect == SlotEffect::CutEQ) shared(kSlotEQPresence, "Presence");
    if (effect == SlotEffect::CabinetIR) shared(kSlotIRLevel, "Output");
    if (effect == SlotEffect::NoiseGate)
    { shared(kSlotGateThreshold, "Threshold"); shared(kSlotGateRelease, "Release"); shared(kSlotGateReduction, "Reduction"); }
    if (effect == SlotEffect::SmoothComp || effect == SlotEffect::PunchComp)
    { shared(kSlotCompThreshold, "Threshold"); shared(kSlotCompRelease, "Release"); shared(kSlotCompMakeup, "Makeup"); }
    for (int p = 0; p < kUtilityControls; ++p)
    {
      // Header choices and display controls are documented as prose; tables cover the knob grid.
      if (p == uTempoDivision || effect_presentation::HeaderControl(p) || effect_presentation::DisplayControl(p)) continue;
      bool visible = false;
      for (bool sync : {false, true})
        for (bool alternate : {false, true})
          visible |= effect_presentation::Visible(effect, p, sync, alternate, alternate, alternate);
      if (!visible) continue;
      const auto& d = kEffectParameters[p];
      json labels = json::array({d.label});
      const char* presentation = effect_presentation::Label(effect, p);
      if (std::string(presentation) != d.label) labels.push_back(presentation);
      const bool binary = d.minimum == 0. && d.maximum == 1. && d.step == 1.;
      const double initial = defaults[standardControls + p];
      controls.push_back({{"name", d.name}, {"labels", labels}, {"minimum", d.minimum}, {"maximum", d.maximum},
                          {"default", initial}, {"unit", d.unit}, {"binary", binary},
                          {"display_default", binary ? initial : displayDefault(standardControls + p, initial)}});
      if (binary)
      {
        json choices = json::array();
        for (double encoded : {0., 1.})
        {
          char text[128]{};
          assert(params->value_to_text(plugin, SlotUtilityParam(0, p), encoded, text, sizeof(text)));
          choices.push_back(text);
        }
        controls.back()["choices"] = choices;
      }
    }
  }
  return modules;
}
