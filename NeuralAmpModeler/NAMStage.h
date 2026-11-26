#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <vector>
#include "ResamplingNAM.h"
#include "RealtimeExchange.h"
#include "SlotDynamics.h"
#include "SlotUtility.h"
#include "SlotTimeEffects.h"
#include "SlotReverb.h"
#include "SlotAirReverb.h"
#include "SlotOutboard.h"
#include "SlotAirDynamics.h"
#include "SlotCreativeDelays.h"
#include "ExpandedDelay.h"
#include "SlotCosmicEcho.h"
#include "SlotGranulator.h"
#include "StereoUtility.h"
#include "SlotGuitarEffects.h"
#include "RoadmapEffects.h"
#include "SlotHQEffects.h"
#include "EQSignalSpectrum.h"
#include "SlotDistortion.h"
#include "SlotPedalDrive.h"
#include "SlotPedalDistortion.h"
#include "SlotPedalComp.h"
#include "SlotDynamicEQ.h"
#include "SlotTransientShaper.h"
#include "SlotMultiband.h"
#include "SlotGraphicEQ.h"
#include "SlotPitchShift.h"
#include "SlotLoFiTape.h"
#include "AdaptiveIR.h"
#include "SlotIR.h"

struct NAMModelInfo
{
  bool loaded = false, hasInput = false, hasOutput = false, hasLoudness = false, slimmable = false;
  double rate = 48000., input = 0., output = 0., loudness = 0.;
};

struct SlotMeterData
{
  float input = 0.f, output = 0.f, reduction = 0.f;
  float beforeMakeup = 0.f;
  float cleaned = 0.f; // Smart and AI Gate: after Mix, before the final gate.
  // Mono-sum energy and frame count since the last take, for the editor's RMS level strip.
  double inputEnergy = 0., outputEnergy = 0.;
  int frames = 0;
  LoFiTapeHistory lofi;
  std::array<float, 2> filterCutoff{}, filterQ{};
  float pedalPosition = -1.f, pedalGain = 0.f; // Volume Pedal: live position (0..1) and gain.
  // Multiband Dynamics: each band's largest dynamic gain (dB) and detected level (dBFS).
  std::array<float, 3> bandGain{}, bandLevel{-120.f, -120.f, -120.f};
};

// Effects whose editor overlays a live output spectrum on the response curve: every
// EQ / Filter module, the NAM post-EQ and Outboard Channel's cuts, all at one resolution.
inline bool ShowsSignalSpectrum(SlotEffect effect)
{
  return effect == SlotEffect::NAM || effect == SlotEffect::ToneEQ || effect == SlotEffect::CutEQ
         || effect == SlotEffect::ParametricEQ || effect == SlotEffect::SynthFilter
         || effect == SlotEffect::OutboardChannel || effect == SlotEffect::ConsoleEQ || effect == SlotEffect::Wah
         || effect == SlotEffect::DynamicEQ || effect == SlotEffect::Multiband || effect == SlotEffect::GraphicEQ
         || effect == SlotEffect::PitchShift
         || effect == SlotEffect::Utility; // Utility uses the window's stereo samples for its scope.
}

// The newest output samples of one slot, for the editor's spectrum and stereo scope. A plugin
// owns one and attaches it only to the selected slot's stage, so 32 stages share 32 KB.
struct SignalCapture
{
  std::vector<float> left = std::vector<float>(grid_eq_signal::windowSize, 0.f);
  std::vector<float> right = std::vector<float>(grid_eq_signal::windowSize, 0.f);
  size_t write = 0, count = 0;
  void Clear() { write = count = 0; }
  void Push(double l, double r)
  {
    left[write] = static_cast<float>(l);
    right[write] = static_cast<float>(r);
    write = (write + 1) % left.size();
    count = std::min(count + 1, left.size());
  }
  bool Take(grid_eq_signal::Window& window) const
  {
    if (count < left.size()) return false;
    for (size_t i = 0; i < left.size(); ++i)
      window.samples[i] = left[(write + i) % left.size()];
    for (size_t i = 0; i < window.right.size(); ++i)
      window.right[i] = right[(write + left.size() - window.right.size() + i) % left.size()];
    return true;
  }
};

class NAMStage
{
public:
  struct ChannelDSP
  {
    SlotDynamics dynamics;
    SlotEQ eq;
    SlotParametricEQ parametric;
    SlotBoost boost;
    SlotTubeBoost tube;
    SlotDistortion distortion;
    SlotPedalDrive pedal;
    SlotPedalDistortion pedalDistortion;
    SlotEnvelopeFilter filter;
    SlotAutoSwell swell;
    SlotVolumePedal volumePedal;
    SlotDelay echo;
    grid_roadmap::Vibe vibe;
    grid_roadmap::Texture texture;
    SlotModulation modulation;
    SlotWah wah;
    SlotTape tape;

    void Reset(double rate, SlotEffect effect)
    {
      if (effect == SlotEffect::NoiseGate || effect == SlotEffect::SmoothComp || effect == SlotEffect::PunchComp)
        dynamics.Reset(rate);
      if (effect == SlotEffect::ToneEQ || effect == SlotEffect::CutEQ || effect == SlotEffect::NAM)
        eq.Reset(rate);
      if (effect == SlotEffect::ParametricEQ)
        parametric.Reset(rate);
      if (effect == SlotEffect::Boost)
        boost.Reset(rate);
      if (effect == SlotEffect::TubeBoost)
        tube.Reset(rate);
      if (effect == SlotEffect::Distortion)
        distortion.Reset(rate);
      if (effect == SlotEffect::PedalDrive)
        pedal.Reset(rate);
      if (effect == SlotEffect::PedalDistortion)
        pedalDistortion.Reset(rate);
      if (effect == SlotEffect::SynthFilter)
        filter.Reset(rate);
      if (effect == SlotEffect::AutoSwell)
        swell.Reset(rate);
      if (effect == SlotEffect::VolumePedal)
        volumePedal.Reset(rate);
      if (IsDelay(effect) && !UsesCoupledStereo(effect))
        echo.Reset(rate);
      if (effect == SlotEffect::UniVibe)
        vibe.Reset(rate);
      if (effect == SlotEffect::Bitcrusher || effect == SlotEffect::RingMod)
        texture.Reset(rate);
      if (effect == SlotEffect::Wah)
        wah.Reset(rate);
      if (IsModulation(effect))
        modulation.Reset(rate);
      if (effect == SlotEffect::Tape)
        tape.Reset(rate);
    }
  };

  struct RightChannel
  {
    SlotEffect effect = SlotEffect::NAM;
    std::unique_ptr<AdaptiveIR> ir;
    SlotIR irWet;
    std::unique_ptr<ChannelDSP> dsp;
    std::vector<double> input, wet, dry, delay, dynamicsMonitor;
    size_t delayIndex = 0;
    int latency = 0;
    int64_t drainLimit = 0;

    void Reset(double rate, int block, int parentLatency)
    {
      const bool needsDSP = !UsesCoupledStereo(effect) && effect != SlotEffect::NAM
                            && effect != SlotEffect::SpringReverb && effect != SlotEffect::Utility
                            && effect != SlotEffect::CabinetIR;
      if (needsDSP)
      {
        if (!dsp) dsp = std::make_unique<ChannelDSP>();
        dsp->Reset(rate, effect);
      }
      else
        dsp.reset();
      latency = parentLatency;
      input.assign(block, 0.);
      wet.assign(block, 0.);
      dry.assign(block, 0.);
      if (effect == SlotEffect::SmoothComp || effect == SlotEffect::PunchComp)
        dynamicsMonitor.assign(block, 0.);
      else
        dynamicsMonitor.clear();
      delay.assign(std::max(1, latency), 0.);
      delayIndex = 0;
      drainLimit = 0;
      if (ir)
      {
        irWet.Reset(rate);
        drainLimit = static_cast<int64_t>(ir->Length()) + static_cast<int64_t>(rate * 1.5);
      }
    }
  };

  struct Prepared : ChannelDSP
  {
    std::unique_ptr<ResamplingNAM> model;
    SlotEffect effect = SlotEffect::NAM;
    noise_print::Processor noise;
    uint64_t noiseEpoch = 0;
    ai_gate::Processor aiGate;
    SlotEQ namCuts;
    SlotReverb reverb;
    SlotAirReverb airReverb;
    SlotOutboard outboard;
    SlotAirDynamics airDynamics;
    SlotPedalComp pedalComp;
    SlotDynamicEQ dynamicEQ;
    SlotTransientShaper transientShaper;
    SlotMultiband multiband;
    SlotGraphicEQ graphicEQ;
    SlotPitchShift pitchShift;
    SlotMultiHeadDelay multiHead;
    SlotDiffusedDelay diffused;
    std::unique_ptr<SlotExpandedDelay> expandedDelay;
    SlotCosmicEcho cosmic;
    std::unique_ptr<SlotGranulator> granulator; // 4 s stereo buffer, only for Granulator slots.
    SlotLoFiTape lofi;
    SlotSpring spring;
    grid_roadmap::Limiter limiter;
    grid_roadmap::Rotary rotary;
    grid_roadmap::Octaver octaver;
    grid_roadmap::Doubler doubler;
    grid_roadmap::Capture capture;
    grid_roadmap::DualDelay dualDelay;
    grid_hq::Shimmer shimmer;
    grid_hq::JunoChorus juno;
    grid_hq::FrequencyShifter frequencyShifter;
    StereoUtility utilityStereo;
    grid_fx::SmoothControls<1> eqOutput;
    grid_fx::Memo eqOutputGain;
    std::unique_ptr<RightChannel> right;
    std::unique_ptr<AdaptiveIR> ir;
    std::unique_ptr<AdaptiveIR::IRData> rightIRData; // Stereo source survives host resets.
    SlotIR irWet;
    std::vector<double> input, wet, dry, delay, dynamicsMonitor;
    size_t delayIndex = 0;
    int latency = 0;
    int64_t drainFrames = 0, drainLimit = 0;
    // Trails: frames a bypassed tail has rung, and how long its output has stayed silent.
    int64_t trailFrames = 0, trailLimit = 0, trailFade = 0, trailHold = 0, quietFrames = 0;
    void Reset(double rate, int block)
    {
      utilityStereo.Reset(rate); eqOutput.Reset(rate);
      ChannelDSP::Reset(rate, effect);
      if (effect == SlotEffect::NoisePrint)
        noise.Reset(rate);
      if (effect == SlotEffect::AIGate)
        aiGate.Reset(rate);
      if (effect == SlotEffect::Limiter) limiter.Reset(rate);
      if (effect == SlotEffect::Rotary) rotary.Reset(rate);
      if (effect == SlotEffect::Octaver) octaver.Reset(rate);
      if (effect == SlotEffect::Doubler) doubler.Reset(rate);
      if (effect == SlotEffect::Freeze || effect == SlotEffect::Stutter || effect == SlotEffect::ReverseDelay)
        capture.Reset(rate, effect == SlotEffect::Freeze ? 2.1 : 8.1);
      if (effect == SlotEffect::DualDelay) dualDelay.Reset(rate);
      if (effect == SlotEffect::ShimmerReverb) shimmer.Reset(rate);
      if (effect == SlotEffect::JunoChorus) juno.Reset(rate);
      if (effect == SlotEffect::FrequencyShifter) frequencyShifter.Reset(rate);
      if (effect == SlotEffect::LoFiTape) lofi.Reset(rate);
      if (effect == SlotEffect::MultiHeadDelay) multiHead.Reset(rate);
      if (effect == SlotEffect::DiffusedDelay) diffused.Reset(rate);
      if (IsExpandedDelay(effect))
      {
        if (!expandedDelay) expandedDelay = std::make_unique<SlotExpandedDelay>();
        expandedDelay->Reset(rate, block, effect);
      }
      if (effect == SlotEffect::CosmicEcho) cosmic.Reset(rate);
      if (effect == SlotEffect::Granulator)
      {
        if (!granulator) granulator = std::make_unique<SlotGranulator>();
        granulator->Reset(rate);
      }
      if (IsOutboard(effect)) outboard.Reset(rate, effect);
      if (IsAirDynamics(effect)) airDynamics.Reset(rate, effect);
      if (effect == SlotEffect::PedalComp) pedalComp.Reset(rate);
      if (effect == SlotEffect::DynamicEQ) dynamicEQ.Reset(rate);
      if (effect == SlotEffect::TransientShaper) transientShaper.Reset(rate);
      if (effect == SlotEffect::Multiband) multiband.Reset(rate);
      if (effect == SlotEffect::GraphicEQ) graphicEQ.Reset(rate);
      if (effect == SlotEffect::PitchShift) pitchShift.Reset(rate);
      if (IsAirReverb(effect)) airReverb.Reset(rate, effect);
      else if (IsReverb(effect) && effect != SlotEffect::SpringReverb) reverb.Reset(rate, effect);
      if (effect == SlotEffect::SpringReverb)
        spring.Reset(rate);
      if (effect == SlotEffect::NAM)
      {
        eq.Process(nullptr, 0, false, {});
        namCuts.Reset(rate);
        namCuts.Process(nullptr, 0, true, {});
      }
      if (model)
        model->Reset(rate, block);
      latency = model ? model->GetLatency() : 0; // Pedal Drive is minimum phase: no lookahead.
      input.assign(block, 0.);
      wet.assign(block, 0.);
      dry.assign(block, 0.);
      if (effect == SlotEffect::SmoothComp || effect == SlotEffect::PunchComp) dynamicsMonitor.assign(block, 0.);
      else dynamicsMonitor.clear();
      delay.assign(std::max(1, latency), 0.);
      delayIndex = 0;
      drainLimit = static_cast<int64_t>(rate
                                        * (effect == SlotEffect::DualDelay ? 17.
                                           : IsExpandedDelay(effect) ? 11.
                                           : IsDelay(effect) ? 9.
                                           // Sagittarius can chain eight full-length delays. Include modulation,
                                           // fixed offsets and filter settling before suspending a bypassed tank.
                                           : effect == SlotEffect::CosmicEcho ? 8.*(kEffectParameters[uCosmicTime].maximum*.001+.02)+1.
                                           : IsAirReverb(effect) ? 60.
                                           : IsReverb(effect) ? 4.
                                           // Silence refills the 4 s buffer, the last grains end and the
                                           // plate's longest decay (8 s) settles.
                                           : effect == SlotEffect::Granulator ? 15.
                                           : effect == SlotEffect::ShimmerReverb ? 4.
                                           : effect == SlotEffect::FrequencyShifter ? 9.
                                           : effect == SlotEffect::Doubler ? .1
                                           : effect == SlotEffect::Freeze ? 7.
                                           : effect == SlotEffect::Stutter ? 33.
                                           : effect == SlotEffect::Utility ? .045
                                           : effect == SlotEffect::Distortion || effect == SlotEffect::PedalDrive
                                               || effect == SlotEffect::PedalDistortion ? 1.
                                           : effect == SlotEffect::LoFiTape || IsOutboard(effect) || IsAirDynamics(effect) ? 1.
                                           // Dynamic EQ's slowest Release (1 s) settles a full-Range cut in about eight time constants.
                                           : effect == SlotEffect::DynamicEQ ? 8.
                                           // Transient Shaper's slow envelope recovers over at most 500 ms.
                                           : effect == SlotEffect::TransientShaper ? 5.
                                           // Multiband's slowest release is 2.8 s at Time 1000%.
                                           : effect == SlotEffect::Multiband ? 20.
                                           // Pedal Comp's release memory lets go over 800 ms.
                                           : effect == SlotEffect::PedalComp ? 6.
                                           // Detectors settle on silence (slowest release: 800 ms), so
                                           // re-enabling never resumes a stale gain reduction or envelope.
                                           : effect == SlotEffect::NoiseGate || effect == SlotEffect::SmoothComp
                                               || effect == SlotEffect::PunchComp || effect == SlotEffect::Limiter
                                               || effect == SlotEffect::AutoSwell || effect == SlotEffect::SynthFilter ? 3.
                                           : IsModulation(effect) ? .07
                                           : effect == SlotEffect::JunoChorus ? .07
                                           // Every other processor (EQ, filters, oversampling, NAM)
                                           // flushes its short history before suspending, so re-enabling
                                           // never replays audio from the moment it was bypassed.
                                                                  : .1));
      if (ir)
      {
        if (ir->NeedsReset(rate, block))
          ir = std::make_unique<AdaptiveIR>(ir->GetData(), rate, block, ir->GetMode());
        irWet.Reset(rate);
        drainLimit = static_cast<int64_t>(ir->Length()) + static_cast<int64_t>(rate * 1.5);
      }
      drainFrames = drainLimit; // Fresh histories already contain silence.
      if (!right) right = std::make_unique<RightChannel>();
      right->effect = effect;
      if (ir)
      {
        if (!right->ir || right->ir->NeedsReset(rate, block) || right->ir->GetMode() != ir->GetMode())
          right->ir = std::make_unique<AdaptiveIR>(rightIRData ? *rightIRData : ir->GetData(), rate, block, ir->GetMode());
      }
      else
        right->ir.reset();
      right->Reset(rate, block, latency);
      drainLimit = std::max(drainLimit, right->drainLimit);
      // Include the final fade within the host's per-effect tail estimate, not after it.
      // Delays can be silent between repeats for a whole ring; reverb tails decay steadily.
      trailLimit = static_cast<int64_t>(rate * (IsAirReverb(effect) || effect == SlotEffect::CosmicEcho ? 60.
                                              : IsReverb(effect) ? 45. : 30.));
      trailFade = std::max<int64_t>(1, static_cast<int64_t>(rate));
      trailHold = IsReverb(effect) || effect == SlotEffect::ShimmerReverb ? static_cast<int64_t>(rate * 2.) : drainLimit;
      trailFrames = quietFrames = 0;
    }
    bool IsLoaded() const
    { return model != nullptr || (effect == SlotEffect::CabinetIR ? ir != nullptr : effect != SlotEffect::NAM); }
    NAMModelInfo Info() const
    {
      NAMModelInfo info;
      info.loaded = effect == SlotEffect::CabinetIR ? ir != nullptr : effect != SlotEffect::NAM;
      if (!model)
        return info;
      info.loaded = true;
      info.rate = model->GetEncapsulatedSampleRate();
      info.hasInput = model->HasInputLevel();
      info.hasOutput = model->HasOutputLevel();
      info.hasLoudness = model->HasLoudness();
      info.slimmable = model->GetSlimmableModel() != nullptr;
      if (info.hasInput)
        info.input = model->GetInputLevel();
      if (info.hasOutput)
        info.output = model->GetOutputLevel();
      if (info.hasLoudness)
        info.loudness = model->GetLoudness();
      return info;
    }
  };

  noise_print::Profile GetNoiseProfile(uint64_t epoch, const noise_print::Profile& fallback) const
  { return mNoiseSnapshot.Read(epoch, fallback); }
  uint64_t NoiseSequence() const { return mNoiseSnapshot.Sequence(); }
  void Publish(std::unique_ptr<Prepared> item, bool canWaitForReset = false)
  {
    const int nextLatency = item ? item->latency : 0;
    if (canWaitForReset && nextLatency != mActiveLatency.load(std::memory_order_acquire))
    {
      mWaitForReset.store(true, std::memory_order_release);
      mExchange.Publish(std::move(item));
    }
    else
    {
      mExchange.Publish(std::move(item));
      mWaitForReset.store(false, std::memory_order_release);
    }
  }
  void Collect() { mExchange.Collect(); }
  // Frees the processor while audio cannot use this stage (host stopped or inactive grid).
  void ClearStopped()
  {
    mExchange.ClearStopped();
    mActiveLatency.store(0, std::memory_order_release);
    mWaitForReset.store(false, std::memory_order_release);
    mFirst = true;
    mMeter = {};
  }
  void ResetStopped(double rate, int block)
  {
    Collect();
    mExchange.Adopt();
    Collect();
    auto* p = mExchange.Active();
    if (p)
      p->Reset(rate, block);
    mActiveLatency.store(p ? p->latency : 0, std::memory_order_release);
    mWaitForReset.store(false, std::memory_order_release);
    mStep = 1. / std::max(1., rate * .005); // 5 ms linear ramps
    mSwitchGain = 1.;
    mMix = 0.;
    mFeed = 1.;
    mTrail = 0.;
    mFirst = true;
    mMeter = {};
    if (mCapture) mCapture->Clear();
  }
  // Audio only (or while processing is stopped): route this stage's output into the shared capture.
  void AttachCapture(SignalCapture* capture)
  {
    if (capture && capture != mCapture) capture->Clear();
    mCapture = capture;
  }
  // Audio thread only. Editor telemetry must not change processor histories.
  void EnableMetering(bool enabled)
  {
    mMeteringEnabled = enabled;
    if (!enabled) mMeter = {};
  }
  SlotMeterData TakeMeter()
  {
    auto result = mMonitoring ? mMeter : SlotMeterData{};
    if (auto* p = mExchange.Active(); mMonitoring && p && p->effect == SlotEffect::LoFiTape)
      result.lofi = p->lofi.History();
    mMeter = {};
    return result;
  } // audio only
  // Latest full output window for the editor's spectrum. Audio only; only the
  // selected slot's stage has the capture attached.
  bool TakeSignalWindow(grid_eq_signal::Window& window) const
  {
    const auto* p = mExchange.Active();
    return mCapture && mMonitoring && p && ShowsSignalSpectrum(p->effect) && mCapture->Take(window);
  }
  int Latency() const
  {
    auto* p = mExchange.Active();
    return p ? p->latency : 0;
  }
  // Control thread only, while processing is stopped (e.g. OnReset).
  bool GetIRSourceStopped(AdaptiveIR::IRData& left, std::vector<float>& right, bool& longIR) const
  {
    const auto* p = mExchange.Active();
    if (!p || p->effect != SlotEffect::CabinetIR || !p->ir) return false;
    left = p->ir->GetData();
    right = p->rightIRData ? p->rightIRData->mRawAudio : std::vector<float>{};
    longIR = p->ir->GetMode() == AdaptiveIR::Mode::Long;
    return true;
  }
  // Called exclusively by audio, or while host processing is stopped.
  double* Process(double* samples, int frames, bool enabled, double inputDB, double trimDB,
                  const DynamicsControls& dynamics = {}, const EQControls& eq = {}, const UtilityControls& utility = {},
                  double* rightSamples = nullptr)
  {
    const bool switching = mExchange.HasPending() && !mWaitForReset.load(std::memory_order_acquire);
    if (switching && mSwitchGain <= 0. && mExchange.Adopt())
    {
      const auto* adopted = mExchange.Active();
      mActiveLatency.store(adopted ? adopted->latency : 0, std::memory_order_release);
      mFirst = true;
      mFeed = 0.; // The new processor's histories start silent: fade its input in.
      mTrail = 0.; // A replaced module's tail leaves with its switch fade.
    }
    auto* p = mExchange.Active();
    const bool loaded = p && p->IsLoaded();
    mMonitoring = mMeteringEnabled && enabled && loaded;
    if (!mMonitoring) mMeter = {};
    float inputPeak = 0.f;
    if (mMonitoring)
    {
      for (int i = 0; i < frames; ++i)
      {
        const double x = std::clamp(rightSamples ? .5 * (samples[i] + rightSamples[i]) : samples[i], -1.e6, 1.e6);
        inputPeak = std::max(inputPeak, static_cast<float>(std::abs(x)));
        mMeter.inputEnergy += x * x;
      }
      mMeter.frames += frames;
    }
    mMeter.input = std::max(mMeter.input, inputPeak);
    if (p && loaded)
    {
      if (p->effect != SlotEffect::NAM)
        inputDB = trimDB = 0.; // Built-ins use their dedicated controls.
      if (p->effect == SlotEffect::CabinetIR)
      {
        inputDB = utility.irInput;
        trimDB = eq.irLevel + 18.; // Undo the reused convolver's fixed -18 dB; knob is actual IR gain.
      }
      const double irMixTarget = std::clamp(utility.irMix * .01, 0., 1.);
      const double inTarget = mInGainMemo(inputDB * .05, [](double x) { return std::pow(10., x); });
      const double outTarget = mOutGainMemo(trimDB * .05, [](double x) { return std::pow(10., x); });
      if (mFirst)
      {
        mIn = inTarget;
        mOut = outTarget;
        mMix = enabled ? 1. : 0.;
        mIRMix = irMixTarget;
        mFirst = false;
      }
      const double inDelta = (inTarget - mIn) / std::max(1, frames);
      for (int i = 0; i < frames; ++i)
      {
        mIn += inDelta;
        p->input[i] = (rightSamples && p->model ? .5 * (samples[i] + rightSamples[i]) : samples[i]) * mIn;
        if (rightSamples)
        {
          auto& r = *p->right;
          r.input[i] = rightSamples[i] * mIn;
          r.dry[i] = p->latency ? r.delay[r.delayIndex] : rightSamples[i];
          r.delay[r.delayIndex] = rightSamples[i];
          if (++r.delayIndex == r.delay.size()) r.delayIndex = 0; // No per-sample integer division.
        }
        p->dry[i] = p->latency ? p->delay[p->delayIndex] : samples[i];
        p->delay[p->delayIndex] = samples[i];
        if (++p->delayIndex == p->delay.size()) p->delayIndex = 0;
      }
      // Trails: a bypassed delay or reverb stops taking input but keeps its feedback,
      // and its tail plays over the dry signal until it decays. It then drains as below.
      const bool trailing = !enabled && mTrail > 0.;
      // Drain temporal histories with silence after bypass, then suspend them.
      // Delay feedback stops once fully muted: one 8.1 s ring plus filter settling
      // clears its history without nine minutes of inaudible repeat processing.
      // This prevents an old echo from resurfacing when a slot is re-enabled.
      const bool draining = !enabled && mMix <= 0. && !trailing && p->drainFrames < p->drainLimit;
      double trailTarget = 0., trailStep = mStep;
      if (enabled || mMix > 0. || trailing || draining)
      {
        // Fade the processor's input in after it was suspended, drained or replaced, and
        // out as draining or a trail starts. Delay lines and filters then never store a
        // hard step that they would replay after the slot's own 5 ms crossfade has finished.
        const bool feedOff = draining || trailing;
        if (feedOff && mFeed <= 0.)
        {
          std::fill_n(p->input.data(), frames, 0.);
          if (rightSamples)
            std::fill_n(p->right->input.data(), frames, 0.);
        }
        else if (feedOff || mFeed < 1.)
          for (int i = 0; i < frames; ++i)
          {
            mFeed = Approach(mFeed, feedOff ? 0. : 1., mStep);
            p->input[i] *= mFeed;
            if (rightSamples) p->right->input[i] *= mFeed;
          }
        if (draining)
          p->drainFrames += frames;
        else
          p->drainFrames = 0;
        double* in = p->input.data();
        double* out = p->wet.data();
        if (p->model)
        {
          p->model->process(&in, &out, frames);
          p->eq.Process(out, frames, false, eq);
          auto cuts = eq;
          cuts.presence = 0.;
          p->namCuts.Process(out, frames, true, cuts);
        }
        else
        {
          if (p->effect == SlotEffect::NoisePrint)
          {
            std::copy_n(in, frames, out);
            if (rightSamples)
              std::copy_n(p->right->input.data(), frames, p->right->wet.data());
            p->noise.Process(out, rightSamples ? p->right->wet.data() : nullptr, frames, utility.noise);
            if (p->noise.TakeChanged())
              mNoiseSnapshot.Publish(p->noise.GetProfile(), p->noiseEpoch);
            if (mMonitoring)
            {
              mMeter.reduction = std::max(mMeter.reduction, static_cast<float>(p->noise.Reduction() * mMix));
              mMeter.cleaned = std::max(mMeter.cleaned, static_cast<float>(p->noise.CleanPeak()));
            }
          }
          else if (p->effect == SlotEffect::AIGate)
          {
            std::copy_n(in, frames, out);
            if (rightSamples)
              std::copy_n(p->right->input.data(), frames, p->right->wet.data());
            auto controls = utility.aiGate;
            controls.gateThreshold = utility.noise.gateThreshold; // Shared with Smart Gate's final gate.
            controls.gateRelease = utility.noise.gateRelease;
            p->aiGate.Process(out, rightSamples ? p->right->wet.data() : nullptr, frames, controls);
            if (mMonitoring)
            {
              mMeter.reduction = std::max(mMeter.reduction, static_cast<float>(p->aiGate.Reduction() * mMix));
              mMeter.cleaned = std::max(mMeter.cleaned, static_cast<float>(p->aiGate.CleanPeak()));
            }
          }
          else if (UsesCoupledStereo(p->effect))
          {
            std::copy_n(in, frames, out);
            double* r = rightSamples ? p->right->wet.data() : nullptr;
            if (r) std::copy_n(p->right->input.data(), frames, r);
            if (p->effect == SlotEffect::NoiseGate || p->effect == SlotEffect::SmoothComp || p->effect == SlotEffect::PunchComp)
            {
              p->dynamics.Process(out, frames, p->effect, dynamics,
                                  p->dynamicsMonitor.empty() ? nullptr : p->dynamicsMonitor.data(),
                                  r, r && !p->right->dynamicsMonitor.empty() ? p->right->dynamicsMonitor.data() : nullptr);
              if (mMonitoring) mMeter.reduction = std::max(mMeter.reduction, static_cast<float>(p->dynamics.PeakReduction() * mMix));
            }
            else if (p->effect == SlotEffect::AutoSwell) p->swell.Process(out, frames, utility, r);
            else if (p->effect == SlotEffect::Limiter)
            {
              p->limiter.Process(out, r, frames, utility);
              if (mMonitoring) mMeter.reduction = std::max(mMeter.reduction, static_cast<float>(p->limiter.Reduction() * mMix));
            }
            else if (p->effect == SlotEffect::Rotary) p->rotary.Process(out, r, frames, utility);
            else if (p->effect == SlotEffect::Octaver) p->octaver.Process(out, r, frames, utility);
            else if (p->effect == SlotEffect::Doubler) p->doubler.Process(out, r, frames, utility);
            else if (p->effect == SlotEffect::DualDelay) p->dualDelay.Process(out, r, frames, utility, draining);
            else if (p->effect == SlotEffect::ShimmerReverb) p->shimmer.Process(out, r, frames, utility, draining);
            else if (p->effect == SlotEffect::JunoChorus) p->juno.Process(out, r, frames, utility);
            else if (p->effect == SlotEffect::FrequencyShifter)
              p->frequencyShifter.Process(out, r, frames, utility, draining);
            else if (p->effect == SlotEffect::LoFiTape) p->lofi.Process(out, r, frames, utility, draining);
            else if (p->effect == SlotEffect::MultiHeadDelay) p->multiHead.Process(out, r, frames, utility, draining);
            else if (p->effect == SlotEffect::DiffusedDelay) p->diffused.Process(out, r, frames, utility, draining);
            else if (p->expandedDelay) p->expandedDelay->Process(out, r, frames, utility, draining);
            else if (p->effect == SlotEffect::CosmicEcho) p->cosmic.Process(out, r, frames, utility, draining);
            else if (p->granulator) p->granulator->Process(out, r, frames, utility, draining);
            else if (IsAirDynamics(p->effect))
            {
              p->airDynamics.Process(out, r, frames, utility);
              if (mMonitoring) mMeter.reduction = std::max(mMeter.reduction, static_cast<float>(p->airDynamics.Reduction()*mMix));
            }
            else if (p->effect == SlotEffect::DynamicEQ)
            {
              p->dynamicEQ.Process(out, r, frames, utility, draining);
              if (mMonitoring) mMeter.reduction = std::max(mMeter.reduction, static_cast<float>(p->dynamicEQ.Reduction() * mMix));
            }
            else if (p->effect == SlotEffect::TransientShaper)
            {
              p->transientShaper.Process(out, r, frames, utility, draining);
              if (mMonitoring) mMeter.reduction = std::max(mMeter.reduction, static_cast<float>(p->transientShaper.Reduction() * mMix));
            }
            else if (p->effect == SlotEffect::PitchShift)
              p->pitchShift.Process(out, r, frames, utility);
            else if (p->effect == SlotEffect::GraphicEQ)
              p->graphicEQ.Process(out, r, frames, utility);
            else if (p->effect == SlotEffect::Multiband)
            {
              p->multiband.Process(out, r, frames, utility);
              if (mMonitoring)
              {
                mMeter.reduction = std::max(mMeter.reduction, static_cast<float>(p->multiband.Reduction() * mMix));
                for (int b = 0; b < SlotMultiband::kBands; ++b)
                {
                  const auto gain = static_cast<float>(p->multiband.BandGain()[b]);
                  if (std::abs(gain) > std::abs(mMeter.bandGain[b])) mMeter.bandGain[b] = gain;
                  mMeter.bandLevel[b] = std::max(mMeter.bandLevel[b], static_cast<float>(p->multiband.BandLevel()[b]));
                }
              }
            }
            else if (p->effect == SlotEffect::PedalComp)
            {
              p->pedalComp.Process(out, r, frames, utility);
              if (mMonitoring) mMeter.reduction = std::max(mMeter.reduction, static_cast<float>(p->pedalComp.Reduction() * mMix));
            }
            else if (IsOutboard(p->effect))
            {
              p->outboard.Process(out, r, frames, utility);
              if (mMonitoring) mMeter.reduction = std::max(mMeter.reduction, static_cast<float>(p->outboard.Reduction()*mMix));
            }
            else if (IsAirReverb(p->effect)) p->airReverb.Process(out, r, frames, utility, draining);
            else if (IsReverb(p->effect)) p->reverb.Process(out, r, frames, utility, draining);
            else p->capture.Process(out, r, frames, p->effect, utility, draining);
          }
          else if (p->effect == SlotEffect::SpringReverb)
          {
            std::copy_n(in, frames, out);
            if (rightSamples)
              std::copy_n(p->right->input.data(), frames, p->right->wet.data());
            p->spring.Process(out, rightSamples ? p->right->wet.data() : nullptr, frames, utility, draining);
          }
          else if (rightSamples && (IsDelay(p->effect) || IsReverb(p->effect)))
          {
            auto* rightOut = p->right->wet.data();
            if (IsDelay(p->effect))
            {
              std::copy_n(in, frames, out);
              std::copy_n(p->right->input.data(), frames, rightOut);
              p->echo.Process(out, frames, p->effect, utility, &p->right->dsp->echo, rightOut, draining);
            }
          }
          else
          {
            ProcessBuiltin(p, p->input, p->wet, p->dynamicsMonitor, p->ir.get(), p->irWet, p->effect, false,
                           frames, dynamics, eq, utility, draining);
            if (rightSamples)
              ProcessBuiltin(p->right->dsp.get(), p->right->input, p->right->wet, p->right->dynamicsMonitor,
                             p->right->ir.get(), p->right->irWet, p->effect, true, frames, dynamics, eq, utility, draining);
            else if (p->rightIRData)
            {
              // The same mono excitation through both IRs equals their averaged response.
              std::copy_n(p->input.data(), frames, p->right->input.data());
              ProcessBuiltin(p->right->dsp.get(), p->right->input, p->right->wet, p->right->dynamicsMonitor,
                             p->right->ir.get(), p->right->irWet, p->effect, true, frames, dynamics, eq, utility, draining);
              for (int i = 0; i < frames; ++i)
                out[i] = .5 * (out[i] + p->right->wet[i]);
            }
          }
          if (p->effect == SlotEffect::Utility)
            p->utilityStereo.Process(out, rightSamples ? p->right->wet.data() : nullptr, frames, utility);
          if (p->effect == SlotEffect::ToneEQ || p->effect == SlotEffect::CutEQ || p->effect == SlotEffect::ConsoleEQ)
          {
            p->eqOutput.Target({utility.extra.eqOutput}, uEQOutput);
            for (int i = 0; i < frames; ++i)
            {
              const double gain = p->eqOutputGain(p->eqOutput.Tick()[0] * .05, [](double x) { return std::pow(10., x); });
              out[i] *= gain;
              if (rightSamples) p->right->wet[i] *= gain;
            }
          }
        }
        if (p->model && rightSamples)
          std::copy_n(out, frames, p->right->wet.data());
        if (mMonitoring && p->effect == SlotEffect::SynthFilter)
        {
          mMeter.filterCutoff[0] = static_cast<float>(p->filter.AppliedCutoff());
          mMeter.filterQ[0] = static_cast<float>(p->filter.AppliedResonance());
          mMeter.filterCutoff[1] = rightSamples && p->right->dsp
            ? static_cast<float>(p->right->dsp->filter.AppliedCutoff()) : mMeter.filterCutoff[0];
          mMeter.filterQ[1] = rightSamples && p->right->dsp
            ? static_cast<float>(p->right->dsp->filter.AppliedResonance()) : mMeter.filterQ[0];
        }
        if (mMonitoring && p->effect == SlotEffect::VolumePedal)
        {
          mMeter.pedalPosition = static_cast<float>(p->volumePedal.Position());
          mMeter.pedalGain = static_cast<float>(p->volumePedal.CurrentGain());
        }
        if (utility.trails && IsTrailEffect(p->effect))
        {
          if (enabled)
          {
            p->trailFrames = p->quietFrames = 0;
            trailTarget = 1.; // Inaudible while the slot plays; ready when it is bypassed.
          }
          else if (trailing)
          {
            // With no input, the module's output is its tail alone.
            double peak = 0.;
            for (int i = 0; i < frames; ++i)
              peak = std::max({peak, std::abs(p->wet[i]), rightSamples ? std::abs(p->right->wet[i]) : 0.});
            p->trailFrames += frames;
            p->quietFrames = peak < 1.e-5 ? p->quietFrames + frames : 0; // -100 dBFS
            trailTarget = p->quietFrames < p->trailHold && p->trailFrames < p->trailLimit ? 1. : 0.;
            // A tail still ringing fades during its final second, including partial blocks.
            if (p->trailFrames >= p->trailLimit - p->trailFade) trailStep = 1. / p->trailFade;
          }
        }
      }
      else
        mFeed = 0.; // Suspended: resume with a fade-in.
      const bool limitFading = trailing && p->trailFrames >= p->trailLimit - p->trailFade;
      const double outDelta = (outTarget - mOut) / std::max(1, frames);
      const bool spectrum = mCapture && mMonitoring && ShowsSignalSpectrum(p->effect);
      for (int i = 0; i < frames; ++i)
      {
        mOut += outDelta;
        mMix = Approach(mMix, enabled ? 1. : 0., mStep);
        mIRMix = Approach(mIRMix, irMixTarget, mStep);
        const double mix = p->ir ? mMix * mIRMix : mMix;
        // A trail keeps the module output while dry fades in; with mTrail at 0 this is mix.
        mTrail = Approach(mTrail, trailTarget, trailStep);
        if (limitFading)
        {
          const auto remaining = std::max<int64_t>(0, p->trailLimit - p->trailFrames + frames - i - 1);
          mTrail = std::min(mTrail, static_cast<double>(remaining) / p->trailFade);
        }
        const double wetMix = mix + mTrail * (1. - mix);
        mSwitchGain = Approach(mSwitchGain, switching ? 0. : 1., mStep);
        if (rightSamples)
          rightSamples[i] = (p->right->dry[i] * (1. - mix) + p->right->wet[i] * mOut * wetMix) * mSwitchGain;
        samples[i] = (p->dry[i] * (1. - mix) + p->wet[i] * mOut * wetMix) * mSwitchGain;
        if (spectrum)
          mCapture->Push(samples[i], rightSamples ? rightSamples[i] : samples[i]);
        if (mMonitoring)
        {
          const double x = std::clamp(rightSamples ? .5 * (samples[i] + rightSamples[i]) : samples[i], -1.e6, 1.e6);
          mMeter.output = std::max(mMeter.output, static_cast<float>(std::abs(x)));
          mMeter.outputEnergy += x * x;
        }
        if (mMonitoring && !p->dynamicsMonitor.empty())
        {
          const double l=p->dry[i]*(1.-mMix)+p->dynamicsMonitor[i]*mMix;
          const double r=rightSamples && !p->right->dynamicsMonitor.empty()
            ? p->right->dry[i]*(1.-mMix)+p->right->dynamicsMonitor[i]*mMix : l;
          mMeter.beforeMakeup=std::max(mMeter.beforeMakeup,static_cast<float>(std::min(1.e6,std::abs(.5*(l+r)*mSwitchGain))));
        }
      }
    }
    else
    {
      if (mFirst)
      {
        mMix = 0.;
        mFirst = false;
      }
      if (!switching && mSwitchGain >= 1.)
      {
        return samples; // Empty grid slots need no per-sample work once switching has settled.
      }
      for (int i = 0; i < frames; ++i)
      {
        mSwitchGain = Approach(mSwitchGain, switching ? 0. : 1., mStep);
        samples[i] *= mSwitchGain;
        if (rightSamples)
          rightSamples[i] *= mSwitchGain;
      }
    }
    return samples;
  }

private:
  void ProcessBuiltin(ChannelDSP* dsp, const std::vector<double>& inputVec, std::vector<double>& wetVec,
                      std::vector<double>& dynamicsMonitor, AdaptiveIR* ir, SlotIR& irWet,
                      SlotEffect effect, bool rightChannel, int frames,
                      const DynamicsControls& dynamics, const EQControls& eq,
                      const UtilityControls& utility, bool draining)
  {
    double* in = const_cast<double*>(inputVec.data());
    double* out = wetVec.data();
    if (ir)
    {
      auto* convolved = ir->Process(&in, 1, frames);
      std::copy_n(convolved[0], frames, out);
      irWet.Process(out, frames, utility);
    }
    else
    {
      std::copy_n(in, frames, out);
      if (dsp)
      {
        if (effect == SlotEffect::ToneEQ || effect == SlotEffect::CutEQ)
          dsp->eq.Process(out, frames, effect == SlotEffect::CutEQ, eq);
        else if (effect == SlotEffect::ParametricEQ)
          dsp->parametric.Process(out, frames, utility, eq);
        else if (effect == SlotEffect::Boost)
          dsp->boost.Process(out, frames, utility, eq);
        else if (effect == SlotEffect::TubeBoost)
          dsp->tube.Process(out, in, frames, utility, eq);
        else if (effect == SlotEffect::Distortion)
          dsp->distortion.Process(out, frames, utility);
        else if (effect == SlotEffect::PedalDrive)
          dsp->pedal.Process(out, frames, utility);
        else if (effect == SlotEffect::PedalDistortion)
          dsp->pedalDistortion.Process(out, frames, utility);
        else if (effect == SlotEffect::SynthFilter)
          dsp->filter.Process(out, frames, utility);
        else if (effect == SlotEffect::AutoSwell)
          dsp->swell.Process(out, frames, utility);
        else if (effect == SlotEffect::VolumePedal)
          dsp->volumePedal.Process(out, frames, utility);
        else if (effect == SlotEffect::UniVibe)
          dsp->vibe.Process(out, frames, utility, rightChannel);
        else if (effect == SlotEffect::Bitcrusher || effect == SlotEffect::RingMod)
          dsp->texture.Process(out, frames, effect, utility);
        else if (IsDelay(effect))
          dsp->echo.Process(out, frames, effect, utility, nullptr, nullptr, draining);
        else if (IsModulation(effect))
          dsp->modulation.Process(out, frames, effect, utility, rightChannel);
        else if (effect == SlotEffect::Wah)
          dsp->wah.Process(out, frames, utility);
        else if (effect == SlotEffect::Tape)
          dsp->tape.Process(out, frames, utility);
        else if (effect != SlotEffect::Utility)
        {
          dsp->dynamics.Process(out, frames, effect, dynamics, dynamicsMonitor.empty() ? nullptr : dynamicsMonitor.data());
          if (mMonitoring) mMeter.reduction = std::max(mMeter.reduction, static_cast<float>(dsp->dynamics.PeakReduction() * mMix));
        }
      }
    }
  }
  static double Approach(double current, double target, double step)
  { return current < target ? std::min(target, current + step) : std::max(target, current - step); }
  noise_print::Snapshot mNoiseSnapshot;
  RealtimeExchange<Prepared> mExchange;
  std::atomic<int> mActiveLatency{0};
  std::atomic<bool> mWaitForReset{false};
  double mMix = 0., mIRMix = 1., mIn = 1., mOut = 1., mSwitchGain = 1., mFeed = 1., mTrail = 0., mStep = 1. / 240.;
  grid_fx::Memo mInGainMemo, mOutGainMemo;
  bool mFirst = true;
  bool mMonitoring = false, mMeteringEnabled = true;
  SlotMeterData mMeter;
  SignalCapture* mCapture = nullptr; // Shared, owned by the plugin; set only on the selected slot.
};
