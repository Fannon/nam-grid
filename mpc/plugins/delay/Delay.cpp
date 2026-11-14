// NAM Grid Delay for MPC (shown as NG Delay): the NAM Grid delay family behind one Type menu. Tempo sync
// follows the MPC's tempo. Each type starts from its own starting settings.
#include "MPCEngine.h"

namespace mpc_plugins
{
inline const mpc_grid::PluginSpec& DelaySpec()
{
  using namespace mpc_grid;
  static const PluginSpec spec{
    "NG Delay", MPC_FOURCC('N', 'G', 'D', 'l'), "nam-grid-delay",
    {{"delay", "Delay",
      {{"digital-delay", "Digital", SlotEffect::DigitalDelay},
       {"analog-delay", "Analog", SlotEffect::AnalogDelay},
       {"tape-delay", "Tape", SlotEffect::TapeDelay},
       {"multi-head-delay", "Multi-Head", SlotEffect::MultiHeadDelay},
       {"diffused-delay", "Diffused", SlotEffect::DiffusedDelay},
       {"reverse-delay", "Reverse", SlotEffect::ReverseDelay},
       {"dual-delay", "Dual", SlotEffect::DualDelay}},
      1, false, true, "Type"}},
    {{"Digital", {0}}, {"Analog", {1}}, {"Tape", {2}}, {"Multi-Head", {3}}, {"Diffused", {4}}, {"Reverse", {5}},
     {"Dual", {6}}},
  };
  return spec;
}
} // namespace mpc_plugins

MPC_GRID_PLUGIN(mpc_plugins::DelaySpec)
