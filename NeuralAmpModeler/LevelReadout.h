#pragma once
#include <algorithm>
#include <cmath>

struct LevelReadout
{
  double average = 0., peak = 0., hold = 0.;
  void Receive(double rms, double incomingPeak)
  {
    average = std::isfinite(rms) ? std::max(0., rms) : 0.;
    if (std::isfinite(incomingPeak) && incomingPeak > peak)
    { peak = incomingPeak; hold = 1.5; }
  }
  void Tick(double seconds, bool stale)
  {
    if (stale) average *= std::exp(-seconds / .35);
    const double release = std::max(0., seconds - hold);
    hold = std::max(0., hold - seconds);
    if (release > 0.) peak = std::max(average, peak * std::pow(10., -12. * release / 20.));
    if (average < 1.e-8) average = 0.;
    if (peak < 1.e-8) peak = 0.;
  }
};

// A module's input against its output over windows of about 0.4 s: RMS level, peak and
// crest factor (peak to RMS, in dB). Gain is the RMS change. A lower crest at the
// output means the module compresses or saturates; a higher one means it expands
// or emphasizes transients. Mono sums, like the slot meters.
struct LevelComparison
{
  struct Side
  {
    double rmsDB = -120., peakDB = -120., crestDB = 0.;
  };
  Side input, output;
  bool valid = false;
  // Energies are sums of squares over `frames`; peaks are linear.
  void Receive(double inputEnergy, double outputEnergy, double inputPeak, double outputPeak, int frames, double rate)
  {
    if (frames <= 0 || !std::isfinite(inputEnergy) || !std::isfinite(outputEnergy))
    {
      // A bypassed or stopped module sends empty packets: drop the reading after a while.
      if (++mIdle > 30) valid = false;
      return;
    }
    mIdle = 0;
    mInput += inputEnergy; mOutput += outputEnergy; mFrames += frames;
    mInputPeak = std::max(mInputPeak, inputPeak); mOutputPeak = std::max(mOutputPeak, outputPeak);
    if (mFrames < .4 * (rate > 0. ? rate : 48000.)) return;
    const auto side = [&](double energy, double peak) {
      Side result;
      result.rmsDB = 10. * std::log10(std::max(energy / mFrames, 1.e-12));
      result.peakDB = 20. * std::log10(std::max(peak, 1.e-6));
      result.crestDB = std::max(0., result.peakDB - result.rmsDB);
      return result;
    };
    input = side(mInput, mInputPeak);
    output = side(mOutput, mOutputPeak);
    // Below -70 dBFS RMS the input is pick noise or silence; ratios would mislead.
    valid = input.rmsDB > -70. && output.rmsDB > -100.;
    mInput = mOutput = mInputPeak = mOutputPeak = 0.; mFrames = 0;
  }
  // No signal arrived for a while: forget the last reading.
  void Clear() { *this = {}; }
  double GainDB() const { return output.rmsDB - input.rmsDB; }
  double CrestChangeDB() const { return output.crestDB - input.crestDB; }

private:
  double mInput = 0., mOutput = 0., mInputPeak = 0., mOutputPeak = 0.;
  int mFrames = 0, mIdle = 0;
};
