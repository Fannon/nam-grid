// NAM Grid Mod for MPC: the characterful modulation modules (Juno Chorus, Uni-Vibe, Rotary,
// Harmonic Tremolo) behind one Type menu. Tempo sync follows the MPC's tempo.
#include "MPCEngine.h"

namespace mpc_plugins
{
inline const mpc_grid::PluginSpec& ModSpec()
{
  using namespace mpc_grid;
  static const PluginSpec spec{
    "NAM Grid Mod", MPC_FOURCC('N', 'G', 'M', 'd'), "nam-grid-mod",
    {{"mod", "Mod",
      {{"juno-chorus", "Juno Chorus", SlotEffect::JunoChorus},
       {"uni-vibe", "Uni-Vibe", SlotEffect::UniVibe},
       {"rotary", "Rotary", SlotEffect::Rotary},
       {"harmonic-tremolo", "Harmonic Trem", SlotEffect::HarmonicTremolo}},
      0, false, true, "Type"}},
    {{"Juno Chorus", {0}}, {"Uni-Vibe", {1}}, {"Rotary", {2}}, {"Harmonic Tremolo", {3}}},
  };
  return spec;
}
} // namespace mpc_plugins

MPC_GRID_PLUGIN(mpc_plugins::ModSpec)
