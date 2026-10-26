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
#include "SlotCosmicEcho.h"
#include "StereoUtility.h"
#include "SlotGuitarEffects.h"
#include "RoadmapEffects.h"
#include "SlotHQEffects.h"
#include "EQSignalSpectrum.h"
#include "SlotDistortion.h"
#include "SlotPedalDrive.h"
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
  // Mono-sum energy and frame count since the last take, for the editor's RMS level strip.
  double inputEnergy = 0., outputEnergy = 0.;
  int frames = 0;
  LoFiTapeHistory lofi;
  std::array<float, 2> filterCutoff{}, filterQ{};
};

// Effects whose editor overlays a live output spectrum on the response curve: every
// EQ / Filter module, the NAM post-EQ and Outboard Channel's cuts, all at one resolution.
inline bool ShowsSignalSpectrum(SlotEffect effect)
{
  return effect == SlotEffect::NAM || effect == SlotEffect::ToneEQ || effect == SlotEffect::CutEQ
         || effect == SlotEffect::ParametricEQ || effect == SlotEffect::SynthFilter
         || effect == SlotEffect::OutboardChannel || effect == SlotEffect::ConsoleEQ || effect == SlotEffect::Wah
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
  struct Prepared
  {
    std::unique_ptr<ResamplingNAM> model;
    SlotEffect effect = SlotEffect::NAM;
    SlotDynamics dynamics;
    noise_print::Processor noise;
    uint64_t noiseEpoch = 0;
    SlotEQ eq, namCuts;
    SlotParametricEQ parametric;
    SlotBoost boost;
    SlotTubeBoost tube;
    SlotDistortion distortion;
    SlotPedalDrive pedal;
    SlotEnvelopeFilter filter;
    SlotAutoSwell swell;
    SlotDelay echo;
    SlotReverb reverb;
    SlotAirReverb airReverb;
    SlotOutboard outboard;
    SlotAirDynamics airDynamics;
    SlotMultiHeadDelay multiHead;
    SlotDiffusedDelay diffused;
    SlotCosmicEcho cosmic;
    SlotModulation modulation;
    SlotTape tape;
    SlotLoFiTape lofi;
    SlotSpring spring;
    SlotWah wah;
    grid_roadmap::Limiter limiter;
    grid_roadmap::Rotary rotary;
    grid_roadmap::Vibe vibe;
    grid_roadmap::Octaver octaver;
    grid_roadmap::Doubler doubler;
    grid_roadmap::Capture capture;
    grid_roadmap::DualDelay dualDelay;
    grid_roadmap::Texture texture;
    grid_hq::Shimmer shimmer;
    grid_hq::JunoChorus juno;
    grid_hq::FrequencyShifter frequencyShifter;
    StereoUtility utilityStereo;
    grid_fx::SmoothControls<1> eqOutput;
    grid_fx::Memo eqOutputGain;
    std::unique_ptr<Prepared> right;
    bool rightChannel = false;
    std::unique_ptr<AdaptiveIR> ir;
    SlotIR irWet;
    std::vector<double> input, wet, dry, delay, dynamicsMonitor;
    size_t delayIndex = 0;
    int latency = 0;
    int64_t drainFrames = 0, drainLimit = 0;
    void Reset(double rate, int block)
    {
      utilityStereo.Reset(rate); eqOutput.Reset(rate);
      dynamics.Reset(rate);
      if (effect == SlotEffect::NoisePrint && !rightChannel)
        noise.Reset(rate);
      eq.Reset(rate);
      parametric.Reset(rate);
      boost.Reset(rate);
      if (effect == SlotEffect::TubeBoost)
        tube.Reset(rate);
      if (effect == SlotEffect::Distortion)
        distortion.Reset(rate);
      if (effect == SlotEffect::PedalDrive)
        pedal.Reset(rate);
      if (effect == SlotEffect::SynthFilter)
        filter.Reset(rate);
      if (effect == SlotEffect::AutoSwell)
        swell.Reset(rate);
      if (IsDelay(effect) && !UsesCoupledStereo(effect))
        echo.Reset(rate);
      if (!rightChannel)
      {
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
        if (effect == SlotEffect::CosmicEcho) cosmic.Reset(rate);
        if (IsOutboard(effect)) outboard.Reset(rate, effect);
        if (IsAirDynamics(effect)) airDynamics.Reset(rate, effect);
        if (IsAirReverb(effect)) airReverb.Reset(rate, effect);
        else if (IsReverb(effect) && effect != SlotEffect::SpringReverb) reverb.Reset(rate, effect);
      }
      if (effect == SlotEffect::UniVibe) vibe.Reset(rate);
      if (effect == SlotEffect::Bitcrusher || effect == SlotEffect::RingMod) texture.Reset(rate);
      if (effect == SlotEffect::SpringReverb && !rightChannel)
        spring.Reset(rate);
      if (effect == SlotEffect::Wah)
        wah.Reset(rate);
      if (IsModulation(effect))
        modulation.Reset(rate);
      if (effect == SlotEffect::Tape)
        tape.Reset(rate);
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
      if (effect == SlotEffect::SmoothComp || effect == SlotEffect::PunchComp) dynamicsMonitor.assign(block,0.);
      delay.assign(std::max(1, latency), 0.);
      delayIndex = 0;
      drainLimit = static_cast<int64_t>(rate
                                        * (effect == SlotEffect::DualDelay ? 17.
                                           : IsDelay(effect) ? 9.
                                           // Sagittarius can chain eight full-length delays. Include modulation,
                                           // fixed offsets and filter settling before suspending a bypassed tank.
                                           : effect == SlotEffect::CosmicEcho ? 8.*(kEffectParameters[uCosmicTime].maximum*.001+.02)+1.
                                           : IsReverb(effect) ? 60.
                                           : effect == SlotEffect::ShimmerReverb ? 30.
                                           : effect == SlotEffect::FrequencyShifter ? 9.
                                           : effect == SlotEffect::Doubler ? .1
                                           : effect == SlotEffect::Freeze ? 7.
                                           : effect == SlotEffect::Stutter ? 33.
                                           : effect == SlotEffect::Utility ? .045
                                           : effect == SlotEffect::Distortion || effect == SlotEffect::PedalDrive ? 1.
                                           : effect == SlotEffect::LoFiTape || IsOutboard(effect) || IsAirDynamics(effect) ? 1.
                                           : IsModulation(effect) ? .07
                                           : effect == SlotEffect::JunoChorus ? .07
                                                                  : 0.));
      if (ir)
      {
        ir = std::make_unique<AdaptiveIR>(ir->GetData(), rate, block, ir->GetMode());
        irWet.Reset(rate);
        drainLimit = static_cast<int64_t>(ir->Length()) + static_cast<int64_t>(rate * 1.5);
      }
      drainFrames = drainLimit; // Fresh histories already contain silence.
      if (!rightChannel)
      {
        right = std::make_unique<Prepared>();
        right->rightChannel = true;
        right->effect = effect;
        if (ir)
          right->ir = std::make_unique<AdaptiveIR>(ir->GetData(), rate, block, ir->GetMode());
        right->Reset(rate, block);
        // Mono NAM has one inference; the second history is for stereo dry bypass.
        right->latency = latency;
        right->delay.assign(std::max(1, latency), 0.);
      }
    }
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
  void Publish(std::unique_ptr<Prepared> item, bool waitForReset = false)
  {
    if (waitForReset)
      mWaitForReset.store(true);
    mExchange.Publish(std::move(item));
  }
  void Collect() { mExchange.Collect(); }
  // Frees the processor while audio cannot use this stage (host stopped or inactive grid).
  void ClearStopped()
  {
    mExchange.ClearStopped();
    mWaitForReset.store(false);
    mFirst = true;
    mMeter = {};
  }
  void ResetStopped(double rate, int block)
  {
    Collect();
    mExchange.Adopt();
    Collect();
    if (auto* p = mExchange.Active())
      p->Reset(rate, block);
    mWaitForReset.store(false);
    mStep = 1. / std::max(1., rate * .005); // 5 ms linear ramps
    mSwitchGain = 1.;
    mMix = 0.;
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
  // Called exclusively by audio, or while host processing is stopped.
  double* Process(double* samples, int frames, bool enabled, double inputDB, double trimDB,
                  const DynamicsControls& dynamics = {}, const EQControls& eq = {}, const UtilityControls& utility = {},
                  double* rightSamples = nullptr)
  {
    const bool switching = mExchange.HasPending() && !mWaitForReset.load();
    if (switching && mSwitchGain <= 0.)
      mExchange.Adopt();
    auto* p = mExchange.Active();
    const auto info = p ? p->Info() : NAMModelInfo{};
    mMonitoring = enabled && info.loaded;
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
    if (p && p->effect != SlotEffect::NAM)
      inputDB = trimDB = 0.; // Built-ins use their dedicated controls.
    if (p && p->effect == SlotEffect::CabinetIR)
    {
      inputDB = utility.irInput;
      trimDB = eq.irLevel + 18.; // Undo the reused convolver's fixed -18 dB; knob is actual IR gain.
    }
    const double irMixTarget = std::clamp(utility.irMix * .01, 0., 1.);
    const double inTarget = std::pow(10., inputDB * .05), outTarget = std::pow(10., trimDB * .05);
    if (mFirst)
    {
      mIn = inTarget;
      mOut = outTarget;
      mMix = enabled && info.loaded ? 1. : 0.;
      mIRMix = irMixTarget;
      mFirst = false;
    }
    if (p && info.loaded)
    {
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
      // Drain temporal histories with silence after bypass, then suspend them.
      // Delay feedback stops once fully muted: one 8.1 s ring plus filter settling
      // clears its history without nine minutes of inaudible repeat processing.
      // This prevents an old echo from resurfacing when a slot is re-enabled.
      const bool draining = !enabled && mMix <= 0. && p->drainFrames < p->drainLimit;
      if (enabled || mMix > 0. || draining)
      {
        if (draining)
        {
          std::fill_n(p->input.data(), frames, 0.);
          if (rightSamples)
            std::fill_n(p->right->input.data(), frames, 0.);
          p->drainFrames += frames;
        }
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
              mMeter.reduction = std::max(mMeter.reduction, static_cast<float>(p->noise.Reduction() * mMix));
          }
          else if (UsesCoupledStereo(p->effect))
          {
            std::copy_n(in, frames, out);
            double* r = rightSamples ? p->right->wet.data() : nullptr;
            if (r) std::copy_n(p->right->input.data(), frames, r);
            if (p->effect == SlotEffect::Limiter)
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
            else if (p->effect == SlotEffect::CosmicEcho) p->cosmic.Process(out, r, frames, utility, draining);
            else if (IsAirDynamics(p->effect))
            {
              p->airDynamics.Process(out, r, frames, utility);
              if (mMonitoring) mMeter.reduction = std::max(mMeter.reduction, static_cast<float>(p->airDynamics.Reduction()*mMix));
            }
            else if (IsOutboard(p->effect))
            {
              p->outboard.Process(out, r, frames, utility);
              if (mMonitoring) mMeter.reduction = std::max(mMeter.reduction, static_cast<float>(p->outboard.Reduction()*mMix));
            }
            else if (IsAirReverb(p->effect)) p->airReverb.Process(out, r, frames, utility, draining);
            else if (IsReverb(p->effect)) p->reverb.Process(out, r, frames, utility);
            else p->capture.Process(out, r, frames, p->effect, utility, draining);
          }
          else if (p->effect == SlotEffect::SpringReverb)
          {
            std::copy_n(in, frames, out);
            if (rightSamples)
              std::copy_n(p->right->input.data(), frames, p->right->wet.data());
            p->spring.Process(out, rightSamples ? p->right->wet.data() : nullptr, frames, utility);
          }
          else if (rightSamples && (IsDelay(p->effect) || IsReverb(p->effect)))
          {
            auto* rightOut = p->right->wet.data();
            if (IsDelay(p->effect))
            {
              std::copy_n(in, frames, out);
              std::copy_n(p->right->input.data(), frames, rightOut);
              p->echo.Process(out, frames, p->effect, utility, &p->right->echo, rightOut, draining);
            }
          }
          else
          {
            ProcessBuiltin(p, frames, dynamics, eq, utility, draining);
            if (rightSamples)
              ProcessBuiltin(p->right.get(), frames, dynamics, eq, utility, draining);
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
          mMeter.filterCutoff[1] = rightSamples ? static_cast<float>(p->right->filter.AppliedCutoff()) : mMeter.filterCutoff[0];
          mMeter.filterQ[1] = rightSamples ? static_cast<float>(p->right->filter.AppliedResonance()) : mMeter.filterQ[0];
        }
      }
      const double outDelta = (outTarget - mOut) / std::max(1, frames);
      const bool spectrum = mCapture && mMonitoring && ShowsSignalSpectrum(p->effect);
      for (int i = 0; i < frames; ++i)
      {
        mOut += outDelta;
        mMix = Approach(mMix, enabled ? 1. : 0., mStep);
        mIRMix = Approach(mIRMix, irMixTarget, mStep);
        const double mix = p->ir ? mMix * mIRMix : mMix;
        mSwitchGain = Approach(mSwitchGain, switching ? 0. : 1., mStep);
        if (rightSamples)
        {
          rightSamples[i] = (p->right->dry[i] * (1. - mix) + p->right->wet[i] * mOut * mix) * mSwitchGain;
        }
        samples[i] = (p->dry[i] * (1. - mix) + p->wet[i] * mOut * mix) * mSwitchGain;
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
          const double r=rightSamples ? p->right->dry[i]*(1.-mMix)+p->right->dynamicsMonitor[i]*mMix : l;
          mMeter.beforeMakeup=std::max(mMeter.beforeMakeup,static_cast<float>(std::min(1.e6,std::abs(.5*(l+r)*mSwitchGain))));
        }
      }
    }
    else
    {
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
  void ProcessBuiltin(Prepared* p, int frames, const DynamicsControls& dynamics, const EQControls& eq,
                      const UtilityControls& utility, bool draining)
  {
    double* in = p->input.data();
    double* out = p->wet.data();
    if (p->ir)
    {
      auto* convolved = p->ir->Process(&in, 1, frames);
      std::copy_n(convolved[0], frames, out);
      p->irWet.Process(out, frames, utility);
    }
    else
    {
      std::copy_n(in, frames, out);
      if (p->effect == SlotEffect::ToneEQ || p->effect == SlotEffect::CutEQ)
        p->eq.Process(out, frames, p->effect == SlotEffect::CutEQ, eq);
      else if (p->effect == SlotEffect::ParametricEQ)
        p->parametric.Process(out, frames, utility, eq);
      else if (p->effect == SlotEffect::Boost)
        p->boost.Process(out, frames, utility, eq);
      else if (p->effect == SlotEffect::TubeBoost)
        p->tube.Process(out, in, frames, utility, eq);
      else if (p->effect == SlotEffect::Distortion)
        p->distortion.Process(out, frames, utility);
      else if (p->effect == SlotEffect::PedalDrive)
        p->pedal.Process(out, frames, utility);
      else if (p->effect == SlotEffect::SynthFilter)
        p->filter.Process(out, frames, utility);
      else if (p->effect == SlotEffect::AutoSwell)
        p->swell.Process(out, frames, utility);
      else if (p->effect == SlotEffect::UniVibe)
        p->vibe.Process(out, frames, utility, p->rightChannel);
      else if (p->effect == SlotEffect::Bitcrusher || p->effect == SlotEffect::RingMod)
        p->texture.Process(out, frames, p->effect, utility, p->rightChannel);
      else if (IsDelay(p->effect))
        p->echo.Process(out, frames, p->effect, utility, nullptr, nullptr, draining);
      else if (IsModulation(p->effect))
        p->modulation.Process(out, frames, p->effect, utility, p->rightChannel);
      else if (p->effect == SlotEffect::Wah)
        p->wah.Process(out, frames, utility);
      else if (p->effect == SlotEffect::Tape)
        p->tape.Process(out, frames, utility);
      else if (p->effect != SlotEffect::Utility)
      {
        p->dynamics.Process(out, frames, p->effect, dynamics,p->dynamicsMonitor.empty() ? nullptr : p->dynamicsMonitor.data());
        if (mMonitoring) mMeter.reduction = std::max(mMeter.reduction, static_cast<float>(p->dynamics.PeakReduction() * mMix));
      }
    }
  }
  static double Approach(double current, double target, double step)
  { return current < target ? std::min(target, current + step) : std::max(target, current - step); }
  noise_print::Snapshot mNoiseSnapshot;
  RealtimeExchange<Prepared> mExchange;
  std::atomic<bool> mWaitForReset{false};
  double mMix = 0., mIRMix = 1., mIn = 1., mOut = 1., mSwitchGain = 1., mStep = 1. / 240.;
  bool mFirst = true;
  bool mMonitoring = false;
  SlotMeterData mMeter;
  SignalCapture* mCapture = nullptr; // Shared, owned by the plugin; set only on the selected slot.
};
