// NAM Grid Amp for MPC (shown as NG Amp): Gate -> NAM capture -> cabinet IR.
// The gate is Off (default), the standard Noise Gate, Noise Gate (Smart) or Noise Gate (AI); a Smart
// Gate's learned profile is kept when another gate is tried in between. Captures and IRs
// come from <drive>/NAM Grid/Captures and /IRs, chosen by Folder and File.
#include "MPCEngine.h"

namespace mpc_plugins
{
inline const mpc_grid::PluginSpec& AmpSpec()
{
  using namespace mpc_grid;
  static const PluginSpec spec{
    "NG Amp", MPC_FOURCC('N', 'G', 'A', 'm'), "nam-grid-amp",
    {
      // Gate first in the signal path; off by default. Knob columns: the gate itself, then
      // the final gate and Mix.
      {"gate", "Gate",
       {{"off", "Off", SlotEffect::NAM},
        {"noise-gate", "Noise Gate", SlotEffect::NoiseGate},
        {"smart-gate", "Smart Gate", SlotEffect::NoisePrint, FileKind::None, -1, 0,
         {U(uNoiseSensitivity), U(uNoiseReduction), U(uNoiseAttack), U(uNoiseRelease), U(uNoiseGateThreshold),
          U(uNoiseGateRelease), U(uNoiseProtectAttack), U(uNoiseMix)}},
        {"ai-gate", "AI Gate", SlotEffect::AIGate, FileKind::None, -1, 0,
         {U(uAIGateReduction), U(uAIGateSensitivity), U(uAIGateAttackProtection), U(uAIGateMix),
          U(uNoiseGateThreshold), U(uNoiseGateRelease)}}},
       0},
      // A 5 Hz DC blocker follows the capture.
      {"amp", "Amp", {{"capture", "Capture", SlotEffect::NAM, FileKind::Capture}}, 0, false, true, nullptr, true},
      // The cabinet is off by default (many captures include one). Knob columns as on the
      // AMP page: levels and cuts, then mix and pre-delay.
      {"cab", "Cab",
       {{"cabinet-ir", "Cabinet IR", SlotEffect::CabinetIR, FileKind::IR, -1, 0,
         {U(uIRInput), parameter_layout::kSlotIRLevel, U(uIRLowCut), U(uIRHighCut), U(uIRMix), U(uIRPredelay)}}},
       0, true, false},
    },
    {}, // Programs would replace the chosen capture; the MPC's own presets keep it.
    true, // Mono: NAM captures are mono, so the gate and cabinet run once, not per channel.
  };
  return spec;
}
} // namespace mpc_plugins

MPC_GRID_PLUGIN(mpc_plugins::AmpSpec)
