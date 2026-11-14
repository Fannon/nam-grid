#pragma once
// The reviewed MPC compatibility baseline. UI wording belongs outside this contract.
inline nlohmann::json DescribeContract(const mpc_grid::GridPlugin& plugin)
{
  using namespace mpc_grid;
  const auto& spec = plugin.Spec();
  nlohmann::json result{{"uid", spec.uid}, {"format", spec.stateFormat}};
  auto& params = result["parameters"] = nlohmann::json::array();
  for (int i = 0; i < plugin.numParams(); ++i)
  {
    const auto& p = plugin.param(i);
    params.push_back({{"id", p.id}, {"kind", static_cast<int>(p.kind)}, {"minimum", p.min}, {"maximum", p.max},
                      {"default", p.def}, {"skew", p.skew}, {"logarithmic", p.logarithmic}, {"automatable", p.automatable}});
  }
  auto& sections = result["sections"] = nlohmann::json::array();
  for (const auto& section : spec.sections)
  {
    nlohmann::json j{{"id", section.id}, {"defaultChoice", section.defaultChoice}, {"defaultOn", section.defaultOn}};
    auto& options = j["options"] = nlohmann::json::array();
    const auto order = SectionOptionOrder(section);
    for (int c : order) options.push_back(ControlKey(c));
    auto& choices = j["choices"] = nlohmann::json::array();
    for (const auto& choice : section.choices)
    {
      nlohmann::json c{{"key", choice.key}, {"effect", static_cast<int>(choice.effect)},
                       {"files", static_cast<int>(choice.files)}, {"styleControl", choice.styleControl}, {"style", choice.style}};
      auto& layouts = c["layouts"] = nlohmann::json::array();
      for (bool sync : {false, true})
      {
        auto values = ChoiceDefaults(choice);
        values[U(uTempoSync)] = sync ? 1. : 0.;
        const auto layout = MakeLayout(choice, values, true, &order);
        nlohmann::json l;
        for (int control : layout.knobs) l["knobs"].push_back(control < 0 ? "" : ControlKey(control));
        for (int control : layout.options) l["options"].push_back(control < 0 ? "" : ControlKey(control));
        layouts.push_back(l);
      }
      choices.push_back(c);
    }
    sections.push_back(j);
  }
  return result;
}

inline nlohmann::json SavedParameters(mpc_grid::GridPlugin& plugin)
{
  nlohmann::json parameters = nlohmann::json::array();
  for (int i = 0; i < plugin.numParams(); ++i)
    parameters.push_back({{"id", plugin.param(i).id}, {"plain", plugin.get(i)}, {"normalized", plugin.getNormalised(i)}});
  std::vector<uint8_t> extra;
  plugin.saveExtraState(extra);
  return {{"parameters", parameters}, {"extra", nlohmann::json::parse(extra)}};
}
