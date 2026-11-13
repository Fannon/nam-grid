#pragma once
#include "ParameterLayout.h"
#include "FixedNAMGrid.h"
#include <array>
#include <cstddef>
#include <type_traits>

// The slot-control map shared by the host refresh, the modulation matrix and
// the MPC build. Plugin-framework free: it maps slot control indices to the
// fields of FixedNAMGrid::Controls.
namespace grid_modulation
{
using namespace grid_performance;
using parameter_layout::standardControls;

// Every slot control that the audio snapshot reads, by slot control index. The
// host refresh and the matrix share this one map, so they cannot disagree.
template <class C, class Visit>
void VisitSlotControls(C& c, Visit&& visit)
{
  using namespace parameter_layout;
  const auto u = [&](int p, auto& field) { visit(standardControls + p, field); };
  const auto family = [&](int first, auto& values) {
    for (size_t i = 0; i < values.size(); ++i) u(first + static_cast<int>(i), values[i]);
  };
  auto& d = c.dynamics;
  auto& e = c.eq;
  auto& t = c.utility;
  u(uIRInput, t.irInput);
  u(uIRMix, t.irMix);
  u(uIRPredelay, t.irPredelay);
  u(uIRLowCut, t.irLowCut);
  u(uIRHighCut, t.irHighCut);
  u(uIRLength, t.irLength);
  visit(kSlotEnabled, c.enabled);
  visit(kSlotIn, c.inputDB);
  visit(kSlotTrim, c.trimDB);
  visit(kSlotGateThreshold, d.gateThreshold);
  visit(kSlotGateRelease, d.gateRelease);
  visit(kSlotGateReduction, d.gateRange);
  u(uGateMode, d.gateMode);
  visit(kSlotCompThreshold, d.compThreshold);
  visit(kSlotCompRelease, d.compRelease);
  visit(kSlotCompMakeup, d.compMakeup);
  u(uCompAttack, d.compAttack);
  u(uPunchAttack, d.punchAttack);
  u(uCompMix, d.compMix);
  visit(kSlotEQBass, e.bass);
  visit(kSlotEQMid, e.mid);
  visit(kSlotEQTreble, e.treble);
  visit(kSlotEQHighPass, e.highPass);
  visit(kSlotEQLowPass, e.lowPass);
  visit(kSlotEQPresence, e.presence);
  visit(kSlotIRLevel, e.irLevel);
  u(uNoiseLearn, t.noise.learn);
  u(uNoiseSensitivity, t.noise.sensitivity);
  u(uNoiseReduction, t.noise.reduction);
  u(uNoiseAttack, t.noise.attack);
  u(uNoiseRelease, t.noise.release);
  u(uNoiseMix, t.noise.mix);
  u(uNoiseGateThreshold, t.noise.gateThreshold);
  u(uNoiseGateRelease, t.noise.gateRelease);
  u(uNoiseTrackingMode, t.noise.trackingMode);
  u(uNoiseProtectAttack, t.noise.protectAttack);
  u(uAIGateReduction, t.aiGate.reduction);
  u(uAIGateSensitivity, t.aiGate.sensitivity);
  u(uAIGateMix, t.aiGate.mix);
  u(uAIGateAttackProtection, t.aiGate.attackProtection);
  u(uTempoSync, t.sync);
  u(uTempoDivision, t.division);
  u(uUtilityLevel, t.level);
  u(uUtilityBalance, t.balance);
  u(uUtilityStereo, t.stereo);
  u(uUtilityInvertL, t.invertL);
  u(uUtilityInvertR, t.invertR);
  u(uDelayPingPong, t.pingPong);
  u(uChorusSpread, t.chorusSpread);
  u(uReverbStereo, t.stereoReverb);
  family(uSpringDecay, t.spring);
  family(uWahPosition, t.wah);
  u(uFrequency, t.frequency);
  u(uGain, t.gain);
  u(uQ, t.q);
  u(uParamLowQ, t.paramLowQ);
  u(uParamHighQ, t.paramHighQ);
  u(uParamShelfGain, t.paramShelfGain);
  u(uParamLowMode, t.paramLowMode);
  u(uBoostGain, t.boost);
  u(uBoostTilt, t.tilt);
  u(uFilterCutoff, t.cutoff);
  u(uFilterResonance, t.resonance);
  u(uFilterEnvelope, t.envelope);
  u(uFilterMode, t.filterMode);
  u(uSwellRise, t.rise);
  u(uSwellThreshold, t.threshold);
  u(uSwellRelease, t.release);
  family(uDelayTime, t.delay);
  family(uReverbDecay, t.reverb);
  family(uModRate, t.modulation);
  family(uTapeDrive, t.tape);
  u(uDelayMotion, t.delayMotion);
  u(uDelayOutput, t.delayOutput);
  u(uReverbMotion, t.reverbMotion);
  u(uReverbHighPass, t.reverbHighPass);
  family(uTubeDrive, t.tube);
  u(uUtilityDelay, t.fineDelay);
  u(uVibeMode, t.vibeMode);
  u(uEQMode, t.eqMode);
  family(uRotarySlow, t.rotary);
  family(uLimiterCeiling, t.limiter);
  u(uLimiterClip, t.limiterClip);
  family(uOctDry, t.octaver);
  family(uDoubleDetune, t.doubler);
  family(uFreezeHold, t.freeze);
  family(uStutterTime, t.stutter);
  family(uCrushBits, t.crusher);
  family(uRingFrequency, t.ring);
  family(uDualRatio, t.dual);
  family(uJunoMode, t.juno);
  family(uShimmerMix, t.shimmer);
  family(uShiftFrequency, t.shift);
  family(uDistStyle, t.distortion);
  u(uDistLowPreserve, t.distortionLowPreserve);
  family(uLoFiSpeed, t.lofi);
  u(uTapeHeads, t.tapeHeads);
  u(uDelayDiffusion, t.delayDiffusion);
  family(uAirPlateInput, t.airPlate);
  family(uAirHallSustain, t.airHall);
  family(uSpaceReplace, t.space);
  family(uCosmicTime, t.cosmic);
  u(uCosmicStyle, t.cosmicStyle);
  family(uStripLowCut, t.strip);
  family(uColourInput, t.colour);
  family(uConsoleTreble, t.consoleEQ);
  family(uButterCompress, t.butter);
  u(uColourDesk, t.colourDesk);
  family(uBlackThreshold, t.black);
  u(uBlackMode, t.blackMode);
  u(uBlackHold, t.blackHold);
  family(uPedalStyle, t.pedal);
  family(uPDistStyle, t.pedalDistortion);
  u(uToneMidFrequency, e.midFrequency);
  u(uEQOutput, t.extra.eqOutput);
  u(uBoostMidGain, t.extra.boostMidGain);
  u(uTubeHighCut, t.extra.tubeHighCut);
  u(uGateAttack, d.gateAttack);
  u(uGateHold, d.gateHold);
  u(uGateHysteresis, d.gateHysteresis);
  u(uSmoothRatio, d.smoothRatio);
  u(uPunchRatio, d.punchRatio);
  u(uCompDetectorCut, d.detectorCut);
  u(uStripInput, t.extra.stripInput);
  u(uLimiterInput, t.extra.limiterInput);
  u(uChorusDelay, t.extra.chorusDelay);
  u(uJunoWidth, t.extra.junoWidth);
  u(uFlangerDelay, t.extra.flangerDelay);
  u(uModSpread, t.extra.modSpread);
  u(uPhaserStages, t.extra.phaserStages);
  u(uVibratoRise, t.extra.vibratoRise);
  u(uRotaryBalance, t.extra.rotaryBalance);
  u(uVibeSpread, t.extra.vibeSpread);
  u(uRingShape, t.extra.ringShape);
  u(uOctTrackingFilter, t.extra.octTrackingFilter);
  u(uDoubleHumanize, t.extra.doubleHumanize);
  u(uDoubleHighCut, t.extra.doubleHighCut);
  u(uDuckRelease, t.extra.duckRelease);
  u(uReverseFeedback, t.extra.reverseFeedback);
  u(uReverbDuck, t.extra.reverbDuck);
  u(uSpringPredelay, t.extra.springPredelay);
  u(uFreezeAttack, t.extra.freezeAttack);
  u(uFreezeRelease, t.extra.freezeRelease);
  u(uStutterHold, t.extra.stutterHold);
  u(uLoFiDrive, t.extra.lofiDrive);
  u(uUtilityMode, t.extra.utilityMode);
  u(uUtilitySkew, t.extra.utilitySkew);
  u(uSwellCurve, t.extra.swellCurve);
  u(uSwellHold, t.extra.swellHold);
  u(uSwellMix, t.extra.swellMix);
  u(uDelayTimeChange, t.extra.delayTimeChange);
  family(uDynamicEQFrequency, t.dynamicEQ);
  family(uTransientShaperAttack, t.transientShaper);
  u(uHarmonicCrossover, t.extra.harmonicCrossover);
  family(uMultibandDepth, t.multiband);
  family(uMultibandLowSplit, t.multibandShape);
  family(uPitchInterval, t.pitchShift);
  family(uGraphicBand1, t.graphicEQ);
  u(uGraphicVoicing, t.graphicVoicing);
  family(uPedalCompSustain, t.pedalComp);
  u(uPedalCompPickup, t.pedalCompPickup);
  u(uChorusVoices, t.extra.chorusVoices);
  u(uSpringTank, t.extra.springTank);

}
// Host parameter value -> snapshot field, matching IParam::Bool() and Int().
template <class T>
T FieldValue(double value)
{
  if constexpr (std::is_same_v<T, bool>) return value >= .5;
  else return static_cast<T>(value);
}
// Where each control's field sits in the snapshot, taken once from the map so
// the matrix reaches a field directly instead of scanning every control per
// quantum. Nested members and array elements have no single member pointer,
// so the table keeps byte offsets measured on a real snapshot. It is built
// during static initialisation, never on the audio thread.
template <class C>
class ControlFields
{
public:
  enum Type : unsigned char { None, Double, Int, Bool };
  ControlFields()
  {
    const C c{};
    const auto* base = reinterpret_cast<const unsigned char*>(&c);
    VisitSlotControls(c, [&](int i, const auto& field) {
      using T = std::remove_cv_t<std::remove_reference_t<decltype(field)>>;
      static_assert(std::is_same_v<T, double> || std::is_same_v<T, int> || std::is_same_v<T, bool>, "Add a field type here");
      mOffset[i] = static_cast<size_t>(reinterpret_cast<const unsigned char*>(&field) - base);
      mType[i] = std::is_same_v<T, double> ? Double : std::is_same_v<T, int> ? Int : Bool;
    });
  }
  Type TypeOf(int control) const { return Mapped(control) ? mType[control] : None; }
  double Read(const C& c, int control) const
  {
    if (!Mapped(control)) return 0.;
    const auto* field = reinterpret_cast<const unsigned char*>(&c) + mOffset[control];
    switch (mType[control])
    {
      case Double: return *reinterpret_cast<const double*>(field);
      case Int: return static_cast<double>(*reinterpret_cast<const int*>(field));
      case Bool: return static_cast<double>(*reinterpret_cast<const bool*>(field));
      default: return 0.;
    }
  }
  void Write(C& c, int control, double value) const
  {
    if (!Mapped(control)) return;
    auto* field = reinterpret_cast<unsigned char*>(&c) + mOffset[control];
    switch (mType[control])
    {
      case Double: *reinterpret_cast<double*>(field) = FieldValue<double>(value); break;
      case Int: *reinterpret_cast<int*>(field) = FieldValue<int>(value); break;
      case Bool: *reinterpret_cast<bool*>(field) = FieldValue<bool>(value); break;
      default: break;
    }
  }

private:
  bool Mapped(int control) const { return control >= 0 && control < parameter_layout::slotControls && mType[control] != None; }
  std::array<size_t, parameter_layout::slotControls> mOffset{};
  std::array<Type, parameter_layout::slotControls> mType{};
};
template <class C>
inline const ControlFields<C> controlFields;

// Unmapped controls read as zero and ignore writes.
template <class C>
double ReadControl(const C& c, int control)
{
  return controlFields<C>.Read(c, control);
}
template <class C>
void WriteControl(C& c, int control, double value)
{
  controlFields<C>.Write(c, control, value);
}
} // namespace grid_modulation
