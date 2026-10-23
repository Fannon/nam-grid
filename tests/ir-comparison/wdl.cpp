// Instrument WDL's C allocations as well as the benchmark's global C++ new/delete.
// System headers are included before the local macro scope. Dependency files are unchanged.
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#ifdef _WIN32
  #include <windows.h>
#endif
#include "engine.h"
#define malloc ir_bench::Malloc
#define realloc ir_bench::Realloc
#define free ir_bench::Free
#include "convoengine.cpp"
#undef malloc
#undef realloc
#undef free

namespace ir_bench
{
class Wdl final : public Engine
{
  WDL_ConvolutionEngine_Div mEngine;
  std::vector<float> mLeft, mRight;

public:
  Wdl(const std::vector<float>& weights, int maxBlock)
  : mLeft(maxBlock)
  , mRight(maxBlock)
  {
    WDL_fft_init();
    WDL_ImpulseBuffer ir;
    ir.SetNumChannels(1);
    ir.SetLength(static_cast<int>(weights.size()));
    std::copy(weights.begin(), weights.end(), ir.impulses[0].Get());
    if (mEngine.SetImpulse(&ir, 0, maxBlock, 0, 0, 0) != 0)
      throw std::runtime_error("WDL added latency");
  }
  int Latency() const override { return const_cast<WDL_ConvolutionEngine_Div&>(mEngine).GetLatency(); }
  void Process(double* const* input, double** output, int frames) override
  {
    for (int i = 0; i < frames; ++i)
    {
      mLeft[i] = static_cast<float>(input[0][i]);
      mRight[i] = static_cast<float>(input[1][i]);
    }
    float* channels[]{mLeft.data(), mRight.data()};
    mEngine.Add(channels, frames, 2);
    if (mEngine.Avail(frames) != frames)
      throw std::runtime_error("WDL output underflow");
    auto** result = mEngine.Get();
    for (int ch = 0; ch < 2; ++ch)
      std::copy_n(result[ch], frames, output[ch]);
    mEngine.Advance(frames);
  }
};
std::unique_ptr<Engine> MakeWdl(const std::vector<float>& weights, int maxBlock)
{ return std::make_unique<Wdl>(weights, maxBlock); }
} // namespace ir_bench
