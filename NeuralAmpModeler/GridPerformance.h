#pragma once
#include "EffectParameters.h"
#include "TempoSync.h"
#include <cstdint>

namespace grid_performance
{
enum Param
{
  Crossover, CrossoverFrequency, LaneAGain, LaneBGain, LaneAMute, LaneBMute, LaneASolo, LaneBSolo, LaneTiming,
  LFOSlot, LFOTarget, LFORate, LFODepth, LFOSync, LFODivision,
  EnvSlot, EnvTarget, EnvSensitivity, EnvAttack, EnvRelease, EnvDepth,
  StepSlot, StepTarget, StepRate, StepDepth, StepSync, StepDivision,
  Step1, Step2, Step3, Step4, Step5, Step6, Step7, Step8,
  Macro1, Macro1Slot, Macro1Target, Macro1Depth,
  Macro2, Macro2Slot, Macro2Target, Macro2Depth,
  Count
};
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
  {"LFO Division", "Division", 4, 0, 8, 1, "", 0},
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
  {"Sequencer Division", "Division", 0, 0, 8, 1, "", 0},
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
  {"Macro Two Depth", "Depth", 0, -100, 100, .1, "%", 0}
}};
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

class Modulation
{
public:
  void Reset(double rate) { mRate = rate; mPhase = mStepPhase = mEnvelope = mHeldPhase = mHeldEnvelope = 0.; mStep = mHeldStep = 0; mFrames = 0; }
  int Quantum() const { return 16 - static_cast<int>(mFrames % 16); }
  bool Active(const Values& v) const
  {
    return (v[LFOTarget] > 0. && v[LFODepth] != 0.) || (v[EnvTarget] > 0. && v[EnvDepth] != 0.)
        || (v[StepTarget] > 0. && v[StepDepth] != 0.) || (v[Macro1Target] > 0. && v[Macro1Depth] != 0.)
        || (v[Macro2Target] > 0. && v[Macro2Depth] != 0.);
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
      mPhase = std::fmod(mPhase + lfo / mRate, 1.);
      mStepPhase += steps / mRate;
      if (mStepPhase >= 1.) { mStepPhase -= 1.; mStep = (mStep + 1) % 8; }
      if (++mFrames % 16 == 0) { mHeldPhase = mPhase; mHeldEnvelope = mEnvelope; mHeldStep = mStep; }
    }
  }
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
          u.spring[3] = std::clamp(u.spring[3] + amount * 100., 0., 100.); break;
        case 6: c.trimDB = std::clamp(c.trimDB + amount * 24., -40., 40.); break;
        case 7: blend = std::clamp(blend + amount, 0., 1.); break;
        case 8: u.ring[0] = std::clamp(u.ring[0] * std::exp2(amount * 4.), 1., 5000.); break;
        default: break;
      }
    };
    assign(LFOSlot, LFOTarget, std::sin(6.283185307179586 * mHeldPhase) * v[LFODepth] * .01);
    assign(EnvSlot, EnvTarget, std::clamp(mHeldEnvelope, 0., 1.) * v[EnvDepth] * .01);
    assign(StepSlot, StepTarget, (v[Step1 + mHeldStep] * .02 - 1.) * v[StepDepth] * .01);
    assign(Macro1Slot, Macro1Target, v[Macro1] * v[Macro1Depth] * .0001);
    assign(Macro2Slot, Macro2Target, v[Macro2] * v[Macro2Depth] * .0001);
  }
private:
  double mRate = 48000., mPhase = 0., mStepPhase = 0., mEnvelope = 0.;
  int mStep = 0;
  double mHeldPhase = 0., mHeldEnvelope = 0.;
  int mHeldStep = 0;
  uint64_t mFrames = 0;
};
} // namespace grid_performance
