#pragma once
#include <algorithm>
#include <cmath>
#include <vector>
#include "ResamplingNAM.h"
#include "RealtimeExchange.h"
#include "SlotDynamics.h"
#include "../AudioDSPTools/dsp/ImpulseResponse.h"

struct NAMModelInfo
{
  bool loaded = false, hasInput = false, hasOutput = false, hasLoudness = false, slimmable = false;
  double rate = 48000., input = 0., output = 0., loudness = 0.;
};

class NAMStage
{
public:
  struct Prepared
  {
    std::unique_ptr<ResamplingNAM> model;
    SlotEffect effect = SlotEffect::NAM;
    SlotDynamics dynamics;
    std::vector<double> input, wet, dry, delay;
    size_t delayIndex = 0;
    int latency = 0;
    void Reset(double rate, int block)
    {
      dynamics.Reset(rate);
      if (model)
        model->Reset(rate, block);
      latency = model ? model->GetLatency() : 0;
      input.assign(block, 0.);
      wet.assign(block, 0.);
      dry.assign(block, 0.);
      delay.assign(std::max(1, latency), 0.);
      delayIndex = 0;
    }
    NAMModelInfo Info() const
    {
      NAMModelInfo info;
      info.loaded = effect != SlotEffect::NAM;
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
  }
  int Latency() const
  {
    auto* p = mExchange.Active();
    return p ? p->latency : 0;
  }
  // Called exclusively by audio, or while host processing is stopped.
  double* Process(double* samples, int frames, bool enabled, double inputDB, double trimDB, bool calibrate,
                  double calibrationDB, int outputMode, double slim, const DynamicsControls& dynamics = {})
  {
    const bool switching = mExchange.HasPending() && !mWaitForReset.load();
    if (switching && mSwitchGain <= 0.)
      mExchange.Adopt();
    auto* p = mExchange.Active();
    const auto info = p ? p->Info() : NAMModelInfo{};
    if (p && p->effect != SlotEffect::NAM)
      inputDB = trimDB = 0.; // Built-ins expose only their three dedicated controls.
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
      // Skip inference after the bypass ramp finishes. The dry path preserves latency.
      if (enabled || mMix > 0.)
      {
        double* in = p->input.data();
        double* out = p->wet.data();
        if (p->model)
        {
          if (auto* slimmable = p->model->GetSlimmableModel())
            slimmable->SetSlimmableSize(slim);
          p->model->process(&in, &out, frames);
        }
        else
        {
          std::copy_n(in, frames, out);
          p->dynamics.Process(out, frames, p->effect, dynamics);
        }
      }
      const double outDelta = (outTarget - mOut) / std::max(1, frames);
      for (int i = 0; i < frames; ++i)
      {
        mOut += outDelta;
        mMix = Approach(mMix, enabled ? 1. : 0., mStep);
        mSwitchGain = Approach(mSwitchGain, switching ? 0. : 1., mStep);
        samples[i] = (p->dry[i] * (1. - mMix) + p->wet[i] * mOut * mMix) * mSwitchGain;
      }
    }
    else
    {
      if (!switching && mSwitchGain >= 1.)
        return samples; // Empty grid slots need no per-sample work once switching has settled.
      for (int i = 0; i < frames; ++i)
      {
        mSwitchGain = Approach(mSwitchGain, switching ? 0. : 1., mStep);
        samples[i] *= mSwitchGain;
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
