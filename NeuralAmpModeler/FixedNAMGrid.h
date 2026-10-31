#pragma once
#include "NAMStage.h"
#include "GridPerformance.h"
#include <array>

// Four serial input slots -> two independent four-slot lanes -> four serial output slots.
// No graph, channel routing, or inference implementation of its own.
class FixedNAMGrid
{
public:
  static constexpr size_t kSlots = 16;
  static constexpr int kMaxLaneDelay = 65536;
  struct Controls
  {
    bool enabled = true;
    double inputDB = 0., trimDB = 0.;
    DynamicsControls dynamics;
    EQControls eq;
    UtilityControls utility;
  };

  std::array<NAMStage, kSlots> stages;

  static int TotalLatency(const std::array<int, kSlots>& latency)
  {
    int pre = 0, a = 0, b = 0, post = 0;
    for (int i = 0; i < 4; ++i)
    {
      pre += latency[i];
      a += latency[4 + i];
      b += latency[8 + i];
      post += latency[12 + i];
    }
    return pre + std::max(a, b) + post;
  }

  void ResetStopped(double rate, int maxBlock)
  {
    for (auto& stage : stages)
      stage.ResetStopped(rate, maxBlock);
    for (auto& buffer : mLane)
      buffer.assign(maxBlock, 0.);
    for (auto& buffer : mRightLane)
      buffer.assign(maxBlock, 0.);
    mSplitGain.assign(maxBlock, 1.);
    for (auto& tap : mTap)
      for (auto& buffer : tap) buffer.assign(maxBlock, 0.);
    mSplitLow = {}; mSplitMix = 0.; mSplitFrequency = 250.;
    mSplitCoefficientFrequency = -1.;
    mGainA = mGainB = 1.; mRouteFirst = true; mRate = rate;
    mSplitSmoothing = std::exp(-1. / (.01 * rate));
    for (auto& delay : mFineDelay) delay.Reset(rate);
    const auto latency = LaneLatencies();
    int largestStageDelay = kMaxLaneDelay / 4;
    for (int slot = 4; slot < 12; ++slot)
      largestStageDelay = std::max(largestStageDelay, stages[slot].Latency());
    mStep = 1. / std::max(1., rate * .005);
    mDelay[0].Reset(std::max(0, latency[1] - latency[0]), mStep, 4 * largestStageDelay);
    mDelay[1].Reset(std::max(0, latency[0] - latency[1]), mStep, 4 * largestStageDelay);
    for (int i = 0; i < 2; ++i)
      mRightDelay[i].Reset(std::max(0, latency[1 - i] - latency[i]), mStep, 4 * largestStageDelay);
    mFirstBlend = true;
  }

  void ClearStopped()
  {
    for (auto& stage : stages)
      stage.ClearStopped();
  }

  void Process(double* samples, int frames, const std::array<Controls, kSlots>& controls, double blendB,
               double* right = nullptr, const grid_performance::Routing& routing = {}, int inputPrefix = 0)
  {
    ProcessRow(samples, frames, controls, inputPrefix, right, 4);
    std::copy_n(samples, frames, mLane[0].data());
    std::copy_n(samples, frames, mLane[1].data());
    if (right)
      for (auto& lane : mRightLane)
        std::copy_n(right, frames, lane.data());
    if (mRouteFirst)
    {
      mSplitMix = routing.crossover ? 1. : 0.; mSplitFrequency = routing.frequency;
      mGainA = std::pow(10., routing.gainA * .05) * (!routing.muteA && (!routing.soloB || routing.soloA) ? 1. : 0.);
      mGainB = std::pow(10., routing.gainB * .05) * (!routing.muteB && (!routing.soloA || routing.soloB) ? 1. : 0.);
      mRouteFirst = false;
    }
    for (int i = 0; i < frames; ++i)
    {
      mSplitMix += std::clamp((routing.crossover ? 1. : 0.) - mSplitMix, -mStep, mStep);
      const double frequency = std::clamp(routing.frequency, 40., 5000.);
      mSplitFrequency = frequency + mSplitSmoothing * (mSplitFrequency - frequency);
      // Keep sample-rate smoothing/history alive, but reuse the identical
      // coefficient once the smoothed frequency stops changing.
      if (mSplitFrequency != mSplitCoefficientFrequency)
      {
        mSplitCoefficient = std::exp(-6.283185307179586 * std::min(.4 * mRate, mSplitFrequency) / mRate);
        mSplitCoefficientFrequency = mSplitFrequency;
      }
      const double coefficient = mSplitCoefficient;
      const double l = samples[i];
      mSplitLow[0] = grid_fx::Flush(l + coefficient * (mSplitLow[0] - l));
      mLane[0][i] = l + mSplitMix * (mSplitLow[0] - l);
      mLane[1][i] = l - mSplitMix * mSplitLow[0];
      if (right)
      {
        const double r = right[i];
        mSplitLow[1] = grid_fx::Flush(r + coefficient * (mSplitLow[1] - r));
        mRightLane[0][i] = r + mSplitMix * (mSplitLow[1] - r);
        mRightLane[1][i] = r - mSplitMix * mSplitLow[1];
      }
      // Equal unprocessed lanes reconstruct unity throughout a crossover fade.
      mSplitGain[i] = 2. / (2. - mSplitMix);
    }
    ProcessRow(mLane[0].data(), frames, controls, 4, right ? mRightLane[0].data() : nullptr);
    ProcessRow(mLane[1].data(), frames, controls, 8, right ? mRightLane[1].data() : nullptr);
    UtilityControls fineA, fineB;
    fineA.fineDelay = std::max(0., routing.timing); fineB.fineDelay = std::max(0., -routing.timing);
    mFineDelay[0].Process(mLane[0].data(), right ? mRightLane[0].data() : nullptr, frames, fineA);
    mFineDelay[1].Process(mLane[1].data(), right ? mRightLane[1].data() : nullptr, frames, fineB);
    const auto latency = LaneLatencies();
    mDelay[0].SetDelay(std::max(0, latency[1] - latency[0]));
    mDelay[1].SetDelay(std::max(0, latency[0] - latency[1]));
    for (int i = 0; i < 2; ++i)
      mRightDelay[i].SetDelay(std::max(0, latency[1 - i] - latency[i]));
    blendB = std::clamp(blendB, 0., 1.);
    if (mFirstBlend)
    {
      mBlend = blendB;
      mFirstBlend = false;
    }
    const double targetA = std::pow(10., routing.gainA * .05) * (!routing.muteA && (!routing.soloB || routing.soloA) ? 1. : 0.);
    const double targetB = std::pow(10., routing.gainB * .05) * (!routing.muteB && (!routing.soloA || routing.soloB) ? 1. : 0.);
    for (int i = 0; i < frames; ++i)
    {
      mBlend = mBlend < blendB ? std::min(blendB, mBlend + mStep) : std::max(blendB, mBlend - mStep);
      mGainA += std::clamp(targetA - mGainA, -mStep * 4., mStep * 4.);
      mGainB += std::clamp(targetB - mGainB, -mStep * 4., mStep * 4.);
      // Linear blend: identical/dry lanes remain unity at every blend position.
      if (right)
        right[i] =
          mSplitGain[i] * ((1. - mBlend) * mGainA * mRightDelay[0].Process(mRightLane[0][i]) + mBlend * mGainB * mRightDelay[1].Process(mRightLane[1][i]));
      samples[i] = mSplitGain[i] * ((1. - mBlend) * mGainA * mDelay[0].Process(mLane[0][i]) + mBlend * mGainB * mDelay[1].Process(mLane[1][i]));
    }
    ProcessRow(samples, frames, controls, 12, right);
  }

  // Run only the independent input prefix, before Process skips those stages.
  // Keep its first short quantum for existing gain ramps; batch the remainder.
  // Prefix taps cover the full callback, unlike the suffix's per-quantum taps.
  void ProcessInputPrefix(double* samples, int frames, const std::array<Controls, kSlots>& controls,
                          int slots, int firstQuantum, double* right = nullptr)
  {
    const int first = std::min(firstQuantum, frames);
    ProcessRow(samples, first, controls, 0, right, slots);
    if (first < frames)
      ProcessRow(samples + first, frames - first, controls, 0, right ? right + first : nullptr, slots, first);
  }

  // One on-demand measurement tap, independent of slot contents and enable state.
  // Pointers belong to the plugin and are attached only at audio block boundaries.
  void AttachMonitor(int slot, SignalCapture* capture, signal_monitor::Accumulator* meter)
  { mMonitorSlot = slot; mMonitorCapture = capture; mMonitorMeter = meter; }

  // Copy each follower's chosen slot output during Process (-1: grid input).
  void SetTap(int slot, int instance = 0) { mTapSlot[instance] = slot; }
  const double* TapLeft(int instance = 0) const { return mTap[instance][0].data(); }
  const double* TapRight(int instance = 0) const { return mTapStereo[instance] ? mTap[instance][1].data() : nullptr; }

private:
  struct Delay
  {
    std::vector<double> history;
    size_t index = 0;
    int previous = 0, target = 0;
    double mix = 1., step = 1.;
    void Reset(int samples, double rampStep, int capacity)
    {
      // Fixed capacity also makes VST3's dynamic latency handoff allocation-free on audio.
      history.assign(capacity + 1, 0.);
      previous = target = samples;
      mix = 1.;
      index = 0;
      step = rampStep;
    }
    void SetDelay(int samples)
    {
      if (samples != target)
      {
        previous = target;
        target = samples;
        mix = 0.;
      }
    }
    double Process(double input)
    {
      history[index] = input;
      const auto read = [&](int delay) {
        return delay == 0
                 ? input
                 : history[index >= static_cast<size_t>(delay) ? index - delay : history.size() - (delay - index)];
      };
      double output;
      if (mix >= 1.)
        output = read(target);
      else
      {
        mix = std::min(1., mix + step);
        output = (1. - mix) * read(previous) + mix * read(target);
      }
      if (++index == history.size())
        index = 0;
      return output;
    }
  };
  void ProcessRow(double* samples, int frames, const std::array<Controls, kSlots>& controls, int first, double* right,
                  int end = -1, int tapOffset = 0)
  {
    if (end < 0) end = (first / 4 + 1) * 4;
    for (int i = first; i < end; ++i)
    {
      const auto& c = controls[i];
      stages[i].Process(samples, frames, c.enabled, c.inputDB, c.trimDB, c.dynamics, c.eq, c.utility, right);
      if (i == mMonitorSlot && mMonitorCapture && mMonitorMeter)
        for (int frame = 0; frame < frames; ++frame)
        {
          const double l = samples[frame], r = right ? right[frame] : l;
          mMonitorCapture->Push(l, r); mMonitorMeter->Push(l, r);
        }
      for (int instance = 0; instance < grid_performance::sourceInstances; ++instance)
        if (i == mTapSlot[instance])
        {
          std::copy_n(samples, frames, mTap[instance][0].data() + tapOffset);
          if (right) std::copy_n(right, frames, mTap[instance][1].data() + tapOffset);
          mTapStereo[instance] = right != nullptr;
        }
    }
  }
  int mMonitorSlot = -1;
  SignalCapture* mMonitorCapture = nullptr;
  signal_monitor::Accumulator* mMonitorMeter = nullptr;
  std::array<int, 2> LaneLatencies() const
  {
    std::array<int, 2> latency{};
    for (int i = 0; i < 4; ++i)
    {
      latency[0] += stages[4 + i].Latency();
      latency[1] += stages[8 + i].Latency();
    }
    return latency;
  }
  std::array<std::vector<double>, 2> mLane;
  std::array<Delay, 2> mDelay, mRightDelay;
  std::array<std::vector<double>, 2> mRightLane;
  double mBlend = .5, mStep = 1. / 240.;
  bool mFirstBlend = true;
  std::array<StereoUtility, 2> mFineDelay;
  std::vector<double> mSplitGain;
  std::array<double, 2> mSplitLow{};
  double mSplitMix = 0., mSplitFrequency = 250., mGainA = 1., mGainB = 1., mRate = 48000.;
  double mSplitSmoothing = 0.;
  double mSplitCoefficientFrequency = -1., mSplitCoefficient = 0.;
  bool mRouteFirst = true;
  std::array<std::array<std::vector<double>, 2>, grid_performance::sourceInstances> mTap;
  std::array<int, grid_performance::sourceInstances> mTapSlot{-1, -1};
  std::array<bool, grid_performance::sourceInstances> mTapStereo{};
};
