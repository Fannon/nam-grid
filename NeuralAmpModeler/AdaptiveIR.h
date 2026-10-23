#pragma once
#include "../AudioDSPTools/dsp/ImpulseResponse.h"
#include <memory>

// One mono FIR history. Stereo grid/legacy cabinets own independent instances.
// Construct/reconstruct on control while stopped, or before publishing a new stage.
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
