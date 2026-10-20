#pragma once
#include "NAMStage.h"
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
    bool enabled = true, calibrate = false;
    double inputDB = 0., trimDB = 0., calibrationDB = 0., slim = 1.;
    int outputMode = 0;
    DynamicsControls dynamics;
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
    const auto latency = LaneLatencies();
    int largestStageDelay = kMaxLaneDelay / 4;
    for (int slot = 4; slot < 12; ++slot)
      largestStageDelay = std::max(largestStageDelay, stages[slot].Latency());
    mStep = 1. / std::max(1., rate * .005);
    mDelay[0].Reset(std::max(0, latency[1] - latency[0]), mStep, 4 * largestStageDelay);
    mDelay[1].Reset(std::max(0, latency[0] - latency[1]), mStep, 4 * largestStageDelay);
    mFirstBlend = true;
  }

  void Process(double* samples, int frames, const std::array<Controls, kSlots>& controls, double blendB)
  {
    ProcessRow(samples, frames, controls, 0);
    std::copy_n(samples, frames, mLane[0].data());
    std::copy_n(samples, frames, mLane[1].data());
    ProcessRow(mLane[0].data(), frames, controls, 4);
    ProcessRow(mLane[1].data(), frames, controls, 8);
    const auto latency = LaneLatencies();
    mDelay[0].SetDelay(std::max(0, latency[1] - latency[0]));
    mDelay[1].SetDelay(std::max(0, latency[0] - latency[1]));
    blendB = std::clamp(blendB, 0., 1.);
    if (mFirstBlend)
    {
      mBlend = blendB;
      mFirstBlend = false;
    }
    for (int i = 0; i < frames; ++i)
    {
      mBlend = mBlend < blendB ? std::min(blendB, mBlend + mStep) : std::max(blendB, mBlend - mStep);
      // Linear blend: identical/dry lanes remain unity at every blend position.
      samples[i] = (1. - mBlend) * mDelay[0].Process(mLane[0][i]) + mBlend * mDelay[1].Process(mLane[1][i]);
    }
    ProcessRow(samples, frames, controls, 12);
  }

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
  void ProcessRow(double* samples, int frames, const std::array<Controls, kSlots>& controls, int first)
  {
    for (int i = first; i < first + 4; ++i)
    {
      const auto& c = controls[i];
      stages[i].Process(samples, frames, c.enabled, c.inputDB, c.trimDB, c.calibrate, c.calibrationDB, c.outputMode,
                        c.slim, c.dynamics);
    }
  }
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
  std::array<Delay, 2> mDelay;
  double mBlend = .5, mStep = 1. / 240.;
  bool mFirstBlend = true;
};
