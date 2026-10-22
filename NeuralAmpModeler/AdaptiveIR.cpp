#include "AdaptiveIR.h"
#include "../AudioDSPTools/dsp/Resample.h"
#include "third_party/fftconvolver/FFTConvolver.h"
#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <stdexcept>

struct AdaptiveIR::Impl
{
  IRData data;
  double rate;
  int maxBlock, partition = 0;
  size_t length = 0;
  Backend backend = Backend::Direct;
  std::unique_ptr<dsp::ImpulseResponse> direct;
  fftconvolver::FFTConvolver fft;
  std::vector<float> input, output;
  std::array<std::vector<double>, 2> results;
  std::array<double*, 2> pointers;

  Impl(const IRData& source, double sampleRate, int block)
  : data(source)
  , rate(sampleRate)
  , maxBlock(block)
  {
    if (maxBlock <= 0 || !std::isfinite(rate) || rate <= 0. || data.mRawAudio.empty()
        || !std::isfinite(data.mRawAudioSampleRate) || data.mRawAudioSampleRate <= 0.
        || !std::all_of(data.mRawAudio.begin(), data.mRawAudio.end(), [](float x) { return std::isfinite(x); }))
      throw std::invalid_argument("Invalid IR preparation data");
    for (size_t channel = 0; channel < results.size(); ++channel)
    {
      results[channel].assign(maxBlock, 0.);
      pointers[channel] = results[channel].data();
    }
    // Match AudioDSPTools' interpolation, float rounding, cap and gain exactly.
    std::vector<float> weights;
    if (data.mRawAudioSampleRate == rate)
      weights = data.mRawAudio;
    else
    {
      std::vector<float> padded(data.mRawAudio.size() + 2, 0.f);
      std::copy(data.mRawAudio.begin(), data.mRawAudio.end(), padded.begin() + 1);
      dsp::ResampleCubic<float>(padded, data.mRawAudioSampleRate, rate, 0., weights);
    }
    length = std::min<size_t>(weights.size(), 8192);
    if (!length)
      throw std::invalid_argument("Empty resampled IR");
    weights.resize(length);
    if (length <= 64)
    {
      direct = std::make_unique<dsp::ImpulseResponse>(data, rate);
      double* silence = pointers[0];
      direct->Process(&silence, 1, maxBlock); // Allocate all history/outputs before audio.
      return;
    }
    backend = Backend::UniformFFT;
    partition = 32;
    while (partition * 2 <= std::min(maxBlock, 128))
      partition *= 2;
    const float gain = std::pow(10., -18 * .05) * 48000 / rate;
    for (auto& weight : weights)
      weight *= gain;
    input.assign(maxBlock, 0.f);
    output.assign(maxBlock, 0.f);
    if (!fft.init(partition, weights.data(), length))
      throw std::runtime_error("IR FFT preparation failed");
  }
};

AdaptiveIR::AdaptiveIR(const IRData& data, double sampleRate, int maxBlock)
: mImpl(std::make_unique<Impl>(data, sampleRate, maxBlock))
{
}
AdaptiveIR::~AdaptiveIR() = default;
AdaptiveIR::IRData AdaptiveIR::GetData() const
{ return mImpl->data; }
double AdaptiveIR::GetSampleRate() const
{ return mImpl->rate; }
AdaptiveIR::Backend AdaptiveIR::GetBackend() const
{ return mImpl->backend; }
size_t AdaptiveIR::Length() const
{ return mImpl->length; }
int AdaptiveIR::PartitionSize() const
{ return mImpl->partition; }
double** AdaptiveIR::Process(double** input, size_t channels, size_t frames)
{
  auto& state = *mImpl;
  // Public host boundaries reject oversized buffers before any processor runs.
  assert(frames <= static_cast<size_t>(state.maxBlock) && channels <= state.pointers.size());
  if (!frames || !channels || frames > static_cast<size_t>(state.maxBlock) || channels > state.pointers.size())
    return state.pointers.data();
  if (state.backend == Backend::Direct)
  {
    auto* mono = input[0];
    std::copy_n(state.direct->Process(&mono, 1, frames)[0], frames, state.pointers[0]);
  }
  else
  {
    for (size_t i = 0; i < frames; ++i)
      state.input[i] = static_cast<float>(input[0][i]);
    state.fft.process(state.input.data(), state.output.data(), frames);
    std::copy_n(state.output.data(), frames, state.pointers[0]);
  }
  // Preserve the old processor's mono-kernel API. Stereo uses separate objects.
  for (size_t channel = 1; channel < channels; ++channel)
    std::copy_n(state.pointers[0], frames, state.pointers[channel]);
  return state.pointers.data();
}
