#pragma once
#include "../AudioDSPTools/dsp/ImpulseResponse.h"
#include <memory>

// One mono FIR history. Stereo grid/legacy cabinets own independent instances.
// Construct/reconstruct on control while stopped, or before publishing a new stage.
//
// In Cabinet mode the resampled IR (at most 8192 taps) chooses the backend: up to 64 taps use the AudioDSPTools
// direct processor, primed offline; longer responses use FFTConvolver's uniform engine
// (third_party/fftconvolver), keeping every nonzero coefficient, including tails below 1e-6.
// The FFT partition is the largest power of two up to the host maximum block and 128, at least
// 32; a 127-frame maximum selects 64, and smaller maxima feed 32-sample partitions in partial
// calls. Output is delivered per call, so the partition adds no latency. These defaults balance
// FFT overhead against direct work and are not tuned per CPU (compare engines with
// docs/ir-convolution-comparison.md). Plans, spectra and buffers are allocated here, never in
// Process(), which accepts any frame count up to the prepared maximum.
//
// Long mode is opt-in and keeps up to 30 seconds. Above 8192 taps, the first
// 2048 samples use the same small partitions, with the remaining response in
// 2048-sample partitions. One tail input block produces the next output block,
// matching its 2048-sample kernel offset without buffering the dry or early wet.
class AdaptiveIR
{
public:
  using IRData = dsp::ImpulseResponse::IRData;
  enum class Backend
  {
    Direct,
    UniformFFT,
    TwoStageFFT
  };
  enum class Mode { Cabinet = 0, Long = 1 };
  static constexpr double maximumSeconds = 30.;
  AdaptiveIR(const IRData& data, double sampleRate, int maxBlock, Mode mode = Mode::Cabinet);
  ~AdaptiveIR();
  AdaptiveIR(const AdaptiveIR&) = delete;
  AdaptiveIR& operator=(const AdaptiveIR&) = delete;
  double** Process(double** input, size_t channels, size_t frames);
  IRData GetData() const;
  double GetSampleRate() const;
  Backend GetBackend() const;
  size_t Length() const;
  int PartitionSize() const;
  Mode GetMode() const;

private:
  struct Impl;
  std::unique_ptr<Impl> mImpl;
};
