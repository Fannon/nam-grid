#pragma once
#include "ParameterLayout.h"
#include "json.hpp"

// Full snapshots live in memory. Deltas always refer to the rig in the same saved
// document and are expanded before it is applied, never at scene recall time.
namespace performance_scenes
{
inline constexpr int deltaSchema = 23;
inline bool IsParameter(int index)
{
  if (index == kTunerReference || index == kTunerMute || index == kTunerActive) return false;
  // Quality is instance preparation; held audio and noise learning are transient.
  for (size_t slot = 0; slot < parameter_layout::slots; ++slot)
    if (index == SlotFullParam(slot) || index == SlotUtilityParam(slot, uFreezeHold)
        || index == SlotUtilityParam(slot, uShimmerFreeze) || index == SlotUtilityParam(slot, uNoiseLearn))
      return false;
  return index >= 0 && index < kNumParams;
}
template <typename Name>
nlohmann::json Baseline(const nlohmann::json& config, Name name)
{
  auto values = nlohmann::json::object();
  for (int i = 0; i < kNumParams; ++i)
    if (IsParameter(i)) values[name(i)] = config.at(name(i));
  return values;
}
inline void Compact(nlohmann::json& config, const nlohmann::json& scenes, const nlohmann::json& baseline)
{
  auto legacy = nlohmann::json::array(), deltas = nlohmann::json::array();
  for (const auto& scene : scenes)
  {
    bool complete = !scene.empty() && scene.size() == baseline.size();
    for (auto it = scene.begin(); complete && it != scene.end(); ++it) complete = baseline.contains(it.key());
    if (complete)
    {
      auto diff = nlohmann::json::object();
      for (auto it = scene.begin(); it != scene.end(); ++it)
        if (it.value() != baseline.at(it.key())) diff[it.key()] = it.value();
      legacy.push_back(nlohmann::json::object()); deltas.push_back(std::move(diff));
    }
    else
    {
      // Older states may contain partial scenes. Missing controls mean leave them
      // alone on recall; retain that representation rather than filling them in.
      legacy.push_back(scene); deltas.push_back(nullptr);
    }
  }
  config["PerformanceScenes"] = std::move(legacy);
  config["PerformanceSceneDeltas"] = std::move(deltas);
}
inline bool Expand(nlohmann::json& config, const nlohmann::json& baseline)
{
  if (!config.contains("PerformanceSceneDeltas")) return config.at("schema").get<int>() < deltaSchema;
  const auto& deltas = config.at("PerformanceSceneDeltas");
  auto& scenes = config.at("PerformanceScenes");
  if (!deltas.is_array() || deltas.size() != scenes.size()) return false;
  for (size_t i = 0; i < scenes.size(); ++i)
  {
    if (deltas[i].is_null()) continue; // Cleared scene or legacy partial snapshot.
    if (!deltas[i].is_object() || !scenes[i].is_object() || !scenes[i].empty()) return false;
    auto values = baseline;
    for (auto it = deltas[i].begin(); it != deltas[i].end(); ++it)
    {
      if (!baseline.contains(it.key())) return false;
      values[it.key()] = it.value(); // The caller validates all values before applying.
    }
    scenes[i] = std::move(values);
  }
  return true;
}
} // namespace performance_scenes
