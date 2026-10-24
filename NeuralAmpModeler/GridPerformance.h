#pragma once
#include "EffectParameters.h"
#include "TempoSync.h"
#include <atomic>
#include <cstdint>

namespace grid_performance
{
enum Param
{
  // Persistent IDs: append only. Never renumber or reuse retired entries.
  Crossover = 0,
  CrossoverFrequency = 1,
  LaneAGain = 2,
  LaneBGain = 3,
  LaneAMute = 4,
  LaneBMute = 5,
  LaneASolo = 6,
  LaneBSolo = 7,
  LaneTiming = 8,
  // Fixed 0.1 assignments (slot / target / depth per source). Still applied for
  // host automation; older states convert them to matrix rows on load.
  LFOSlot = 9,
  LFOTarget = 10,
  LFORate = 11,
  LFODepth = 12,
  LFOSync = 13,
  LFODivision = 14,
  EnvSlot = 15,
  EnvTarget = 16,
  EnvSensitivity = 17,
  EnvAttack = 18,
  EnvRelease = 19,
  EnvDepth = 20,
  StepSlot = 21,
  StepTarget = 22,
  StepRate = 23,
  StepDepth = 24,
  StepSync = 25,
  StepDivision = 26,
  Step1 = 27,
  Step2 = 28,
  Step3 = 29,
  Step4 = 30,
  Step5 = 31,
  Step6 = 32,
  Step7 = 33,
  Step8 = 34,
  Macro1 = 35,
  Macro1Slot = 36,
  Macro1Target = 37,
  Macro1Depth = 38,
  Macro2 = 39,
  Macro2Slot = 40,
  Macro2Target = 41,
  Macro2Depth = 42,
  InputGain = 43,
  LFOShape = 44,
  Macro3 = 45,
  Macro4 = 46,
  // Eight modulation matrix rows of RowFields controls each; see RowParam().
  ModRow = 47,
  Count = ModRow + 8 * 5,
};
enum RowField
{
  RowSource = 0,
  RowSlot,        // Pre 1 ... Post 4, then gridSlot for routing destinations.
  RowDestination, // Slot control index, or GridDestination for gridSlot.
  RowDepth,
  RowVia,         // Optional source that scales the depth (0 to 1).
  RowFields
};
inline constexpr int rows = 8, gridSlot = 16;
constexpr int RowParam(int row, int field) { return ModRow + row * RowFields + field; }
static_assert(RowParam(rows, 0) == Count);
enum Source
{
  SourceNone = 0,
  SourceLFO,
  SourceEnvelope,
  SourceSequencer,
  SourceMacro1,
  SourceMacro2,
  SourceMacro3,
  SourceMacro4,
  SourceCount
};
inline constexpr std::array<const char*, SourceCount> sourceNames{
  "None", "LFO", "Envelope", "Sequencer", "Macro 1", "Macro 2", "Macro 3", "Macro 4"};
// The LFO and sequencer swing both ways; the follower and macros only add.
inline constexpr bool Bipolar(int source) { return source == SourceLFO || source == SourceSequencer; }
inline constexpr std::array<int, 4> macros{Macro1, Macro2, Macro3, Macro4};
enum GridDestination
{
  GridBlend = 0,
  GridLaneA,
  GridLaneB,
  GridSplit,
  GridDestinations
};
inline constexpr std::array<const char*, GridDestinations> gridDestinationNames{
  "Blend A / B", "Lane A level", "Lane B level", "Split frequency"};
inline constexpr std::array<const char*, 6> lfoShapeNames{"Sine", "Triangle", "Saw up", "Saw down", "Square", "Random"};
inline constexpr std::array<const char*, 9> targetNames{
  "None", "Wah position", "Filter cutoff", "L/R balance", "Delay mix", "Reverb mix", "NAM trim", "Parallel blend", "Ring frequency"};
inline constexpr std::array<EffectParameter, Count> parameters{{
  {"Split Crossover", "Frequency split", 0, 0, 1, 1, "", 0},
  {"Split Frequency", "Crossover", 250, 40, 5000, 1, "Hz", 0, true},
  {"Lane A Level", "A level", 0, -60, 12, .1, "dB", 0},
  {"Lane B Level", "B level", 0, -60, 12, .1, "dB", 0},
  {"Lane A Mute", "Mute A", 0, 0, 1, 1, "", 0},
  {"Lane B Mute", "Mute B", 0, 0, 1, 1, "", 0},
  {"Lane A Solo", "Solo A", 0, 0, 1, 1, "", 0},
  {"Lane B Solo", "Solo B", 0, 0, 1, 1, "", 0},
  {"Lane Fine Timing", "Lane timing", 0, -20, 20, .01, "ms", 0},
  {"LFO Slot", "Target slot", 0, 0, 15, 1, "", 0},
  {"LFO Target", "Target", 0, 0, 8, 1, "", 0},
  {"LFO Rate", "Rate", .5, .05, 10, .01, "Hz", 0, true},
  {"LFO Depth", "Depth", 0, -100, 100, .1, "%", 0},
  {"LFO Sync", "Tempo sync", 0, 0, 1, 1, "", 0},
  {"LFO Division", "Division", 5, 0, 9, 1, "", 0},
  {"Follower Slot", "Target slot", 0, 0, 15, 1, "", 0},
  {"Follower Target", "Target", 0, 0, 8, 1, "", 0},
  {"Follower Sensitivity", "Sensitivity", 12, 0, 36, .1, "dB", 0},
  {"Follower Attack", "Attack", 10, .5, 100, .1, "ms", 0, true},
  {"Follower Release", "Release", 150, 10, 1000, 1, "ms", 0, true},
  {"Follower Depth", "Depth", 0, -100, 100, .1, "%", 0},
  {"Sequencer Slot", "Target slot", 0, 0, 15, 1, "", 0},
  {"Sequencer Target", "Target", 0, 0, 8, 1, "", 0},
  {"Sequencer Rate", "Step rate", 4, .1, 20, .01, "Hz", 0, true},
  {"Sequencer Depth", "Depth", 0, -100, 100, .1, "%", 0},
  {"Sequencer Sync", "Tempo sync", 1, 0, 1, 1, "", 0},
  {"Sequencer Division", "Division", 1, 0, 9, 1, "", 0},
  {"Sequence Step 1", "1", 0, 0, 100, 1, "%", 0},
  {"Sequence Step 2", "2", 100, 0, 100, 1, "%", 0},
  {"Sequence Step 3", "3", 25, 0, 100, 1, "%", 0},
  {"Sequence Step 4", "4", 75, 0, 100, 1, "%", 0},
  {"Sequence Step 5", "5", 0, 0, 100, 1, "%", 0},
  {"Sequence Step 6", "6", 100, 0, 100, 1, "%", 0},
  {"Sequence Step 7", "7", 50, 0, 100, 1, "%", 0},
  {"Sequence Step 8", "8", 75, 0, 100, 1, "%", 0},
  {"Macro One", "Macro 1", 0, 0, 100, .1, "%", 0},
  {"Macro One Slot", "Target slot", 0, 0, 15, 1, "", 0},
  {"Macro One Target", "Target", 0, 0, 8, 1, "", 0},
  {"Macro One Depth", "Depth", 0, -100, 100, .1, "%", 0},
  {"Macro Two", "Macro 2", 0, 0, 100, .1, "%", 0},
  {"Macro Two Slot", "Target slot", 0, 0, 15, 1, "", 0},
  {"Macro Two Target", "Target", 0, 0, 8, 1, "", 0},
  {"Macro Two Depth", "Depth", 0, -100, 100, .1, "%", 0},
  {"Global Input", "Input", 0, -40, 40, .1, "dB", 0},
  {"LFO Shape", "Shape", 0, 0, 5, 1, "", 0, false, 1., 3},
  {"Macro Three", "Macro 3", 0, 0, 100, .1, "%", 0, false, 1., 3},
  {"Macro Four", "Macro 4", 0, 0, 100, .1, "%", 0, false, 1., 3},
  {"Mod 1 Source", "Source", 0, 0, 7, 1, "", 0, false, 1., 3},
  {"Mod 1 Slot", "Slot", 0, 0, 16, 1, "", 0, false, 1., 3},
  {"Mod 1 Destination", "Destination", 0, 0, 1023, 1, "", 0, false, 1., 3},
  {"Mod 1 Depth", "Depth", 0, -100, 100, .1, "%", 0, false, 1., 3},
  {"Mod 1 Via", "Via", 0, 0, 7, 1, "", 0, false, 1., 3},
  {"Mod 2 Source", "Source", 0, 0, 7, 1, "", 0, false, 1., 3},
  {"Mod 2 Slot", "Slot", 0, 0, 16, 1, "", 0, false, 1., 3},
  {"Mod 2 Destination", "Destination", 0, 0, 1023, 1, "", 0, false, 1., 3},
  {"Mod 2 Depth", "Depth", 0, -100, 100, .1, "%", 0, false, 1., 3},
  {"Mod 2 Via", "Via", 0, 0, 7, 1, "", 0, false, 1., 3},
  {"Mod 3 Source", "Source", 0, 0, 7, 1, "", 0, false, 1., 3},
  {"Mod 3 Slot", "Slot", 0, 0, 16, 1, "", 0, false, 1., 3},
  {"Mod 3 Destination", "Destination", 0, 0, 1023, 1, "", 0, false, 1., 3},
  {"Mod 3 Depth", "Depth", 0, -100, 100, .1, "%", 0, false, 1., 3},
  {"Mod 3 Via", "Via", 0, 0, 7, 1, "", 0, false, 1., 3},
  {"Mod 4 Source", "Source", 0, 0, 7, 1, "", 0, false, 1., 3},
  {"Mod 4 Slot", "Slot", 0, 0, 16, 1, "", 0, false, 1., 3},
  {"Mod 4 Destination", "Destination", 0, 0, 1023, 1, "", 0, false, 1., 3},
  {"Mod 4 Depth", "Depth", 0, -100, 100, .1, "%", 0, false, 1., 3},
  {"Mod 4 Via", "Via", 0, 0, 7, 1, "", 0, false, 1., 3},
  {"Mod 5 Source", "Source", 0, 0, 7, 1, "", 0, false, 1., 3},
  {"Mod 5 Slot", "Slot", 0, 0, 16, 1, "", 0, false, 1., 3},
  {"Mod 5 Destination", "Destination", 0, 0, 1023, 1, "", 0, false, 1., 3},
  {"Mod 5 Depth", "Depth", 0, -100, 100, .1, "%", 0, false, 1., 3},
  {"Mod 5 Via", "Via", 0, 0, 7, 1, "", 0, false, 1., 3},
  {"Mod 6 Source", "Source", 0, 0, 7, 1, "", 0, false, 1., 3},
  {"Mod 6 Slot", "Slot", 0, 0, 16, 1, "", 0, false, 1., 3},
  {"Mod 6 Destination", "Destination", 0, 0, 1023, 1, "", 0, false, 1., 3},
  {"Mod 6 Depth", "Depth", 0, -100, 100, .1, "%", 0, false, 1., 3},
  {"Mod 6 Via", "Via", 0, 0, 7, 1, "", 0, false, 1., 3},
  {"Mod 7 Source", "Source", 0, 0, 7, 1, "", 0, false, 1., 3},
  {"Mod 7 Slot", "Slot", 0, 0, 16, 1, "", 0, false, 1., 3},
  {"Mod 7 Destination", "Destination", 0, 0, 1023, 1, "", 0, false, 1., 3},
  {"Mod 7 Depth", "Depth", 0, -100, 100, .1, "%", 0, false, 1., 3},
  {"Mod 7 Via", "Via", 0, 0, 7, 1, "", 0, false, 1., 3},
  {"Mod 8 Source", "Source", 0, 0, 7, 1, "", 0, false, 1., 3},
  {"Mod 8 Slot", "Slot", 0, 0, 16, 1, "", 0, false, 1., 3},
  {"Mod 8 Destination", "Destination", 0, 0, 1023, 1, "", 0, false, 1., 3},
  {"Mod 8 Depth", "Depth", 0, -100, 100, .1, "%", 0, false, 1., 3},
  {"Mod 8 Via", "Via", 0, 0, 7, 1, "", 0, false, 1., 3},
}};
// Matrix routing choices are saved state, like algorithm selectors: no host automation,
// so a longer source or destination list never reinterprets recorded automation.
inline constexpr bool StateOnly(int parameter)
{
  if (parameter < ModRow || parameter >= Count) return false;
  const int field = (parameter - ModRow) % RowFields;
  return field != RowDepth;
}
using Values = std::array<double, Count>;
inline Values Defaults()
{
  Values v{}; for (int i = 0; i < Count; ++i) v[i] = parameters[i].initial; return v;
}
struct Routing
{
  bool crossover = false, muteA = false, muteB = false, soloA = false, soloB = false;
  double frequency = 250., gainA = 0., gainB = 0., timing = 0.;
};
inline Routing Route(const Values& v)
{ return {v[Crossover] >= .5, v[LaneAMute] >= .5, v[LaneBMute] >= .5, v[LaneASolo] >= .5, v[LaneBSolo] >= .5,
          v[CrossoverFrequency], v[LaneAGain], v[LaneBGain], v[LaneTiming]}; }
inline bool RowActive(const Values& v, int row)
{ return v[RowParam(row, RowSource)] > 0. && v[RowParam(row, RowDepth)] != 0.; }
// One LFO cycle at phase 0..1; Random is held per cycle elsewhere.
inline double LFOWave(int shape, double phase)
{
  switch (shape)
  {
    case 1: return 1. - 4. * std::abs(std::fmod(phase + .25, 1.) - .5);
    case 2: return 2. * phase - 1.;
    case 3: return 1. - 2. * phase;
    case 4: return phase < .5 ? 1. : -1.;
    default: return std::sin(6.283185307179586 * phase);
  }
}

// Shared sources. Values are held for each 16-sample control quantum.
class Modulation
{
public:
  // Latest source values for the editor; written once per audio block.
  struct Monitor
  {
    float phase = 0.f, lfo = 0.f, envelope = 0.f;
    int step = 0;
    bool running = false;
  };
  void Reset(double rate)
  {
    mRate = rate; mPhase = mStepPhase = mEnvelope = mHeldPhase = mHeldEnvelope = 0.; mStep = mHeldStep = 0; mFrames = 0;
    mRandom = mHeldRandom = 0.; mSeed = 0x2545F491u;
    mMonitorRunning.store(false, std::memory_order_relaxed);
  }
  int Quantum() const { return 16 - static_cast<int>(mFrames % 16); }
  bool LegacyActive(const Values& v) const
  {
    return (v[LFOTarget] > 0. && v[LFODepth] != 0.) || (v[EnvTarget] > 0. && v[EnvDepth] != 0.)
        || (v[StepTarget] > 0. && v[StepDepth] != 0.) || (v[Macro1Target] > 0. && v[Macro1Depth] != 0.)
        || (v[Macro2Target] > 0. && v[Macro2Depth] != 0.);
  }
  bool Active(const Values& v) const
  {
    for (int row = 0; row < rows; ++row)
      if (RowActive(v, row)) return true;
    return LegacyActive(v);
  }
  void Advance(const double* l, const double* r, int n, const Values& v, double tempo)
  {
    const double lfo = v[LFOSync] >= .5 ? 1. / grid_tempo::Seconds(tempo, static_cast<int>(v[LFODivision])) : v[LFORate];
    const double steps = v[StepSync] >= .5 ? 1. / grid_tempo::Seconds(tempo, static_cast<int>(v[StepDivision])) : v[StepRate];
    const double sensitivity = std::pow(10., v[EnvSensitivity] * .05);
    const double attack = std::exp(-1. / (.001 * v[EnvAttack] * mRate));
    const double release = std::exp(-1. / (.001 * v[EnvRelease] * mRate));
    for (int i = 0; i < n; ++i)
    {
      const double peak = std::max(std::abs(l[i]), r ? std::abs(r[i]) : 0.) * sensitivity;
      mEnvelope = peak + (peak > mEnvelope ? attack : release) * (mEnvelope - peak);
      const double phase = mPhase + lfo / mRate;
      if (phase >= 1.)
      {
        // xorshift32: a new held value per Random cycle, without allocation or locks.
        mSeed ^= mSeed << 13; mSeed ^= mSeed >> 17; mSeed ^= mSeed << 5;
        mRandom = mSeed * (2. / 4294967295.) - 1.;
      }
      mPhase = std::fmod(phase, 1.);
      mStepPhase += steps / mRate;
      if (mStepPhase >= 1.) { mStepPhase -= 1.; mStep = (mStep + 1) % 8; }
      if (++mFrames % 16 == 0) { mHeldPhase = mPhase; mHeldEnvelope = mEnvelope; mHeldStep = mStep; mHeldRandom = mRandom; }
    }
    mMonitorPhase.store(static_cast<float>(mHeldPhase), std::memory_order_relaxed);
    mMonitorLFO.store(static_cast<float>(LFOValue(v)), std::memory_order_relaxed);
    mMonitorEnvelope.store(static_cast<float>(std::clamp(mHeldEnvelope, 0., 1.)), std::memory_order_relaxed);
    mMonitorStep.store(mHeldStep, std::memory_order_relaxed);
    mMonitorRunning.store(true, std::memory_order_relaxed);
  }
  // Editor only: stop drawing live values once processing has stopped or no source is used.
  void Idle() { mMonitorRunning.store(false, std::memory_order_relaxed); }
  Monitor Read() const
  {
    return {mMonitorPhase.load(std::memory_order_relaxed), mMonitorLFO.load(std::memory_order_relaxed),
            mMonitorEnvelope.load(std::memory_order_relaxed), mMonitorStep.load(std::memory_order_relaxed),
            mMonitorRunning.load(std::memory_order_relaxed)};
  }
  double LFOValue(const Values& v) const
  {
    const int shape = static_cast<int>(v[LFOShape]);
    return shape == 5 ? mHeldRandom : LFOWave(shape, mHeldPhase);
  }
  // Held value of a source: -1..1 for the LFO and sequencer, 0..1 otherwise.
  double Value(int source, const Values& v) const
  {
    switch (source)
    {
      case SourceLFO: return LFOValue(v);
      case SourceEnvelope: return std::clamp(mHeldEnvelope, 0., 1.);
      case SourceSequencer: return v[Step1 + mHeldStep] * .02 - 1.;
      case SourceMacro1: case SourceMacro2: case SourceMacro3: case SourceMacro4:
        return std::clamp(v[macros[source - SourceMacro1]] * .01, 0., 1.);
      default: return 0.;
    }
  }
  // Apply() edits at most these slots. Restoring them from the host values gives
  // each quantum the same input as copying every slot again.
  template<class Controls>
  void Restore(Controls& animated, const Controls& base, const Values& v) const
  {
    for (const int slotIndex : {LFOSlot, EnvSlot, StepSlot, Macro1Slot, Macro2Slot})
    {
      const int slot = std::clamp(static_cast<int>(v[slotIndex]), 0, 15);
      animated[slot] = base[slot];
    }
  }
  // The fixed 0.1 assignments. Matrix rows are applied by grid_modulation::Matrix.
  template<class Controls>
  void Apply(Controls& controls, double& blend, const Values& v) const
  {
    const auto assign = [&](int slotIndex, int targetIndex, double amount) {
      const int slot = std::clamp(static_cast<int>(v[slotIndex]), 0, 15);
      auto& c = controls[slot]; auto& u = c.utility;
      switch (static_cast<int>(v[targetIndex]))
      {
        case 1: u.wah[0] = std::clamp(u.wah[0] + amount * 100., 0., 100.); break;
        case 2: u.cutoff = std::clamp(u.cutoff * std::exp2(amount * 6.), 40., 12000.); break;
        case 3: u.balance = std::clamp(u.balance + amount * 100., -100., 100.); break;
        case 4: u.delay[4] = std::clamp(u.delay[4] + amount * 100., 0., 100.); break;
        case 5:
          u.reverb[4] = std::clamp(u.reverb[4] + amount * 100., 0., 100.);
          u.airPlate[4] = std::clamp(u.airPlate[4] + amount * 100., 0., 100.);
          u.airHall[5] = std::clamp(u.airHall[5] + amount * 100., 0., 100.);
          u.space[5] = std::clamp(u.space[5] + amount * 100., 0., 100.);
          u.cosmic[6] = std::clamp(u.cosmic[6] + amount * 100., 0., 100.);
          u.shimmer[0] = std::clamp(u.shimmer[0] + amount * 100., 0., 100.);
          u.spring[3] = std::clamp(u.spring[3] + amount * 100., 0., 100.); break;
        case 6: c.trimDB = std::clamp(c.trimDB + amount * 24., -40., 40.); break;
        case 7: blend = std::clamp(blend + amount, 0., 1.); break;
        case 8: u.ring[0] = std::clamp(u.ring[0] * std::exp2(amount * 4.), 1., 5000.); break;
        default: break;
      }
    };
    assign(LFOSlot, LFOTarget, Value(SourceLFO, v) * v[LFODepth] * .01);
    assign(EnvSlot, EnvTarget, Value(SourceEnvelope, v) * v[EnvDepth] * .01);
    assign(StepSlot, StepTarget, Value(SourceSequencer, v) * v[StepDepth] * .01);
    assign(Macro1Slot, Macro1Target, v[Macro1] * v[Macro1Depth] * .0001);
    assign(Macro2Slot, Macro2Target, v[Macro2] * v[Macro2Depth] * .0001);
  }
private:
  double mRate = 48000., mPhase = 0., mStepPhase = 0., mEnvelope = 0.;
  int mStep = 0;
  double mHeldPhase = 0., mHeldEnvelope = 0.;
  int mHeldStep = 0;
  double mRandom = 0., mHeldRandom = 0.;
  uint32_t mSeed = 0x2545F491u;
  uint64_t mFrames = 0;
  std::atomic<float> mMonitorPhase{0.f}, mMonitorLFO{0.f}, mMonitorEnvelope{0.f};
  std::atomic<int> mMonitorStep{0};
  std::atomic<bool> mMonitorRunning{false};
};
} // namespace grid_performance
