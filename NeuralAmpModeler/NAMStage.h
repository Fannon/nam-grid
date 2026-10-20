#pragma once
#include <algorithm>
#include <cmath>
#include <vector>
#include "ResamplingNAM.h"
#include "RealtimeExchange.h"
#include "SlotDynamics.h"
#include "SlotUtility.h"
#include "SlotTimeEffects.h"
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
    SlotEQ eq;
    SlotParametricEQ parametric;
    SlotBoost boost;
    SlotEnvelopeFilter filter;
    SlotAutoSwell swell;
    SlotDelay echo;
    SlotReverb reverb;
    SlotModulation modulation;
    SlotTape tape;
    std::unique_ptr<dsp::ImpulseResponse> ir;
    std::vector<double> input, wet, dry, delay;
    size_t delayIndex = 0;
    int latency = 0;
    int64_t drainFrames = 0, drainLimit = 0;
    void Reset(double rate, int block)
    {
      dynamics.Reset(rate);
      eq.Reset(rate);
      parametric.Reset(rate);
      boost.Reset(rate);
      if (effect == SlotEffect::SynthFilter)
        filter.Reset(rate);
      if (effect == SlotEffect::AutoSwell)
        swell.Reset(rate);
      if (IsDelay(effect))
        echo.Reset(rate);
      if (IsReverb(effect))
        reverb.Reset(rate);
      if (IsModulation(effect))
        modulation.Reset(rate);
      if (effect == SlotEffect::Tape)
        tape.Reset(rate);
      if (effect == SlotEffect::NAM)
        eq.Process(nullptr, 0, false, {});
      if (model)
        model->Reset(rate, block);
      latency = model ? model->GetLatency() : 0;
      input.assign(block, 0.);
      wet.assign(block, 0.);
      dry.assign(block, 0.);
      delay.assign(std::max(1, latency), 0.);
      delayIndex = 0;
      drainLimit = static_cast<int64_t>(rate
                                        * (IsDelay(effect)        ? 110.
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
                  const EQControls& eq = {}, const UtilityControls& utility = {})
  {
    float inputPeak = 0.f;
    for (int i = 0; i < frames; ++i)
      inputPeak = std::max(inputPeak, static_cast<float>(std::min(1.e6, std::abs(samples[i]))));
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
        p->input[i] = samples[i] * mIn;
        p->dry[i] = p->latency ? p->delay[p->delayIndex] : samples[i];
        p->delay[p->delayIndex] = samples[i];
        p->delayIndex = (p->delayIndex + 1) % p->delay.size();
      }
      // Drain temporal histories with silence after bypass, then suspend them.
      // This prevents an old echo from resurfacing when a slot is re-enabled.
      const bool draining = !enabled && mMix <= 0. && p->drainFrames < p->drainLimit;
      if (enabled || mMix > 0. || draining)
      {
        if (draining)
        {
          std::fill_n(p->input.data(), frames, 0.);
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
        }
        else if (p->ir)
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
          else if (p->effect == SlotEffect::SynthFilter)
            p->filter.Process(out, frames, utility);
          else if (p->effect == SlotEffect::AutoSwell)
            p->swell.Process(out, frames, utility);
          else if (IsDelay(p->effect))
            p->echo.Process(out, frames, p->effect, utility);
          else if (IsReverb(p->effect))
            p->reverb.Process(out, frames, p->effect, utility);
          else if (IsModulation(p->effect))
            p->modulation.Process(out, frames, p->effect, utility);
          else if (p->effect == SlotEffect::Tape)
            p->tape.Process(out, frames, utility);
          else
          {
            p->dynamics.Process(out, frames, p->effect, dynamics);
            mMeter.reduction = std::max(mMeter.reduction, static_cast<float>(p->dynamics.PeakReduction() * mMix));
          }
        }
      }
      const double outDelta = (outTarget - mOut) / std::max(1, frames);
      for (int i = 0; i < frames; ++i)
      {
        mOut += outDelta;
        mMix = Approach(mMix, enabled ? 1. : 0., mStep);
        mSwitchGain = Approach(mSwitchGain, switching ? 0. : 1., mStep);
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
        mMeter.output = std::max(mMeter.output, static_cast<float>(std::min(1.e6, std::abs(samples[i]))));
      }
    }
    return samples;
  }

private:
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
  std::unique_ptr<dsp::ImpulseResponse> ir;
  void Prepare(int block)
  {
    if (!ir)
      return;
    std::vector<double> zeros(block, 0.);
    double* ptr = zeros.data();
    ir->Process(&ptr, 1, block); // prepare history/output buffers outside audio
  }
};
