// NAM Grid Console for MPC (shown as NG Console): the Airwindows-based mix colour modules (Console Colour,
// Outboard Channel, Butter Comp) behind one Type menu, for buses and finished beats.
#include "MPCEngine.h"

namespace mpc_plugins
{
inline const mpc_grid::PluginSpec& ConsoleSpec()
{
  using namespace mpc_grid;
  static const PluginSpec spec{
    "NG Console", MPC_FOURCC('N', 'G', 'C', 's'), "nam-grid-console",
    {{"console", "Console",
      {{"console-colour", "Console Colour", SlotEffect::ConsoleColour},
       {"outboard-channel", "Outboard", SlotEffect::OutboardChannel},
       {"butter-comp", "Butter Comp", SlotEffect::ButterComp}},
      0, false, true, "Type", false, {U(uColourDesk)}}},
    {{"Console Colour", {0}}, {"Outboard Channel", {1}}, {"Butter Comp", {2}}},
  };
  return spec;
}
} // namespace mpc_plugins

MPC_GRID_PLUGIN(mpc_plugins::ConsoleSpec)
