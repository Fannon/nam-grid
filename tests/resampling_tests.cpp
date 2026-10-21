#include "IPlugConstants.h"
#define DEFAULT_BLOCK_SIZE 2048
#include "../AudioDSPTools/dsp/ResamplingContainer/ResamplingContainer.h"
#include <array>
#include <cassert>
#include <iostream>

// Test the adapter directly with exact WDL allocations: its normal 4 KiB
// allocation granularity can conceal reads beyond the initialized warm-up data.
template <int Channels>
void TestResamplingWarmup()
{
  for (double modelRate : {8000., 44100., 48000., 192000.})
    for (double hostRate : {8000., 44100., 48000., 192000.})
      for (int maximum : {1, 3, 128})
      {
        dsp::ResamplingContainer<double, Channels, 12> resampler(modelRate);
        for (int reset = 0; reset < 2; ++reset)
        {
          resampler.Reset(hostRate, maximum);
          std::array<std::array<double, 128>, Channels> inputs{}, outputs{};
          std::array<double*, Channels> in{}, out{};
          for (int ch = 0; ch < Channels; ++ch)
          {
            in[ch] = inputs[ch].data(); out[ch] = outputs[ch].data();
          }
          for (int block = 0; block < 8; ++block)
          {
            for (auto& channel : outputs) channel.fill(1.);
            resampler.ProcessBlock(in.data(), out.data(), maximum, [](double** src, double** dst, int n) {
              for (int ch = 0; ch < Channels; ++ch) std::copy_n(src[ch], n, dst[ch]);
            });
            for (const auto& channel : outputs)
              for (int i = 0; i < maximum; ++i) assert(channel[i] == 0.);
          }
        }
      }
}

int main()
{
  TestResamplingWarmup<1>();
  TestResamplingWarmup<2>();
  std::cout << "Resampler warm-up/reset silence, mono/stereo and small blocks passed\n";
}
