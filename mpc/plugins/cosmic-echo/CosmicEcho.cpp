// NAM Grid Cosmic Echo for MPC (shown as NG Cosmic): the Cosmic Echo module with its styles as one menu.
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
    PluginSpec s{"NG Cosmic", MPC_FOURCC('N', 'G', 'C', 'e'), "nam-grid-cosmic-echo",
                 {{"echo", "Echo", styles, initial, false, true, "Style", false, {}, 10}}, {}};
    // Keep the insertion sound first. Programs load complete desktop recipes;
    // changing Style alone keeps the player's knob settings.
    s.programs.push_back({effect_modes::cosmicNames[initial], {initial}});
    for (const auto& recipe : module_presets::For(SlotEffect::CosmicEcho))
    {
      int style = initial;
      for (const auto& value : recipe.values)
        if (value.control == U(uCosmicStyle)) style = static_cast<int>(value.value);
      s.programs.push_back({recipe.name, {style}, {}, {&recipe}});
    }
    return s;
  }();
  return spec;
}
} // namespace mpc_plugins

MPC_GRID_PLUGIN(mpc_plugins::CosmicEchoSpec)
