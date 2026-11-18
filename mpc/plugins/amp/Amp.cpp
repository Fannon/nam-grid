// NAM Grid Amp for MPC (shown as NG Amp): Gate -> Pedal -> NAM capture -> cabinet IR.
// The gate is Off (default), the standard Noise Gate, Noise Gate (Smart) or Noise Gate (AI); a Smart
// Gate's learned profile is kept when another gate is tried in between. Captures and IRs
// come from <drive>/NAM Grid/Captures and /IRs, chosen by Folder and File.
#include "MPCEngine.h"

namespace mpc_plugins
{
inline const mpc_grid::PluginSpec& AmpSpec()
{
  using namespace mpc_grid;
  static const PluginSpec spec = [] {
    std::vector<Choice> pedals{{"off", "Off", SlotEffect::NAM}, {"pedal-comp", "Pedal Comp", SlotEffect::PedalComp}};
    for (int i = 0; i < effect_modes::pedalCount; ++i)
      pedals.push_back({"pedal-drive", effect_modes::pedalNames[i], SlotEffect::PedalDrive,
                        FileKind::None, U(uPedalStyle), i});
    return PluginSpec{
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
         0, false, true, nullptr, false, {U(uGateMode), U(uNoiseLearn), U(uNoiseTrackingMode)}},
        // A 5 Hz DC blocker follows the capture.
        {"amp", "Amp", {{"capture", "Capture", SlotEffect::NAM, FileKind::Capture}}, 0, false, true, nullptr, true,
         {parameter_layout::kSlotFullQuality}},
        // The cabinet is off by default (many captures include one). Knob columns as on the
        // AMP page: levels and cuts, then mix and pre-delay.
        {"cab", "Cab",
         {{"cabinet-ir", "Cabinet IR", SlotEffect::CabinetIR, FileKind::IR, -1, 0,
           {U(uIRInput), parameter_layout::kSlotIRLevel, U(uIRLowCut), U(uIRHighCut), U(uIRMix), U(uIRPredelay)}}},
         0, true, false, nullptr, false, {U(uIRLength)}},
        {"pedal", "Pedal", std::move(pedals), 0, false, true, "Pedal", false, {U(uPedalCompPickup)}},
      },
      {}, // Programs would replace the chosen capture; the MPC's own presets keep it.
      true, // Mono: NAM captures are mono, so the gate and cabinet run once, not per channel.
      {0, 3, 1, 2}, // Gate, pedal, capture, cabinet.
      3, // Gate, Amp, Cab and library display parameters keep their host indices.
    };
  }();
  return spec;
}
} // namespace mpc_plugins

MPC_GRID_PLUGIN(mpc_plugins::AmpSpec)
