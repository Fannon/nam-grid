#pragma once
#include <algorithm>
#include <cmath>
#include <vector>
#include "ResamplingNAM.h"
#include "RealtimeExchange.h"
#include "SlotDynamics.h"
#include "SlotUtility.h"
#include "SlotTimeEffects.h"
#include "StereoUtility.h"
#include "SlotGuitarEffects.h"
#include "../AudioDSPTools/dsp/ImpulseResponse.h"

struct NAMModelInfo
{
  bool loaded = false, hasInput = false, hasOutput = false, hasLoudness = false, slimmable = false;
  double rate = 48000., input = 0., output = 0., loudness = 0.;
};

struct SlotMeterData
{ float input = 0.f, output = 0.f, reduction = 0.f; };

class NAMStage
{
public:
  struct Prepared
  {
    std::unique_ptr<ResamplingNAM> model;
    SlotEffect effect = SlotEffect::NAM;
    SlotDynamics dynamics;
    SlotEQ eq, namCuts;
    SlotParametricEQ parametric;
    SlotBoost boost;
    SlotTubeBoost tube;
    SlotEnvelopeFilter filter;
    SlotAutoSwell swell;
    SlotDelay echo;
    SlotReverb reverb;
    SlotModulation modulation;
    SlotTape tape;
    SlotSpring spring;
    SlotWah wah;
    StereoUtility utilityStereo;
    std::unique_ptr<Prepared> right;
    bool rightChannel = false;
    std::unique_ptr<dsp::ImpulseResponse> ir;
    std::vector<double> input, wet, dry, delay;
    size_t delayIndex = 0;
    int latency = 0;
    int64_t drainFrames = 0, drainLimit = 0;
    void Reset(double rate, int block)
    {
      utilityStereo.Reset(rate);
      dynamics.Reset(rate);
      eq.Reset(rate);
      parametric.Reset(rate);
      boost.Reset(rate);
      if (effect == SlotEffect::TubeBoost)
        tube.Reset(rate);
      if (effect == SlotEffect::SynthFilter)
        filter.Reset(rate);
      if (effect == SlotEffect::AutoSwell)
        swell.Reset(rate);
      if (IsDelay(effect))
        echo.Reset(rate);
      if (IsReverb(effect) && effect != SlotEffect::SpringReverb)
        reverb.Reset(rate);
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
      latency = model ? model->GetLatency() : 0;
      input.assign(block, 0.);
      wet.assign(block, 0.);
      dry.assign(block, 0.);
      delay.assign(std::max(1, latency), 0.);
      delayIndex = 0;
      drainLimit = static_cast<int64_t>(rate
                                        * (IsDelay(effect)        ? 9.
                                           : IsReverb(effect)     ? 30.
                                           : IsModulation(effect) ? .07
                                                                  : 0.));
      drainFrames = drainLimit; // Fresh histories already contain silence.
      if (ir)
      {
        ir = std::make_unique<dsp::ImpulseResponse>(ir->GetData(), rate);
        double* silence = input.data();
        ir->Process(&silence, 1, block);
      }
      if (!rightChannel)
      {
        right = std::make_unique<Prepared>();
        right->rightChannel = true;
        right->effect = effect;
        if (ir)
          right->ir = std::make_unique<dsp::ImpulseResponse>(ir->GetData(), rate);
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

  void Publish(std::unique_ptr<Prepared> item, bool waitForReset = false)
  {
    if (waitForReset)
      mWaitForReset.store(true);
    mExchange.Publish(std::move(item));
  }
  void Collect() { mExchange.Collect(); }
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
  }
  SlotMeterData TakeMeter()
  {
    const auto result = mMeter;
    mMeter = {};
    return result;
  } // audio only
  int Latency() const
  {
    auto* p = mExchange.Active();
    return p ? p->latency : 0;
  }
  // Called exclusively by audio, or while host processing is stopped.
  double* Process(double* samples, int frames, bool enabled, double inputDB, double trimDB, bool calibrate,
                  double calibrationDB, int outputMode, double slim, const DynamicsControls& dynamics = {},
                  const EQControls& eq = {}, const UtilityControls& utility = {}, double* rightSamples = nullptr)
  {
    float inputPeak = 0.f;
    for (int i = 0; i < frames; ++i)
      inputPeak =
        std::max(inputPeak, static_cast<float>(std::min(
                              1.e6, std::max(std::abs(samples[i]), rightSamples ? std::abs(rightSamples[i]) : 0.))));
    mMeter.input = std::max(mMeter.input, inputPeak);
    const bool switching = mExchange.HasPending() && !mWaitForReset.load();
    if (switching && mSwitchGain <= 0.)
      mExchange.Adopt();
    auto* p = mExchange.Active();
    const auto info = p ? p->Info() : NAMModelInfo{};
    if (p && p->effect != SlotEffect::NAM)
      inputDB = trimDB = 0.; // Built-ins use their dedicated controls.
    if (p && p->effect == SlotEffect::CabinetIR)
      trimDB = eq.irLevel + 18.; // Undo the reused convolver's fixed -18 dB; knob is actual IR gain.
    if (calibrate && info.hasInput)
      inputDB += calibrationDB - info.input;
    if (outputMode == 1 && info.hasLoudness)
      trimDB += -18. - info.loudness;
    if (outputMode == 2 && info.hasOutput)
      trimDB += info.output - calibrationDB;
    const double inTarget = std::pow(10., inputDB * .05), outTarget = std::pow(10., trimDB * .05);
    if (mFirst)
    {
      mIn = inTarget;
      mOut = outTarget;
      mMix = enabled && info.loaded ? 1. : 0.;
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
          r.delayIndex = (r.delayIndex + 1) % r.delay.size();
        }
        p->dry[i] = p->latency ? p->delay[p->delayIndex] : samples[i];
        p->delay[p->delayIndex] = samples[i];
        p->delayIndex = (p->delayIndex + 1) % p->delay.size();
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
          if (p->effect == SlotEffect::SpringReverb)
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
            else
              p->reverb.ProcessStereo(
                out, rightOut, in, p->right->input.data(), frames, p->effect, utility, p->right->reverb);
          }
          else
          {
            ProcessBuiltin(p, frames, dynamics, eq, utility, draining);
            if (rightSamples)
              ProcessBuiltin(p->right.get(), frames, dynamics, eq, utility, draining);
          }
          if (p->effect == SlotEffect::Utility)
            p->utilityStereo.Process(out, rightSamples ? p->right->wet.data() : nullptr, frames, utility);
        }
        if (p->model && rightSamples)
          std::copy_n(out, frames, p->right->wet.data());
      }
      const double outDelta = (outTarget - mOut) / std::max(1, frames);
      for (int i = 0; i < frames; ++i)
      {
        mOut += outDelta;
        mMix = Approach(mMix, enabled ? 1. : 0., mStep);
        mSwitchGain = Approach(mSwitchGain, switching ? 0. : 1., mStep);
        if (rightSamples)
        {
          rightSamples[i] = (p->right->dry[i] * (1. - mMix) + p->right->wet[i] * mOut * mMix) * mSwitchGain;
          mMeter.output = std::max(mMeter.output, static_cast<float>(std::min(1.e6, std::abs(rightSamples[i]))));
        }
        samples[i] = (p->dry[i] * (1. - mMix) + p->wet[i] * mOut * mMix) * mSwitchGain;
        mMeter.output = std::max(mMeter.output, static_cast<float>(std::min(1.e6, std::abs(samples[i]))));
      }
    }
    else
    {
      if (!switching && mSwitchGain >= 1.)
      {
        mMeter.output = std::max(mMeter.output, inputPeak);
        return samples; // Empty grid slots need no per-sample work once switching has settled.
      }
      for (int i = 0; i < frames; ++i)
      {
        mSwitchGain = Approach(mSwitchGain, switching ? 0. : 1., mStep);
        samples[i] *= mSwitchGain;
        if (rightSamples)
          rightSamples[i] *= mSwitchGain;
        mMeter.output = std::max(mMeter.output, static_cast<float>(std::min(1.e6, std::abs(samples[i]))));
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
      else if (p->effect == SlotEffect::SynthFilter)
        p->filter.Process(out, frames, utility);
      else if (p->effect == SlotEffect::AutoSwell)
        p->swell.Process(out, frames, utility);
      else if (IsDelay(p->effect))
        p->echo.Process(out, frames, p->effect, utility, nullptr, nullptr, draining);
      else if (IsReverb(p->effect))
        p->reverb.Process(out, frames, p->effect, utility);
      else if (IsModulation(p->effect))
        p->modulation.Process(out, frames, p->effect, utility, p->rightChannel);
      else if (p->effect == SlotEffect::Wah)
        p->wah.Process(out, frames, utility);
      else if (p->effect == SlotEffect::Tape)
        p->tape.Process(out, frames, utility);
      else if (p->effect != SlotEffect::Utility)
      {
        p->dynamics.Process(out, frames, p->effect, dynamics);
        mMeter.reduction = std::max(mMeter.reduction, static_cast<float>(p->dynamics.PeakReduction() * mMix));
      }
    }
  }
  static double Approach(double current, double target, double step)
  { return current < target ? std::min(target, current + step) : std::max(target, current - step); }
  RealtimeExchange<Prepared> mExchange;
  std::atomic<bool> mWaitForReset{false};
  double mMix = 0., mIn = 1., mOut = 1., mSwitchGain = 1., mStep = 1. / 240.;
  bool mFirst = true;
  SlotMeterData mMeter;
};

struct PreparedIR
{
  std::unique_ptr<dsp::ImpulseResponse> ir, right;
  void Prepare(int block)
  {
    if (!ir)
      return;
    std::vector<double> zeros(block, 0.);
    double* ptr = zeros.data();
    ir->Process(&ptr, 1, block); // prepare history/output buffers outside audio
    right = std::make_unique<dsp::ImpulseResponse>(ir->GetData(), ir->GetSampleRate());
    right->Process(&ptr, 1, block);
  }
};
