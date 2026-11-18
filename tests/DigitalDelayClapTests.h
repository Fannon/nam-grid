#pragma once

// Digital Delay ignored the shared Modulation control before schema 32. Older rigs and
// their partial scenes load with it at zero; other modules and current states keep theirs.
template <class Save, class Load>
void TestDigitalDelayUpgradeClap(const json& cleared, Save save, Load load)
{
  const auto key = [](int slot, int p) { return GridSlotName(slot) + " " + kEffectParameters[p].name; };
  const auto digital = key(0, uDelayMotion), tape = key(1, uDelayMotion);
  auto historical = cleared;
  historical["schema"] = kEffectParameters[uDigitalDelayVoice].introducedIn - 1;
  for (int p = uSpaceHeads; p <= uDigitalDelayVoice; ++p)
    for (int slot = 0; slot < 16; ++slot) historical.erase(key(slot, p));
  historical["SlotEffects"][0] = static_cast<int>(SlotEffect::DigitalDelay);
  historical["SlotEffects"][1] = static_cast<int>(SlotEffect::TapeDelay);
  historical[digital] = 40.;
  historical[tape] = 40.;
  historical["PerformanceScenes"][0] = json{{digital, 50.}, {tape, 50.}}; // A partial legacy scene.
  historical["PerformanceSceneDeltas"][0] = nullptr;
  load(historical);
  const auto upgraded = save().config();
  assert(upgraded[digital] == 0. && upgraded[tape] == 40. && upgraded[key(0, uDigitalDelayVoice)] == 0.);
  const auto scene = [&](const json& config, const std::string& name) {
    const auto& stored = config["PerformanceScenes"][0];
    return stored.contains(name) ? stored[name] : config["PerformanceSceneDeltas"][0][name];
  };
  assert(scene(upgraded, digital) == 0. && scene(upgraded, tape) == 50.);
  auto current = upgraded;
  current[digital] = 40.;
  load(current);
  assert(save().config()[digital] == 40.);
  auto reset = cleared;
  load(reset);
  std::cout << "Digital Delay CLAP: schema 31 rigs and scenes keep unmodulated repeats\n";
}
