#pragma once
#include "EffectParameters.h"
#include "TempoSync.h"
#include <atomic>
#include <cstdint>

namespace grid_performance
{
enum Param
{
  // Persistent IDs from 0.1.0: append only. Never renumber or reuse retired entries.
  InputGain = 0,
  Crossover,
  CrossoverFrequency,
  LaneAGain,
  LaneBGain,
  LaneAMute,
  LaneBMute,
  LaneASolo,
  LaneBSolo,
  LaneTiming,
  LFOShape,
  LFORate,
  LFOSync,
  LFODivision,
  LFORetrigger,   // Retrigger modes: Free, audio onset, MIDI note.
  LFOOneShot,
  EnvInput,       // 0: grid input; 1..16: after that slot.
  EnvThreshold,
  EnvSensitivity,
  EnvAttack,
  EnvRelease,
  EnvCurve,
  StepRate,
  StepSync,
  StepDivision,
  StepRetrigger,
  Step1,
  Step2,
  Step3,
  Step4,
  Step5,
  Step6,
  Step7,
  Step8,
  Macro1,
  Macro2,
  Macro3,
  Macro4,
  // Eight modulation matrix rows of RowFields controls each; see RowParam().
  ModRow,
  LFO2Shape = ModRow + 8 * 5,
  LFO2Rate, LFO2Sync, LFO2Division, LFO2Retrigger, LFO2OneShot,
  Env2Input, Env2Threshold, Env2Sensitivity, Env2Attack, Env2Release, Env2Curve,
  Seq2Rate, Seq2Sync, Seq2Division, Seq2Retrigger,
  Seq2Step1, Seq2Step2, Seq2Step3, Seq2Step4, Seq2Step5, Seq2Step6, Seq2Step7, Seq2Step8,
  Count,
  // Numbered identities for the original bank; released host names/keys stay unchanged.
  LFO1Shape = LFOShape, LFO1Rate = LFORate, LFO1Sync = LFOSync, LFO1Division = LFODivision,
  LFO1Retrigger = LFORetrigger, LFO1OneShot = LFOOneShot,
  Env1Input = EnvInput, Env1Threshold = EnvThreshold, Env1Sensitivity = EnvSensitivity,
  Env1Attack = EnvAttack, Env1Release = EnvRelease, Env1Curve = EnvCurve,
  Seq1Rate = StepRate, Seq1Sync = StepSync, Seq1Division = StepDivision, Seq1Retrigger = StepRetrigger,
  Seq1Step1 = Step1, Seq1Step2 = Step2, Seq1Step3 = Step3, Seq1Step4 = Step4,
  Seq1Step5 = Step5, Seq1Step6 = Step6, Seq1Step7 = Step7, Seq1Step8 = Step8,
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
static_assert(RowParam(rows, 0) == LFO2Shape);
inline constexpr int sourceInstances = 2;
// A bank's controls share their relative order; new banks append after all old parameters.
constexpr int SourceParam(int parameter, int instance)
{ return instance == 0 ? parameter : LFO2Shape + parameter - LFO1Shape; }
enum Source
{
  SourceNone = 0,
  SourceLFO1,
  SourceEnv1,
  SourceSeq1,
  SourceMacro1,
  SourceMacro2,
  SourceMacro3,
  SourceMacro4,
  SourceLFO2,
  SourceEnv2,
  SourceSeq2,
  SourceCount,
  SourceLFO = SourceLFO1, SourceEnvelope = SourceEnv1, SourceSequencer = SourceSeq1
};
inline constexpr std::array<const char*, SourceCount> sourceNames{
  "None", "LFO 1", "Env 1", "Seq 1", "Macro 1", "Macro 2", "Macro 3", "Macro 4", "LFO 2", "Env 2", "Seq 2"};
inline constexpr std::array<const char*, SourceCount> sourceKeys{
  "none", "lfo1", "env1", "seq1", "macro1", "macro2", "macro3", "macro4", "lfo2", "env2", "seq2"};
inline constexpr std::array<int, 6> sourceTabs{SourceLFO1, SourceLFO2, SourceEnv1, SourceEnv2, SourceSeq1, SourceSeq2};
constexpr int SourceInstance(int source) { return source >= SourceLFO2 ? 1 : 0; }
constexpr int SourceKind(int source) { return source >= SourceLFO2 ? source - SourceLFO2 + SourceLFO1 : source; }
constexpr bool DynamicSource(int source) { return SourceKind(source) >= SourceLFO1 && SourceKind(source) <= SourceSeq1; }
// The LFO and sequencer swing both ways; the follower and macros only add.
inline constexpr bool Bipolar(int source) { return SourceKind(source) == SourceLFO1 || SourceKind(source) == SourceSeq1; }
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
enum Retrigger
{
  RetriggerFree = 0,
  RetriggerOnset,
  RetriggerMidi,
  RetriggerModes
};
inline constexpr std::array<const char*, RetriggerModes> retriggerNames{"Free running", "Audio onset", "MIDI note"};
// The follower's threshold control turns off at its minimum.
inline constexpr double kThresholdOff = -90.;
inline constexpr std::array<EffectParameter, Count> parameters{{
  {"Input", "Input", 0, -40, 40, .1, "dB", 0},
  {"Frequency Split", "Frequency split", 0, 0, 1, 1, "", 0},
  {"Split Frequency", "Split frequency", 250, 40, 5000, 1, "Hz", 0, true},
  {"Lane A Level", "A level", 0, -60, 12, .1, "dB", 0},
  {"Lane B Level", "B level", 0, -60, 12, .1, "dB", 0},
  {"Lane A Mute", "Mute A", 0, 0, 1, 1, "", 0},
  {"Lane B Mute", "Mute B", 0, 0, 1, 1, "", 0},
  {"Lane A Solo", "Solo A", 0, 0, 1, 1, "", 0},
  {"Lane B Solo", "Solo B", 0, 0, 1, 1, "", 0},
  {"Lane Timing", "Lane timing", 0, -20, 20, .01, "ms", 0},
  {"LFO Shape", "Shape", 0, 0, 5, 1, "", 0},
  {"LFO Rate", "Rate", .5, .05, 10, .01, "Hz", 0, true},
  {"LFO Sync", "Tempo sync", 0, 0, 1, 1, "", 0},
  {"LFO Division", "Division", 5, 0, 9, 1, "", 0},
  {"LFO Retrigger", "Retrigger", 0, 0, 2, 1, "", 0},
  {"LFO One Shot", "One shot", 0, 0, 1, 1, "", 0},
  {"Envelope Listen At", "Listen at", 0, 0, 16, 1, "", 0},
  {"Envelope Threshold", "Threshold", kThresholdOff, kThresholdOff, 0, .1, "dB", 0},
  {"Envelope Sensitivity", "Sensitivity", 12, 0, 36, .1, "dB", 0},
  {"Envelope Attack", "Attack", 10, .5, 100, .1, "ms", 0, true},
  {"Envelope Release", "Release", 150, 10, 1000, 1, "ms", 0, true},
  {"Envelope Curve", "Curve", 0, -100, 100, .1, "%", 0},
  {"Sequencer Rate", "Rate", 4, .1, 20, .01, "Hz", 0, true},
  {"Sequencer Sync", "Tempo sync", 1, 0, 1, 1, "", 0},
  {"Sequencer Division", "Division", 1, 0, 9, 1, "", 0},
  {"Sequencer Retrigger", "Retrigger", 0, 0, 2, 1, "", 0},
  {"Sequencer Step 1", "1", 0, 0, 100, 1, "%", 0},
  {"Sequencer Step 2", "2", 100, 0, 100, 1, "%", 0},
  {"Sequencer Step 3", "3", 25, 0, 100, 1, "%", 0},
  {"Sequencer Step 4", "4", 75, 0, 100, 1, "%", 0},
  {"Sequencer Step 5", "5", 0, 0, 100, 1, "%", 0},
  {"Sequencer Step 6", "6", 100, 0, 100, 1, "%", 0},
  {"Sequencer Step 7", "7", 50, 0, 100, 1, "%", 0},
  {"Sequencer Step 8", "8", 75, 0, 100, 1, "%", 0},
  {"Macro 1", "Macro 1", 0, 0, 100, .1, "%", 0},
  {"Macro 2", "Macro 2", 0, 0, 100, .1, "%", 0},
  {"Macro 3", "Macro 3", 0, 0, 100, .1, "%", 0},
  {"Macro 4", "Macro 4", 0, 0, 100, .1, "%", 0},
  {"Mod 1 Source", "Source", 0, 0, SourceCount - 1, 1, "", 0},
  {"Mod 1 Slot", "Slot", 0, 0, 16, 1, "", 0},
  {"Mod 1 Destination", "Destination", 0, 0, 1023, 1, "", 0},
  {"Mod 1 Depth", "Depth", 0, -100, 100, .1, "%", 0},
  {"Mod 1 Via", "Via", 0, 0, SourceCount - 1, 1, "", 0},
  {"Mod 2 Source", "Source", 0, 0, SourceCount - 1, 1, "", 0},
  {"Mod 2 Slot", "Slot", 0, 0, 16, 1, "", 0},
  {"Mod 2 Destination", "Destination", 0, 0, 1023, 1, "", 0},
  {"Mod 2 Depth", "Depth", 0, -100, 100, .1, "%", 0},
  {"Mod 2 Via", "Via", 0, 0, SourceCount - 1, 1, "", 0},
  {"Mod 3 Source", "Source", 0, 0, SourceCount - 1, 1, "", 0},
  {"Mod 3 Slot", "Slot", 0, 0, 16, 1, "", 0},
  {"Mod 3 Destination", "Destination", 0, 0, 1023, 1, "", 0},
  {"Mod 3 Depth", "Depth", 0, -100, 100, .1, "%", 0},
  {"Mod 3 Via", "Via", 0, 0, SourceCount - 1, 1, "", 0},
  {"Mod 4 Source", "Source", 0, 0, SourceCount - 1, 1, "", 0},
  {"Mod 4 Slot", "Slot", 0, 0, 16, 1, "", 0},
  {"Mod 4 Destination", "Destination", 0, 0, 1023, 1, "", 0},
  {"Mod 4 Depth", "Depth", 0, -100, 100, .1, "%", 0},
  {"Mod 4 Via", "Via", 0, 0, SourceCount - 1, 1, "", 0},
  {"Mod 5 Source", "Source", 0, 0, SourceCount - 1, 1, "", 0},
  {"Mod 5 Slot", "Slot", 0, 0, 16, 1, "", 0},
  {"Mod 5 Destination", "Destination", 0, 0, 1023, 1, "", 0},
  {"Mod 5 Depth", "Depth", 0, -100, 100, .1, "%", 0},
  {"Mod 5 Via", "Via", 0, 0, SourceCount - 1, 1, "", 0},
  {"Mod 6 Source", "Source", 0, 0, SourceCount - 1, 1, "", 0},
  {"Mod 6 Slot", "Slot", 0, 0, 16, 1, "", 0},
  {"Mod 6 Destination", "Destination", 0, 0, 1023, 1, "", 0},
  {"Mod 6 Depth", "Depth", 0, -100, 100, .1, "%", 0},
  {"Mod 6 Via", "Via", 0, 0, SourceCount - 1, 1, "", 0},
  {"Mod 7 Source", "Source", 0, 0, SourceCount - 1, 1, "", 0},
  {"Mod 7 Slot", "Slot", 0, 0, 16, 1, "", 0},
  {"Mod 7 Destination", "Destination", 0, 0, 1023, 1, "", 0},
  {"Mod 7 Depth", "Depth", 0, -100, 100, .1, "%", 0},
  {"Mod 7 Via", "Via", 0, 0, SourceCount - 1, 1, "", 0},
  {"Mod 8 Source", "Source", 0, 0, SourceCount - 1, 1, "", 0},
  {"Mod 8 Slot", "Slot", 0, 0, 16, 1, "", 0},
  {"Mod 8 Destination", "Destination", 0, 0, 1023, 1, "", 0},
  {"Mod 8 Depth", "Depth", 0, -100, 100, .1, "%", 0},
  {"Mod 8 Via", "Via", 0, 0, SourceCount - 1, 1, "", 0},
  {"LFO 2 Shape", "Shape", 0, 0, 5, 1, "", 0, false, 1., 6},
  {"LFO 2 Rate", "Rate", .5, .05, 10, .01, "Hz", 0, true, 1., 6},
  {"LFO 2 Sync", "Tempo sync", 0, 0, 1, 1, "", 0, false, 1., 6},
  {"LFO 2 Division", "Division", 5, 0, 9, 1, "", 0, false, 1., 6},
  {"LFO 2 Retrigger", "Retrigger", 0, 0, 2, 1, "", 0, false, 1., 6},
  {"LFO 2 One Shot", "One shot", 0, 0, 1, 1, "", 0, false, 1., 6},
  {"Envelope 2 Listen At", "Listen at", 0, 0, 16, 1, "", 0, false, 1., 6},
  {"Envelope 2 Threshold", "Threshold", kThresholdOff, kThresholdOff, 0, .1, "dB", 0, false, 1., 6},
  {"Envelope 2 Sensitivity", "Sensitivity", 12, 0, 36, .1, "dB", 0, false, 1., 6},
  {"Envelope 2 Attack", "Attack", 10, .5, 100, .1, "ms", 0, true, 1., 6},
  {"Envelope 2 Release", "Release", 150, 10, 1000, 1, "ms", 0, true, 1., 6},
  {"Envelope 2 Curve", "Curve", 0, -100, 100, .1, "%", 0, false, 1., 6},
  {"Sequencer 2 Rate", "Rate", 4, .1, 20, .01, "Hz", 0, true, 1., 6},
  {"Sequencer 2 Sync", "Tempo sync", 1, 0, 1, 1, "", 0, false, 1., 6},
  {"Sequencer 2 Division", "Division", 1, 0, 9, 1, "", 0, false, 1., 6},
  {"Sequencer 2 Retrigger", "Retrigger", 0, 0, 2, 1, "", 0, false, 1., 6},
  {"Sequencer 2 Step 1", "1", 0, 0, 100, 1, "%", 0, false, 1., 6},
  {"Sequencer 2 Step 2", "2", 100, 0, 100, 1, "%", 0, false, 1., 6},
  {"Sequencer 2 Step 3", "3", 25, 0, 100, 1, "%", 0, false, 1., 6},
  {"Sequencer 2 Step 4", "4", 75, 0, 100, 1, "%", 0, false, 1., 6},
  {"Sequencer 2 Step 5", "5", 0, 0, 100, 1, "%", 0, false, 1., 6},
  {"Sequencer 2 Step 6", "6", 100, 0, 100, 1, "%", 0, false, 1., 6},
  {"Sequencer 2 Step 7", "7", 50, 0, 100, 1, "%", 0, false, 1., 6},
  {"Sequencer 2 Step 8", "8", 75, 0, 100, 1, "%", 0, false, 1., 6}
}};
// Matrix routing, the LFO shape, Listen at and the Retrigger modes are saved state, like
// algorithm selectors: no host automation, so a longer list never reinterprets recorded
// automation. Tempo divisions stay automatable; their list of ten values is frozen.
inline constexpr bool StateOnly(int parameter)
{
  for (int instance = 0; instance < sourceInstances; ++instance)
    if (parameter == SourceParam(LFO1Shape, instance) || parameter == SourceParam(Env1Input, instance)
        || parameter == SourceParam(LFO1Retrigger, instance) || parameter == SourceParam(Seq1Retrigger, instance)) return true;
  if (parameter < ModRow || parameter >= RowParam(rows, 0)) return false;
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
class SourceBank
{
public:
  explicit SourceBank(int instance = 0) : mInstance(instance) {}
  // Latest source values for the editor; written once per audio block.
  struct Monitor
  {
    float phase = 0.f, lfo = 0.f, envelope = 0.f, input = 0.f; // input: follower level before shaping
    int step = 0;
    bool running = false;
  };
  void Reset(double rate)
  {
    mRate = rate; mPhase = mStepPhase = mEnvelope = mHeldPhase = mHeldEnvelope = mHeldInput = 0.; mStep = mHeldStep = 0; mFrames = 0;
    mRandom = mHeldRandom = 0.; mSeed = 0x2545F491u ^ (mInstance ? 0x9E3779B9u : 0u);
    mFast = mSlow = 0.; mArmed = true; mHoldoff = 0; mFinished = true;
    mMonitorRunning.store(false, std::memory_order_relaxed);
  }
  int Quantum() const { return 16 - static_cast<int>(mFrames % 16); }
  bool Active(const Values& v) const
  {
    for (int row = 0; row < rows; ++row)
      if (RowActive(v, row)) return true;
    return false;
  }
  // Follower output 0..1: level above the threshold, times Sensitivity, bent by Curve.
  // At the defaults (threshold off, curve 0) this is the plain level times Sensitivity.
  static double ShapeEnvelope(double level, const Values& v, int instance = 0)
  {
    const double threshold = v[SourceParam(EnvThreshold, instance)] <= kThresholdOff ? 0. : std::pow(10., v[SourceParam(EnvThreshold, instance)] * .05);
    const double out = std::clamp((level - threshold) * std::pow(10., v[SourceParam(EnvSensitivity, instance)] * .05), 0., 1.);
    return v[SourceParam(EnvCurve, instance)] == 0. ? out : std::pow(out, std::exp2(-v[SourceParam(EnvCurve, instance)] * .02));
  }
  // `l`/`r` is the follower's listening point. `notes` are this span's MIDI note-on
  // frames in order; each retriggers on its frame.
  void Advance(const double* l, const double* r, int n, const Values& v, double tempo, const int* notes = nullptr,
               int noteCount = 0)
  {
    const double lfo = v[SourceParam(LFOSync, mInstance)] >= .5 ? 1. / grid_tempo::Seconds(tempo, static_cast<int>(v[SourceParam(LFODivision, mInstance)])) : v[SourceParam(LFORate, mInstance)];
    const double steps = v[SourceParam(StepSync, mInstance)] >= .5 ? 1. / grid_tempo::Seconds(tempo, static_cast<int>(v[SourceParam(StepDivision, mInstance)])) : v[SourceParam(StepRate, mInstance)];
    const double attack = std::exp(-1. / (.001 * v[SourceParam(EnvAttack, mInstance)] * mRate));
    const double release = std::exp(-1. / (.001 * v[SourceParam(EnvRelease, mInstance)] * mRate));
    const int lfoRetrigger = static_cast<int>(v[SourceParam(LFORetrigger, mInstance)]), stepRetrigger = static_cast<int>(v[SourceParam(StepRetrigger, mInstance)]);
    const bool oneShot = v[SourceParam(LFOOneShot, mInstance)] >= .5, onsets = lfoRetrigger == RetriggerOnset || stepRetrigger == RetriggerOnset;
    // Onsets: a fast follower jumping 6 dB above a slow one, above the threshold
    // (or -50 dBFS while it is off), at most every 50 ms.
    const double fastAttack = std::exp(-1. / (.0005 * mRate)), fastRelease = std::exp(-1. / (.03 * mRate));
    const double slow = std::exp(-1. / (.04 * mRate));
    const double floor = v[SourceParam(EnvThreshold, mInstance)] <= kThresholdOff ? .00316 : std::pow(10., v[SourceParam(EnvThreshold, mInstance)] * .05);
    int nextNote = 0;
    for (int i = 0; i < n; ++i)
    {
      const double peak = std::max(std::abs(l[i]), r ? std::abs(r[i]) : 0.);
      mEnvelope = peak + (peak > mEnvelope ? attack : release) * (mEnvelope - peak);
      bool onset = false;
      if (onsets)
      {
        mFast = peak + (peak > mFast ? fastAttack : fastRelease) * (mFast - peak);
        mSlow = peak + slow * (mSlow - peak);
        if (mHoldoff > 0) --mHoldoff;
        if (mArmed && mHoldoff == 0 && mFast > floor && mFast > 2. * mSlow)
        {
          onset = true; mArmed = false; mHoldoff = static_cast<int>(.05 * mRate);
        }
        else if (!mArmed && mFast < 1.25 * mSlow) mArmed = true;
      }
      bool midi = false;
      for (; nextNote < noteCount && notes[nextNote] <= i; ++nextNote) midi = true;
      if ((lfoRetrigger == RetriggerOnset && onset) || (lfoRetrigger == RetriggerMidi && midi))
      {
        mPhase = 0.; mFinished = false; NextRandom();
      }
      if ((stepRetrigger == RetriggerOnset && onset) || (stepRetrigger == RetriggerMidi && midi))
      {
        mStep = 0; mStepPhase = 0.;
      }
      if (oneShot)
      {
        // One cycle per trigger, then hold the cycle's end.
        if (mFinished) mPhase = 1.;
        else if ((mPhase += lfo / mRate) >= 1.) { mPhase = 1.; mFinished = true; }
      }
      else
      {
        const double phase = mPhase + lfo / mRate;
        if (phase >= 1.) NextRandom();
        mPhase = std::fmod(phase, 1.);
      }
      mStepPhase += steps / mRate;
      if (mStepPhase >= 1.) { mStepPhase -= 1.; mStep = (mStep + 1) % 8; }
      if (++mFrames % 16 == 0)
      {
        mHeldPhase = mPhase; mHeldInput = mEnvelope; mHeldEnvelope = ShapeEnvelope(mEnvelope, v, mInstance);
        mHeldStep = mStep; mHeldRandom = mRandom;
      }
    }
    mMonitorPhase.store(static_cast<float>(mHeldPhase), std::memory_order_relaxed);
    mMonitorLFO.store(static_cast<float>(LFOValue(v)), std::memory_order_relaxed);
    mMonitorEnvelope.store(static_cast<float>(mHeldEnvelope), std::memory_order_relaxed);
    mMonitorInput.store(static_cast<float>(mHeldInput), std::memory_order_relaxed);
    mMonitorStep.store(mHeldStep, std::memory_order_relaxed);
    mMonitorRunning.store(true, std::memory_order_relaxed);
  }
  // Editor only: stop drawing live values once processing has stopped or no source is used.
  void Idle() { mMonitorRunning.store(false, std::memory_order_relaxed); }
  Monitor Read() const
  {
    return {mMonitorPhase.load(std::memory_order_relaxed), mMonitorLFO.load(std::memory_order_relaxed),
            mMonitorEnvelope.load(std::memory_order_relaxed), mMonitorInput.load(std::memory_order_relaxed),
            mMonitorStep.load(std::memory_order_relaxed), mMonitorRunning.load(std::memory_order_relaxed)};
  }
  double LFOValue(const Values& v) const
  {
    const int shape = static_cast<int>(v[SourceParam(LFOShape, mInstance)]);
    return shape == 5 ? mHeldRandom : LFOWave(shape, mHeldPhase);
  }
  // Held value of a source: -1..1 for the LFO and sequencer, 0..1 otherwise.
  double Value(int source, const Values& v) const
  {
    switch (source)
    {
      case SourceLFO: return LFOValue(v);
      case SourceEnvelope: return mHeldEnvelope;
      case SourceSequencer: return v[SourceParam(Seq1Step1, mInstance) + mHeldStep] * .02 - 1.;
      case SourceMacro1: case SourceMacro2: case SourceMacro3: case SourceMacro4:
        return std::clamp(v[macros[source - SourceMacro1]] * .01, 0., 1.);
      default: return 0.;
    }
  }
private:
  void NextRandom()
  {
    // xorshift32: a new held value per Random cycle, without allocation or locks.
    mSeed ^= mSeed << 13; mSeed ^= mSeed >> 17; mSeed ^= mSeed << 5;
    mRandom = mSeed * (2. / 4294967295.) - 1.;
  }
  int mInstance = 0;
  double mRate = 48000., mPhase = 0., mStepPhase = 0., mEnvelope = 0.;
  int mStep = 0;
  double mHeldPhase = 0., mHeldEnvelope = 0., mHeldInput = 0.;
  int mHeldStep = 0;
  double mRandom = 0., mHeldRandom = 0.;
  uint32_t mSeed = 0x2545F491u;
  double mFast = 0., mSlow = 0.;
  bool mArmed = true, mFinished = true;
  int mHoldoff = 0;
  uint64_t mFrames = 0;
  std::atomic<float> mMonitorPhase{0.f}, mMonitorLFO{0.f}, mMonitorEnvelope{0.f}, mMonitorInput{0.f};
  std::atomic<int> mMonitorStep{0};
  std::atomic<bool> mMonitorRunning{false};
};

// Independent source banks share the same 16-sample clock, MIDI timeline and matrix.
// No allocation or host-parameter writes occur while advancing either bank.
class Modulation
{
public:
  using SourceMonitor = SourceBank::Monitor;
  struct Monitor : SourceMonitor
  {
    SourceMonitor second;
    const SourceMonitor& Instance(int instance) const { return instance == 0 ? *this : second; }
  };
  void Reset(double rate) { for (auto& bank : mBanks) bank.Reset(rate); }
  int Quantum() const { return mBanks[0].Quantum(); }
  bool Active(const Values& v) const { return mBanks[0].Active(v); }
  static double ShapeEnvelope(double level, const Values& v, int instance = 0)
  { return SourceBank::ShapeEnvelope(level, v, instance); }
  // Optional second listening point defaults to the first for standalone DSP callers.
  void Advance(const double* l, const double* r, int n, const Values& v, double tempo,
               const int* notes = nullptr, int noteCount = 0, const double* secondL = nullptr, const double* secondR = nullptr)
  {
    mBanks[0].Advance(l, r, n, v, tempo, notes, noteCount);
    mBanks[1].Advance(secondL ? secondL : l, secondL ? secondR : r, n, v, tempo, notes, noteCount);
  }
  void Idle() { for (auto& bank : mBanks) bank.Idle(); }
  Monitor Read() const { return {mBanks[0].Read(), mBanks[1].Read()}; }
  double LFOValue(const Values& v, int instance = 0) const { return mBanks[instance].LFOValue(v); }
  double Value(int source, const Values& v) const
  { return mBanks[SourceInstance(source)].Value(SourceKind(source), v); }
private:
  std::array<SourceBank, sourceInstances> mBanks{SourceBank(0), SourceBank(1)};
};
} // namespace grid_performance
