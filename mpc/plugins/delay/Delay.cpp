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
       {"dual-delay", "Dual", SlotEffect::DualDelay},
       {"space-tape-echo", "Space Tape Echo", SlotEffect::SpaceTapeEcho},
       {"bucket-brigade", "Bucket Brigade", SlotEffect::BucketBrigade},
       {"pitch-delay", "Pitch Delay", SlotEffect::PitchDelay},
       {"feedback-fx-delay", "Feedback FX", SlotEffect::FeedbackFXDelay},
       {"pattern-delay", "Pattern", SlotEffect::PatternDelay},
       {"granular-delay", "Granular", SlotEffect::GranularDelay}},
      1, false, true, "Type", false, {U(uTempoSync), U(uDelayPingPong), U(uDelayTimeChange), U(uTapeHeads),
        U(uDigitalDelayVoice), U(uSpaceHeads), U(uBBDVoice), U(uPitchDelayMode), U(uFeedbackFX), U(uPatternFeedback),
        U(uGrainDelayMode), U(uEchoRateSync), U(uEchoInterval), U(uTapTime1), U(uTapTime2), U(uTapTime3), U(uTapTime4),
        U(uTapLevel1), U(uTapLevel2), U(uTapLevel3), U(uTapLevel4), U(uTapPan1), U(uTapPan2), U(uTapPan3), U(uTapPan4)}, 7}},
    {{"Digital", {0}}, {"Analog", {1}}, {"Tape", {2}}, {"Multi-Head", {3}}, {"Diffused", {4}}, {"Reverse", {5}},
     {"Dual", {6}}, {"Space Tape Echo", {7}}, {"Bucket Brigade", {8}}, {"Pitch Delay", {9}},
     {"Feedback FX", {10}}, {"Pattern", {11}}, {"Granular", {12}}},
  };
  return spec;
}
} // namespace mpc_plugins

MPC_GRID_PLUGIN(mpc_plugins::DelaySpec)
