#pragma once
#include "../NeuralAmpModeler/PerformanceSceneState.h"
#include "../NeuralAmpModeler/RigPreset.h"

template <class Save, class Load, class Raw>
void TestPerformanceSceneState(const clap_plugin* plugin, json base, Save save, Load load, Raw raw, const std::filesystem::path& home)
{
  const auto* params = static_cast<const clap_plugin_params*>(plugin->get_extension(plugin, CLAP_EXT_PARAMS));
  std::vector<std::string> names(kNumParams);
  for (uint32_t i = 0; i < params->count(plugin); ++i)
  {
    clap_param_info info{}; assert(params->get_info(plugin, i, &info));
    assert(info.id < names.size()); names[info.id] = info.name;
  }
  const auto baseline = [&](const json& config) {
    return performance_scenes::Baseline(config, [&](int i) { return names[i]; });
  };
  auto original = baseline(base);
  auto legacy = base;
  legacy["schema"] = 22;
  legacy.erase("PerformanceSceneDeltas");
  legacy["PerformanceScenes"] = json::array({original, original, original});
  assert(raw(legacy));
  auto compact = save();
  const auto compactConfig = compact.config();
  assert(compact.bytes.size() < legacy.dump().size() / 2);
  assert(compact.config()["schema"] == kEffectStateSchema); // Current schema, in the compact form.
  for (const auto& delta : compactConfig["PerformanceSceneDeltas"]) assert(delta.is_object() && delta.empty());
  for (const auto& scene : compactConfig["PerformanceScenes"]) assert(scene.empty());
  std::cout << "Scene snapshot size: full=" << legacy.dump().size() << " compact=" << compact.bytes.size() << " bytes\n";

  // A formerly inherited value must become an override when the current rig changes.
  clap_event_param_value event{};
  event.header.size = sizeof(event); event.header.type = CLAP_EVENT_PARAM_VALUE;
  event.param_id = PerformanceParam(grid_performance::LaneAGain);
  event.note_id = event.port_index = event.channel = event.key = -1;
  assert(params->text_to_value(plugin, event.param_id, "7.25", &event.value));
  clap_input_events input{&event, [](const clap_input_events*) -> uint32_t { return 1; },
    [](const clap_input_events* e, uint32_t) -> const clap_event_header* { return &static_cast<clap_event_param_value*>(e->ctx)->header; }};
  clap_output_events output{nullptr, [](const clap_output_events*, const clap_event_header*) { return true; }};
  params->flush(plugin, &input, &output);
  auto edited = save().config();
  assert(edited["Lane A Level"] == 7.25);
  for (const auto& delta : edited["PerformanceSceneDeltas"]) assert(delta["Lane A Level"] == original["Lane A Level"]);
  assert(raw(edited));
  auto decoded = save().config();
  assert(performance_scenes::Expand(decoded, baseline(decoded)));
  for (const auto& scene : decoded["PerformanceScenes"]) assert(scene == original);
  assert(save().config() == edited);

  // Null is an unused scene; an empty object is a stored scene equal to the rig.
  auto mixed = compact.config();
  mixed["PerformanceSceneDeltas"][1] = nullptr;
  mixed["PerformanceScenes"][2] = {{"Output", -3.25}};
  mixed["PerformanceSceneDeltas"][2] = nullptr; // Preserve historical partial recall semantics.
  assert(raw(mixed)); assert(save().config() == mixed);
  const auto stable = save().config();
  const auto reject = [&](json bad) { assert(!raw(bad)); assert(save().config() == stable); };
  auto bad = stable; bad.erase("PerformanceSceneDeltas"); reject(bad);
  for (const auto& invalid : {json(nullptr), json::object(), json::array(), json::array({nullptr}),
                             json::array({nullptr, nullptr, nullptr, nullptr})})
  { bad = stable; bad["PerformanceSceneDeltas"] = invalid; reject(bad); }
  for (const auto& invalid : {json(1), json("scene"), json::array(), json{{"Not a parameter", 0.}},
                             json{{"Tuner", 1.}}, json{{"Pre 1 Noise Learn", 1.}},
                             json{{"Output", "invalid"}}, json{{"Post 4 Juno Mode", 1.5}}})
  { bad = stable; bad["PerformanceSceneDeltas"][0] = invalid; reject(bad); }
  bad = stable; bad["PerformanceScenes"][0] = {{"Output", -4.}}; reject(bad); // Ambiguous two sources.

  // A legacy schema without the additive field remains accepted and re-saves compactly.
  auto old = stable; old["schema"] = 15; old.erase("PerformanceSceneDeltas");
  old["PerformanceScenes"] = json::array({json{{"Output", -8.5}}, json::object(), json::object()});
  assert(raw(old));
  assert(save().config()["PerformanceScenes"] == old["PerformanceScenes"]);
  // Fully divergent scenes still fit the existing safety margin. Store enough
  // precision to exercise the longest JSON double representation and preset text.
  auto worst = legacy;
  for (int scene = 0; scene < 3; ++scene)
    for (uint32_t i = 0; i < params->count(plugin); ++i)
    {
      clap_param_info info{}; assert(params->get_info(plugin, i, &info));
      if (!performance_scenes::IsParameter(info.id) || (info.flags & CLAP_PARAM_IS_STEPPED)) continue;
      const double value = original[info.name].get<double>();
      const double offset = (scene + 1) * 1.2345678901234567e-7 * std::max(1., std::abs(value));
      worst["PerformanceScenes"][scene][info.name] = value + (info.default_value < .5 ? offset : -offset);
    }
  assert(raw(worst));
  auto worstSaved = save();
  assert(worstSaved.bytes.size() < rig_preset::kMaximumBytes / 4);
  const auto preset = home / "worst-scene-deltas.ngrid";
  rig_preset::Write(preset, worstSaved.config());
  assert(std::filesystem::file_size(preset) < rig_preset::kMaximumBytes / 4);
  assert(rig_preset::Read(preset) == worstSaved.config());
  std::cout << "Divergent scenes: state=" << worstSaved.bytes.size() << " preset=" << std::filesystem::file_size(preset) << " bytes\n";
  std::filesystem::remove(preset);
  load(base);
  std::cout << "Scene deltas: snapshot rebasing, full/partial legacy states, cleared scenes and atomic rejection passed\n";
}
