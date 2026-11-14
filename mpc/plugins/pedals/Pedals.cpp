// NAM Grid Pedals for MPC (shown as NG Pedals): optional Pedal Comp -> one pedal.
// The Voice menu holds None, Clean Boost, Tube Boost and every Pedal Drive and Pedal
// Distortion voice. Another voice of the same pedal keeps the other settings, like a voice
// switch; another pedal returns to its last settings (or its starting settings). Every voice
// has its gain and output in the first Q-Link column.
#include "MPCEngine.h"

namespace mpc_plugins
{
inline const mpc_grid::PluginSpec& PedalsSpec()
{
  using namespace mpc_grid;
  static const PluginSpec spec = [] {
    using parameter_layout::kSlotEQHighPass;
    std::vector<Choice> voices{
      {"none", "None", SlotEffect::NAM},
      {"boost", "Clean Boost", SlotEffect::Boost, FileKind::None, -1, 0,
       {U(uBoostGain), U(uBoostTilt), U(uBoostMidGain), kSlotEQHighPass}},
      {"tube-boost", "Tube Boost", SlotEffect::TubeBoost, FileKind::None, -1, 0,
       {U(uTubeDrive), U(uTubeOutput), U(uBoostTilt), U(uTubeMix), U(uTubeBias), U(uTubeHighCut), kSlotEQHighPass}},
    };
    for (int i = 0; i < effect_modes::pedalCount; ++i)
      voices.push_back({"pedal-drive", effect_modes::pedalNames[i], SlotEffect::PedalDrive, FileKind::None,
                        U(uPedalStyle), i,
                        {U(uPedalDrive), U(uPedalOutput), U(uPedalTone), U(uPedalMix), U(uPedalBody), U(uPedalCharacter),
                         U(uPedalSplit), U(uPedalLowKeep)}});
    for (int i = 0; i < effect_modes::pedalDistortionCount; ++i)
      voices.push_back({"pedal-distortion", effect_modes::pedalDistortionNames[i], SlotEffect::PedalDistortion,
                        FileKind::None, U(uPDistStyle), i,
                        {U(uPDistGain), U(uPDistLevel), U(uPDistTight), U(uPDistMix), U(uPDistBass), U(uPDistMid),
                         U(uPDistTreble), U(uPDistMidFrequency)}});
    const auto voice = [&](const char* name) {
      for (int i = 0; i < static_cast<int>(voices.size()); ++i)
        if (std::string_view(voices[i].name) == name) return i;
      return 0;
    };
    const int screamer = voice("Screamer");
    // Comp first in the signal path, off by default; its controls are on the main page too.
    PluginSpec s{"NG Pedals", MPC_FOURCC('N', 'G', 'P', 'd'), "nam-grid-pedals",
                 {{"comp", "Comp", {{"pedal-comp", "Pedal Comp", SlotEffect::PedalComp}}, 0, true, false},
                  {"drive", "Drive", voices, screamer, false, true, "Voice"}},
                 {}};
    s.programs = {
      {"Screamer", {0, screamer}, {0}},
      {"Clean Boost", {0, voice("Clean Boost")}, {0}},
      {"Compressed Clean Boost", {0, voice("Clean Boost")}, {1}},
      {"Tube Boost", {0, voice("Tube Boost")}, {0}},
      {"Compressed Blues", {0, voice("Blues OD")}, {1}},
      {"Transparent OD", {0, voice("Transparent OD")}, {0}},
      {"Vintage Fuzz", {0, voice("Vintage Fuzz")}, {0}},
      {"Octave Fuzz Lead", {0, voice("Octave Fuzz")}, {1}},
      {"Classic Dist", {0, voice("Classic Dist")}, {0}},
      {"British Crunch", {0, voice("British Crunch")}, {0}},
      {"Metal Stack", {0, voice("Metal Stack")}, {0}},
      {"Comp Only", {0, voice("None")}, {1}},
    };
    return s;
  }();
  return spec;
}
} // namespace mpc_plugins

MPC_GRID_PLUGIN(mpc_plugins::PedalsSpec)
