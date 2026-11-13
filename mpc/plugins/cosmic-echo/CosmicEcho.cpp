// NAM Grid Cosmic Echo for MPC: the Cosmic Echo module with its styles as one menu.
#include "MPCEngine.h"

namespace mpc_plugins
{
inline const mpc_grid::PluginSpec& CosmicEchoSpec()
{
  using namespace mpc_grid;
  static const PluginSpec spec = [] {
    std::vector<Choice> styles;
    for (int i = 0; i < effect_modes::cosmicCount; ++i)
      styles.push_back({"cosmic-echo", effect_modes::cosmicNames[i], SlotEffect::CosmicEcho, FileKind::None,
                        standardControls + uCosmicStyle, i});
    const int initial = static_cast<int>(effect_modes::CosmicStyle::AuroraHall);
    PluginSpec s{"NAM Grid Cosmic Echo", MPC_FOURCC('N', 'G', 'C', 'e'), "nam-grid-cosmic-echo",
                 {{"echo", "Echo", styles, initial, false, true, "Style"}}, {}};
    // One program per style, the module's default first.
    s.programs.push_back({effect_modes::cosmicNames[initial], {initial}});
    for (int i = 0; i < effect_modes::cosmicCount; ++i)
      if (i != initial) s.programs.push_back({effect_modes::cosmicNames[i], {i}});
    return s;
  }();
  return spec;
}
} // namespace mpc_plugins

MPC_GRID_PLUGIN(mpc_plugins::CosmicEchoSpec)
