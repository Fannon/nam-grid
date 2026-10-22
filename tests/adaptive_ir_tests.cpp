#include "AdaptiveIR.h"
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
