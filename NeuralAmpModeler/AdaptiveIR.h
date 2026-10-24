#pragma once
#include "../AudioDSPTools/dsp/ImpulseResponse.h"
#include <memory>

// One mono FIR history. Stereo grid/legacy cabinets own independent instances.
// Construct/reconstruct on control while stopped, or before publishing a new stage.
//
// The resampled IR (at most 8192 taps) chooses the backend: up to 64 taps use the AudioDSPTools
// direct processor, primed offline; longer responses use FFTConvolver's uniform engine
// (third_party/fftconvolver), keeping every nonzero coefficient, including tails below 1e-6.
// The FFT partition is the largest power of two up to the host maximum block and 128, at least
// 32; a 127-frame maximum selects 64, and smaller maxima feed 32-sample partitions in partial
// calls. Output is delivered per call, so the partition adds no latency. These defaults balance
// FFT overhead against direct work and are not tuned per CPU (compare engines with
// docs/ir-convolution-comparison.md). Plans, spectra and buffers are allocated here, never in
// Process(), which accepts any frame count up to the prepared maximum.
class AdaptiveIR
{
public:
  using IRData = dsp::ImpulseResponse::IRData;
  enum class Backend
  {
    Direct,
    UniformFFT
  };
  AdaptiveIR(const IRData& data, double sampleRate, int maxBlock);
  ~AdaptiveIR();
  AdaptiveIR(const AdaptiveIR&) = delete;
  AdaptiveIR& operator=(const AdaptiveIR&) = delete;
  double** Process(double** input, size_t channels, size_t frames);
  IRData GetData() const;
  double GetSampleRate() const;
  Backend GetBackend() const;
  size_t Length() const;
  int PartitionSize() const;

private:
  struct Impl;
  std::unique_ptr<Impl> mImpl;
};
