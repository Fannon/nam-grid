#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>

// First-order 5 Hz DC blocker matching recursive_linear_filter::HighPass(rate, 5.)
// bit-identically in-place without per-sample modulo indexing or buffer resizing.
class GridDCBlocker
{
public:
  static constexpr size_t kChannels = 2;

  void Prepare(double rate, size_t /*channels*/, size_t /*frames*/)
  {
    mAlpha = AlphaForRate(rate);
    mState = {};
  }

  double** Process(double** inputs, size_t numChannels, size_t numFrames)
  {
    const double alpha = mAlpha;
    const double negAlpha = -alpha;
    const size_t channels = std::min(numChannels, mState.size());
    for (size_t c = 0; c < channels; ++c)
    {
      double prevIn = mState[c].prevIn;
      double prevOut = mState[c].prevOut;
      double* channel = inputs[c];
      for (size_t s = 0; s < numFrames; ++s)
      {
        const double x = channel[s];
        double out = 0.0;
        out += alpha * x;
        out += negAlpha * prevIn;
        out += alpha * prevOut;
        if (std::isnan(out))
          out = 0.0;
        prevIn = x;
        prevOut = out;
        channel[s] = out;
      }
      mState[c].prevIn = prevIn;
      mState[c].prevOut = prevOut;
    }
    return inputs;
  }

private:
  struct ChannelState
  {
    double prevIn = 0., prevOut = 0.;
  };
  static constexpr double AlphaForRate(double rate)
  {
    const double c = 2.0 * 3.14159265358979323846 * 5.0 / rate;
    return 1.0 / (c + 1.0);
  }
  double mAlpha = AlphaForRate(48000.0);
  std::array<ChannelState, kChannels> mState{};
};
