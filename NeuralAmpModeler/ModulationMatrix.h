#pragma once
#include "IPlugParameter.h"
#include "EffectPresentation.h"
#include "FixedNAMGrid.h"
#include "ParameterLayout.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <string>
#include <type_traits>
#include <vector>

// Modulation matrix: any modulatable slot control or routing destination, by
// host parameter. Depth is a share of the destination's normalized range, so
// frequency and time controls move along the same curve as their knobs.
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

// Continuous controls only: switches, modes and Full Quality are not modulated.
inline bool Modulatable(int control)
{
  using namespace parameter_layout;
  if (control < 0 || control >= slotControls) return false;
  if (control < standardControls) return control != kSlotEnabled && control != kSlotFullQuality;
  const int p = control - standardControls;
  const auto& d = kEffectParameters[p];
  return EffectChoices(p).empty() && !IsAlgorithmMode(p) && !(d.minimum == 0. && d.maximum == 1. && d.step == 1.);
}
inline bool ValidDestination(int slot, int control)
{
  return (slot >= 0 && slot < gridSlot && Modulatable(control)) || (slot == gridSlot && control >= 0 && control < GridDestinations);
}
// 0..1 scale for Via: a bipolar source is centred so its middle halves the depth.
inline double Scale(int source, double value) { return Bipolar(source) ? (value + 1.) * .5 : value; }
// Amount a row adds to its destination's normalized value at these source values.
inline double RowAmount(const Values& v, int row, double source, double via)
{
  const int viaSource = static_cast<int>(v[RowParam(row, RowVia)]);
  return source * v[RowParam(row, RowDepth)] * .01 * (viaSource > SourceNone ? Scale(viaSource, via) : 1.);
}

class Matrix
{
public:
  struct Target
  {
    int slot = -1, control = -1;
    double offset = 0.;
  };
  using Targets = std::array<Target, rows>;
  // Host parameter curves for slot 0's controls (every slot shares them) and
  // the routing destinations. Bound once on the control thread.
  void Bind(const std::array<const iplug::IParam*, parameter_layout::slotControls>& slot,
            const std::array<const iplug::IParam*, GridDestinations>& grid)
  {
    mSlot = slot;
    mGrid = grid;
  }
  // Rows sharing a destination add before the result is bounded to its range.
  static int Collect(const Values& v, const Modulation& sources, Targets& targets)
  {
    int count = 0;
    for (int row = 0; row < rows; ++row)
    {
      if (!RowActive(v, row)) continue;
      const int slot = static_cast<int>(v[RowParam(row, RowSlot)]);
      const int control = static_cast<int>(v[RowParam(row, RowDestination)]);
      if (!ValidDestination(slot, control)) continue;
      const int source = static_cast<int>(v[RowParam(row, RowSource)]);
      const double amount =
        RowAmount(v, row, sources.Value(source, v), sources.Value(static_cast<int>(v[RowParam(row, RowVia)]), v));
      const auto end = targets.begin() + count;
      const auto match = std::find_if(targets.begin(), end, [&](const Target& t) { return t.slot == slot && t.control == control; });
      if (match != end) match->offset += amount;
      else targets[count++] = {slot, control, amount};
    }
    return count;
  }
  // Input stages before every dynamic destination have no dependency on the
  // changing sources. They can run ahead of the remaining control quanta, even
  // when an envelope listens later in the rig. Macros are constant per callback.
  static int IndependentInputPrefix(const Values& v)
  {
    int prefix = 4;
    for (int row = 0; row < rows; ++row)
    {
      if (!RowActive(v, row)) continue;
      const int slot = static_cast<int>(v[RowParam(row, RowSlot)]);
      const int control = static_cast<int>(v[RowParam(row, RowDestination)]);
      if (ValidDestination(slot, control)
          && (DynamicSource(static_cast<int>(v[RowParam(row, RowSource)]))
              || DynamicSource(static_cast<int>(v[RowParam(row, RowVia)])))) prefix = std::min(prefix, slot);
    }
    return prefix;
  }
  // Return modulated slot fields to their host values before this quantum's offsets.
  template <class Controls>
  static void Restore(Controls& animated, const Controls& base, const Targets& targets, int count)
  {
    for (int i = 0; i < count; ++i)
      if (const auto& t = targets[i]; t.slot < gridSlot)
        WriteControl(animated[t.slot], t.control, ReadControl(base[t.slot], t.control));
  }
  template <class Controls>
  void Apply(Controls& animated, double& blend, Routing& routing, const Targets& targets, int count) const
  {
    const auto move = [](const iplug::IParam* shape, double value, double offset) {
      return shape ? shape->FromNormalized(std::clamp(shape->ToNormalized(value) + offset, 0., 1.)) : value;
    };
    for (int i = 0; i < count; ++i)
    {
      const auto& t = targets[i];
      if (t.slot < gridSlot)
        WriteControl(animated[t.slot], t.control, move(mSlot[t.control], ReadControl(animated[t.slot], t.control), t.offset));
      else if (t.control == GridBlend)
        blend = std::clamp(blend + t.offset, 0., 1.);
      else if (t.control == GridLaneA)
        routing.gainA = move(mGrid[GridLaneA], routing.gainA, t.offset);
      else if (t.control == GridLaneB)
        routing.gainB = move(mGrid[GridLaneB], routing.gainB, t.offset);
      else if (t.control == GridSplit)
        routing.frequency = move(mGrid[GridSplit], routing.frequency, t.offset);
    }
  }

private:
  std::array<const iplug::IParam*, parameter_layout::slotControls> mSlot{};
  std::array<const iplug::IParam*, GridDestinations> mGrid{};
};

// Editor wording ----------------------------------------------------------

struct Destination
{
  int control, position;
  const char* label;
};
// The modulatable knobs a module shows, in editor order, with its own labels.
inline std::vector<Destination> Destinations(SlotEffect effect, bool loaded, bool sync, bool notch, bool vibrato, bool shelf = false)
{
  using namespace parameter_layout;
  std::vector<Destination> result;
  const bool nam = effect == SlotEffect::NAM;
  if (nam && loaded)
    result = {{kSlotIn, 0, "Input"}, {kSlotEQBass, 1, "Bass"}, {kSlotEQMid, 2, "Mid"}, {kSlotEQTreble, 3, "Treble"},
              {kSlotTrim, 4, "Output"}, {kSlotEQHighPass, 5, "Low cut"}, {kSlotEQLowPass, 6, "High cut"}};
  else if (nam) return result;
  if (effect == SlotEffect::NoiseGate)
    result = {{kSlotGateThreshold, 0, "Threshold"}, {kSlotGateRelease, 1, "Release"}, {kSlotGateReduction, 2, "Reduction"}};
  if (effect == SlotEffect::SmoothComp || effect == SlotEffect::PunchComp)
    result = {{kSlotCompThreshold, 0, "Threshold"}, {kSlotCompRelease, 1, "Release"}, {kSlotCompMakeup, 2, "Makeup"}};
  if (effect == SlotEffect::ToneEQ) result = {{kSlotEQBass, 0, "Bass"}, {kSlotEQMid, 1, "Mid"}, {kSlotEQTreble, 2, "Treble"}};
  if (effect == SlotEffect::CutEQ)
    result = {{kSlotEQHighPass, 0, "Low cut"}, {kSlotEQLowPass, 1, "High cut"}, {kSlotEQPresence, 2, "Presence"}};
  if (effect == SlotEffect::ParametricEQ) result = {{kSlotEQHighPass, 4, shelf ? "Low shelf" : "Low cut"}, {kSlotEQLowPass, 6, "High cut"}};
  if (effect == SlotEffect::Boost || effect == SlotEffect::TubeBoost) result = {{kSlotEQHighPass, 2, "Low cut"}};
  if (effect == SlotEffect::CabinetIR) result = {{kSlotIRLevel, effect_presentation::kOutputPosition, "Output"}};
  for (int p = 0; p < kUtilityControls; ++p)
    if (Modulatable(standardControls + p) && !effect_presentation::HeaderControl(p) && !effect_presentation::DisplayControl(p)
        && effect_presentation::Visible(effect, p, sync, notch, vibrato, shelf))
      result.push_back({standardControls + p, effect_presentation::Position(effect, p), effect_presentation::Label(effect, p)});
  std::stable_sort(result.begin(), result.end(), [](const auto& a, const auto& b) { return a.position < b.position; });
  return result;
}
// Matrix destination of a host parameter, or {-1, -1} when it cannot be modulated.
inline std::pair<int, int> DestinationOf(int id)
{
  if (const int slot = SlotControlOwner(id); slot >= 0)
  {
    const int control = parameter_layout::ControlOf(id);
    return Modulatable(control) ? std::pair{slot, control} : std::pair{-1, -1};
  }
  if (id == kParallelBlend) return {gridSlot, GridBlend};
  if (id == PerformanceParam(LaneAGain)) return {gridSlot, GridLaneA};
  if (id == PerformanceParam(LaneBGain)) return {gridSlot, GridLaneB};
  if (id == PerformanceParam(CrossoverFrequency)) return {gridSlot, GridSplit};
  return {-1, -1};
}
// A source's current value as the editor last saw it. False while unknown
// (audio stopped or modulation idle); macros are always known.
inline bool LiveValue(int source, const Values& v, const Modulation::Monitor& m, double& value)
{
  value = 0.;
  const auto& monitor = m.Instance(SourceInstance(source));
  switch (SourceKind(source))
  {
    case SourceNone: return true;
    case SourceLFO: value = monitor.lfo; return monitor.running;
    case SourceEnvelope: value = monitor.envelope; return monitor.running;
    case SourceSequencer: value = v[SourceParam(Seq1Step1, SourceInstance(source)) + std::clamp(monitor.step, 0, 7)] * .02 - 1.; return monitor.running;
    default: value = std::clamp(v[macros[std::clamp(source - SourceMacro1, 0, 3)]] * .01, 0., 1.); return true;
  }
}
// Normalized reach of every row on one destination, for the knob ring.
struct Ring
{
  int rows = 0, source = SourceNone;
  double low = 0., high = 0., live = 0.;
  bool showLive = false;
};
inline Ring DestinationRing(const Values& v, const Modulation::Monitor& m, int slot, int control)
{
  Ring ring;
  bool known = true;
  for (int row = 0; row < rows; ++row)
  {
    if (!RowActive(v, row) || static_cast<int>(v[RowParam(row, RowSlot)]) != slot
        || static_cast<int>(v[RowParam(row, RowDestination)]) != control)
      continue;
    const int source = static_cast<int>(v[RowParam(row, RowSource)]), via = static_cast<int>(v[RowParam(row, RowVia)]);
    if (!ring.rows++) ring.source = source;
    const double depth = v[RowParam(row, RowDepth)] * .01, low = depth * (Bipolar(source) ? -1. : 0.);
    // Via scales between none and all of the row, so its reach includes zero.
    ring.low += via > SourceNone ? std::min({0., low, depth}) : std::min(low, depth);
    ring.high += via > SourceNone ? std::max({0., low, depth}) : std::max(low, depth);
    double sourceValue = 0., viaValue = 0.;
    const bool sourceKnown = LiveValue(source, v, m, sourceValue), viaKnown = LiveValue(via, v, m, viaValue);
    known = known && sourceKnown && viaKnown;
    ring.live += RowAmount(v, row, sourceValue, viaValue);
  }
  ring.showLive = known && ring.rows > 0;
  return ring;
}
// Host-parameter wording for a slot control, independent of the loaded module.
inline const char* ControlName(int control)
{
  static constexpr const char* standard[]{"In", "Trim", "Enabled", "Gate Threshold", "Gate Release", "Gate Reduction",
                                          "Comp Threshold", "Comp Release", "Comp Makeup", "EQ Bass", "EQ Mid", "EQ Treble",
                                          "EQ Low Cut", "EQ High Cut", "EQ Presence", "IR Level", "Full Quality"};
  static_assert(std::size(standard) == standardControls);
  if (control < 0 || control >= parameter_layout::slotControls) return "?";
  return control < standardControls ? standard[control] : kEffectParameters[control - standardControls].name;
}
} // namespace grid_modulation
