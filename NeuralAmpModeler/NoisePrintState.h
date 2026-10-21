#pragma once
#include "NoisePrint.h"
#include "json.hpp"
#include <stdexcept>

namespace noise_print
{
// Separate from main's evolving schemas. This experiment has its own plugin ID.
inline constexpr int experimentSchema = 1001;
inline nlohmann::json Save(const Profile& profile)
{
  if (!profile.valid)
    return nullptr;
  return {{"version", 1}, {"power", profile.power}};
}
inline Profile Load(const nlohmann::json& value)
{
  if (value.is_null())
    return {};
  if (!value.is_object() || !value.contains("version") || value["version"] != 1 || !value.contains("power")
      || !value["power"].is_array() || value["power"].size() != bands)
    throw std::runtime_error("Invalid Noise Print profile");
  Profile profile;
  double total = 0.;
  for (int i = 0; i < bands; ++i)
  {
    if (!value["power"][i].is_number())
      throw std::runtime_error("Invalid noise power");
    const double power = value["power"][i].get<double>();
    if (!std::isfinite(power) || power < 0. || power > 1.e6)
      throw std::runtime_error("Invalid noise power");
    profile.power[i] = power;
    total += power;
  }
  if (total <= 1.e-18)
    throw std::runtime_error("Empty noise profile");
  profile.valid = true;
  return profile;
}
} // namespace noise_print
