// NAM Grid Lo-Fi for MPC (shown as NG Lo-Fi): LoFi Tape Machine, Airwindows Tape and Bitcrusher behind one
// Type menu. Each type starts from its own starting settings.
#include "MPCEngine.h"

namespace mpc_plugins
{
inline const mpc_grid::PluginSpec& LoFiSpec()
{
  using namespace mpc_grid;
  static const PluginSpec spec{
    "NG Lo-Fi", MPC_FOURCC('N', 'G', 'L', 'f'), "nam-grid-lofi",
    {{"lofi", "LoFi",
      {{"lofi-tape", "LoFi Tape", SlotEffect::LoFiTape},
       {"tape", "Airwindows Tape", SlotEffect::Tape},
       {"bitcrusher", "Bitcrusher", SlotEffect::Bitcrusher}},
      0, false, true, "Type"}},
    {{"LoFi Tape", {0}}, {"Airwindows Tape", {1}}, {"Bitcrusher", {2}}},
  };
  return spec;
}
} // namespace mpc_plugins

MPC_GRID_PLUGIN(mpc_plugins::LoFiSpec)
