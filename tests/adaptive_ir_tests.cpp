#include "AdaptiveIR.h"
#include "third_party/fftconvolver/FFTConvolver.h"
#include "../AudioDSPTools/dsp/Resample.h"
#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <new>
#include <random>

static thread_local bool tracking = false;
static thread_local size_t allocations = 0, deletions = 0;
void* operator new(size_t size)
{
  if (tracking)
    ++allocations;
  if (void* result = std::malloc(size ? size : 1))
    return result;
  throw std::bad_alloc();
}
void* operator new[](size_t size)
{ return ::operator new(size); }
void operator delete(void* ptr) noexcept
{
  if (tracking && ptr)
    ++deletions;
  std::free(ptr);
}
void operator delete[](void* ptr) noexcept
{ ::operator delete(ptr); }
void operator delete(void* ptr, size_t) noexcept
{ ::operator delete(ptr); }
void operator delete[](void* ptr, size_t) noexcept
{ ::operator delete(ptr); }

int main()
{
  // The long tail can accumulate older partitions while collecting its next
  // input block. Compare with ordinary full-block convolution, including resets
  // after incomplete preparation. Independently allocated FFT plans can differ
  // by float rounding even when both use ordinary, unscheduled processing.
  {
    constexpr size_t block = 2048;
    std::vector<float> weights(17003), input(block), expected(block), actual(block);
    std::mt19937 random(9123);
    for (auto& value : weights) value = .01f * (static_cast<float>(random()) / static_cast<float>(random.max()) - .5f);
    fftconvolver::FFTConvolver reference, scheduled;
    for (int reset = 0; reset < 2; ++reset)
    {
      assert(reference.init(block, weights.data(), weights.size()));
      assert(scheduled.init(block, weights.data(), weights.size()));
      for (int iteration = 0; iteration < 24; ++iteration)
      {
        for (auto& value : input) value = static_cast<float>(random()) / static_cast<float>(random.max()) - .5f;
        allocations = deletions = 0; tracking = true;
        scheduled.prepareForBlock(0);
        for (size_t position = 0, part = 0; position < block; ++part)
        {
          position += std::min(block - position, std::array<size_t, 7>{1, 3, 31, 37, 127, 511, 2048}[part % 7]);
          scheduled.prepareForBlock(position);
        }
        reference.process(input.data(), expected.data(), block);
        scheduled.process(input.data(), actual.data(), block);
        tracking = false;
        assert(allocations == 0 && deletions == 0);
        for (size_t i = 0; i < block; ++i)
          assert(std::isfinite(actual[i]) && std::abs(expected[i] - actual[i]) < 1.e-6f);
      }
      scheduled.prepareForBlock(block / 2);
    }
  }
  double maximumError = 0.;
  size_t samples = 0;
  for (double rate : {44100., 48000., 96000., 192000.})
    for (int length : {1, 64, 65, 256, 2048, 16384})
      for (int maxBlock : {1, 127, 512})
      {
        AdaptiveIR::IRData data;
        data.mRawAudioSampleRate = 48000.;
        std::mt19937 random(123);
        data.mRawAudio.resize(length);
        for (auto& coefficient : data.mRawAudio)
          coefficient = static_cast<float>(.04 * (2. * static_cast<double>(random()) / random.max() - 1.));
        data.mRawAudio[0] = .8f;
        AdaptiveIR candidate(data, rate, maxBlock);
        dsp::ImpulseResponse reference(data, rate);
        assert(candidate.GetData().mRawAudio == data.mRawAudio);
        assert(candidate.GetData().mRawAudioSampleRate == data.mRawAudioSampleRate);
        assert(candidate.GetSampleRate() == rate && candidate.Length() > 0 && candidate.Length() <= 8192);
        assert((candidate.GetBackend() == AdaptiveIR::Backend::Direct) == (candidate.Length() <= 64));
        if (candidate.GetBackend() == AdaptiveIR::Backend::UniformFFT)
          assert(candidate.PartitionSize() >= 32 && candidate.PartitionSize() <= 128);
        std::vector<double> input(maxBlock, 0.);
        double* pointer = input.data();
        reference.Process(&pointer, 1, maxBlock); // Reference's lazy buffers, outside tracked processing.
        const int duration = 512 + static_cast<int>(candidate.Length()) + 256;
        const std::array<int, 7> blocks{1, 3, 31, 64, 127, 16, 512};
        for (int position = 0, block = 0; position < duration; ++block)
        {
          const int count = std::min({maxBlock, duration - position, blocks[block % blocks.size()]});
          for (int i = 0; i < count; ++i)
            input[i] = position + i < 512 ? .15 * std::sin(.079 * (position + i))
                                              + .04 * (2. * static_cast<double>(random()) / random.max() - 1.)
                                          : 0.;
          auto* expected = reference.Process(&pointer, 1, count)[0];
          allocations = deletions = 0;
          tracking = true;
          Eigen::internal::set_is_malloc_allowed(false);
          auto** actual = candidate.Process(&pointer, 2, count);
          Eigen::internal::set_is_malloc_allowed(true);
          tracking = false;
          assert(allocations == 0 && deletions == 0);
          for (int i = 0; i < count; ++i)
          {
            assert(std::isfinite(actual[0][i]) && actual[0][i] == actual[1][i]);
            maximumError = std::max(maximumError, std::abs(actual[0][i] - expected[i]));
            assert(std::abs(actual[0][i] - expected[i]) < 1.e-6);
          }
          samples += count;
          position += count;
        }
      }
  // This must catch upstream FFTConvolver's 1e-6 tail trimming, even though its
  // error could pass a looser general floating-point tolerance.
  for (int maxBlock : {1, 32, 64, 128, 511})
  {
    AdaptiveIR::IRData data;
    data.mRawAudioSampleRate = 48000.;
    data.mRawAudio.assign(8192, 4.e-6f);
    data.mRawAudio[0] = 1.f;
    AdaptiveIR candidate(data, 48000., maxBlock);
    std::vector<double> input(maxBlock, 0.);
    double* ptr = input.data();
    for (int position = 0; position < 8448;)
    {
      const int count = std::min(maxBlock, 8448 - position);
      std::fill(input.begin(), input.end(), 0.);
      if (!position)
        input[0] = 1.;
      auto* actual = candidate.Process(&ptr, 1, count)[0];
      const float gain = std::pow(10., -18 * .05);
      for (int i = 0; i < count; ++i)
      {
        const float expected = position + i < 8192 ? gain * data.mRawAudio[position + i] : 0.f;
        assert(std::abs(actual[i] - expected) < 2.e-8);
        if (position + i == 8191)
          assert(actual[i] > 4.e-7);
      }
      position += count;
    }
  }
  AdaptiveIR::IRData silence{{}, 48000.};
  // Long mode preserves late coefficients and places the large FFT tail at
  // exactly frame 2048, including partial host blocks and sample-rate conversion.
  for (double rate : {44100., 48000., 96000., 192000.})
    for (int maxBlock : {1, 127, 512, 4096})
    {
      AdaptiveIR::IRData data{{}, 48000.};
      data.mRawAudio.assign(17001, 0.f);
      for (size_t n : {0, 63, 2047, 2048, 4095, 8191, 8192, 16384, 17000})
        data.mRawAudio[n] = n == 17000 ? 4.e-6f : .1f;
      AdaptiveIR candidate(data, rate, maxBlock, AdaptiveIR::Mode::Long);
      std::vector<float> weights;
      if (rate == 48000.) weights = data.mRawAudio;
      else
      {
        std::vector<float> padded(data.mRawAudio.size() + 2, 0.f);
        std::copy(data.mRawAudio.begin(), data.mRawAudio.end(), padded.begin() + 1);
        dsp::ResampleCubic<float>(padded, 48000., rate, 0., weights);
      }
      assert(candidate.Length() == weights.size());
      assert(candidate.GetMode() == AdaptiveIR::Mode::Long && candidate.GetBackend() == AdaptiveIR::Backend::TwoStageFFT);
      const float gain = std::pow(10., -18 * .05) * 48000 / rate;
      std::vector<double> input(maxBlock);
      double* pointer = input.data();
      const std::array<int, 7> blocks{1, 3, 127, 511, 2047, 2048, 4096};
      for (size_t position = 0, block = 0; position < weights.size() + 4096; ++block)
      {
        const int count = static_cast<int>(std::min({static_cast<size_t>(maxBlock),
          weights.size() + 4096 - position, static_cast<size_t>(blocks[block % blocks.size()])}));
        std::fill(input.begin(), input.end(), 0.);
        if (position == 0) input[0] = 1.;
        allocations = deletions = 0; tracking = true;
        const auto* result = candidate.Process(&pointer, 1, count)[0];
        tracking = false;
        assert(allocations == 0 && deletions == 0);
        for (int i = 0; i < count; ++i)
        {
          const double expected = position + i < weights.size() ? weights[position + i] * gain : 0.;
          assert(std::abs(result[i] - expected) < 2.e-8);
        }
        position += count;
      }
    }
  {
    AdaptiveIR::IRData data{{}, 48000.};
    data.mRawAudio.assign(1440001, 0.f);
    data.mRawAudio[1439999] = .2f;
    data.mRawAudio.back() = 1.f;
    AdaptiveIR bounded(data, 48000., 512, AdaptiveIR::Mode::Long);
    assert(bounded.Length() == 1440000);
    std::array<double, 512> input{};
    double* pointer = input.data();
    const float gain = std::pow(10., -18 * .05);
    for (int position = 0; position < 1440512; position += 512)
    {
      input.fill(0.);
      if (!position) input[0] = 1.;
      auto* output = bounded.Process(&pointer, 1, 512)[0];
      for (int i = 0; i < 512; ++i)
        assert(std::abs(output[i] - (position + i == 1439999 ? .2f * gain : 0.f)) < 2.e-8);
    }
  }
  // Independent sparse FIR oracle with sustained excitation across many tail
  // partitions, not just a single impulse in the first block.
  for (int maxBlock : {1, 127, 512, 4096})
  {
    AdaptiveIR::IRData data{{}, 48000.};
    data.mRawAudio.assign(17001, 0.f);
    const std::array<int, 9> taps{0, 127, 2047, 2048, 4095, 8191, 8192, 16384, 17000};
    for (int n : taps) data.mRawAudio[n] = n % 2 ? -.15f : .2f;
    AdaptiveIR candidate(data, 48000., maxBlock, AdaptiveIR::Mode::Long);
    std::vector<float> excitation(10241);
    for (size_t i = 0; i < excitation.size(); ++i) excitation[i] = static_cast<float>(.2 * std::sin(i * .079));
    std::vector<double> input(maxBlock);
    double* pointer = input.data();
    const int duration = static_cast<int>(excitation.size() + data.mRawAudio.size() + 512);
    const float gain = std::pow(10., -18 * .05);
    for (int position = 0, block = 0; position < duration; ++block)
    {
      const int count = std::min({maxBlock, duration - position, std::array<int, 5>{3, 127, 512, 2047, 4096}[block % 5]});
      for (int i = 0; i < count; ++i)
        input[i] = position + i < excitation.size() ? excitation[position + i] : 0.;
      auto* result = candidate.Process(&pointer, 1, count)[0];
      for (int i = 0; i < count; ++i)
      {
        double expected = 0.;
        for (int n : taps)
          if (position + i >= n && position + i - n < excitation.size())
            expected += static_cast<double>(data.mRawAudio[n] * gain) * excitation[position + i - n];
        assert(std::abs(result[i] - expected) < 1.e-7);
      }
      position += count;
    }
  }
  silence.mRawAudio.assign(8192, 0.f);
  AdaptiveIR zero(silence, 48000., 128);
  std::array<double, 128> zeros{};
  double* ptr = zeros.data();
  for (int i = 0; i < 128; ++i)
    zeros[i] = .2;
  auto* actual = zero.Process(&ptr, 1, 128)[0];
  for (int i = 0; i < 128; ++i)
    assert(actual[i] == 0.);
  for (double badRate : {0., -1., std::numeric_limits<double>::quiet_NaN()})
  {
    bool rejected = false;
    try
    {
      AdaptiveIR bad(silence, badRate, 128);
    }
    catch (const std::invalid_argument&)
    {
      rejected = true;
    }
    assert(rejected);
  }
  std::cout << "Adaptive IR: " << samples
            << " reference samples, four rates, direct/FFT selection, variable blocks, "
               "quiet tails, silence, channel API and no processing allocation/deletion passed; max error "
            << maximumError << "\n";
}
