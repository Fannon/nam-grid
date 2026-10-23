#include "engine.h"
#include "ImpulseResponse.h"
#include "Resample.h"
#include "FFTConvolver.h"
#include "TwoStageFFTConvolver.h"
#include <json.hpp>
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <fstream>
#include <iostream>
#include <limits>
#include <numeric>
#include <random>
#include <stdexcept>
#include <string>

void* operator new(size_t n)
{
  if (auto* p = ir_bench::Malloc(n ? n : 1))
    return p;
  throw std::bad_alloc();
}
void* operator new[](size_t n)
{ return ::operator new(n); }
void operator delete(void* p) noexcept
{ ir_bench::Free(p); }
void operator delete[](void* p) noexcept
{ ir_bench::Free(p); }
void operator delete(void* p, size_t) noexcept
{ ir_bench::Free(p); }
void operator delete[](void* p, size_t) noexcept
{ ir_bench::Free(p); }

using json = nlohmann::json;
using Clock = std::chrono::steady_clock;
using namespace ir_bench;
static void Require(bool value, const char* message)
{
  if (!value)
    throw std::runtime_error(message);
}
struct Asset
{
  dsp::ImpulseResponse::IRData source;
  std::vector<float> weights;
};
static Asset LoadAsset(const json& config)
{
  Asset asset;
  auto& raw = asset.source.mRawAudio;
  asset.source.mRawAudioSampleRate = 48000.;
  const auto name = config.at("asset").get<std::string>();
  if (name == "wav")
  {
    Require(dsp::wav::Load(config.at("path").get<std::string>().c_str(), raw, asset.source.mRawAudioSampleRate)
              == dsp::wav::LoadReturnCode::SUCCESS,
            "WAV load failed");
  }
  else
  {
    const int length = config.at("length");
    raw.resize(length);
    std::mt19937 generator(42);
    for (int i = 0; i < length; ++i)
    {
      // Dense tail deliberately above FFTConvolver's 1e-6 trimming threshold.
      const double random = static_cast<double>(generator()) / generator.max() * 2. - 1.;
      raw[i] = static_cast<float>(.04 * random * (.1 + .9 * std::exp(-6. * i / length)));
    }
    raw[0] = .8f;
    raw.back() = .01f;
    if (name == "quiet-tail")
    {
      std::fill(raw.begin() + 128, raw.end(), 4.e-6f);
      raw[127] = .02f;
    }
    if (name == "silence")
      std::fill(raw.begin(), raw.end(), 0.f);
  }
  Require(!raw.empty(), "Empty IR unsupported by reference");
  const double rate = config.at("rate");
  if (asset.source.mRawAudioSampleRate == rate)
    asset.weights = raw;
  else
  {
    std::vector<float> padded(raw.size() + 2, 0.f);
    std::copy(raw.begin(), raw.end(), padded.begin() + 1);
    dsp::ResampleCubic<float>(padded, asset.source.mRawAudioSampleRate, rate, 0., asset.weights);
  }
  asset.weights.resize(std::min<size_t>(8192, asset.weights.size()));
  const float gain = std::pow(10., -18 * .05) * 48000 / rate;
  for (auto& weight : asset.weights)
    weight *= gain;
  return asset;
}
class Direct final : public Engine
{
  dsp::ImpulseResponse mLeft, mRight;

public:
  Direct(const Asset& asset, double rate)
  : mLeft(asset.source, rate)
  , mRight(asset.source, rate)
  {
  }
  void Process(double* const* input, double** output, int frames) override
  {
    auto* left = input[0];
    auto* right = input[1];
    std::copy_n(mLeft.Process(&left, 1, frames)[0], frames, output[0]);
    std::copy_n(mRight.Process(&right, 1, frames)[0], frames, output[1]);
  }
};
template <class Convolver>
class Fft final : public Engine
{
  std::array<Convolver, 2> mEngine;
  std::vector<float> mInput, mOutput;

public:
  Fft(const Asset& asset, int block)
  : mInput(block)
  , mOutput(block)
  {
    for (auto& engine : mEngine)
    {
      if constexpr (std::is_same_v<Convolver, fftconvolver::FFTConvolver>)
        Require(engine.init(block, asset.weights.data(), asset.weights.size()), "FFT init failed");
      else
        Require(engine.init(block, 1024, asset.weights.data(), asset.weights.size()), "Two-stage init failed");
    }
  }
  void Process(double* const* input, double** output, int frames) override
  {
    for (int ch = 0; ch < 2; ++ch)
    {
      for (int i = 0; i < frames; ++i)
        mInput[i] = static_cast<float>(input[ch][i]);
      mEngine[ch].process(mInput.data(), mOutput.data(), frames);
      std::copy_n(mOutput.data(), frames, output[ch]);
    }
  }
};
static std::unique_ptr<Engine> Make(const std::string& name, const Asset& asset, int block, int rate)
{
  if (name == "direct")
    return std::make_unique<Direct>(asset, rate);
  if (name == "fft-uniform")
    return std::make_unique<Fft<fftconvolver::FFTConvolver>>(asset, block);
  if (name == "fft-two-stage")
    return std::make_unique<Fft<fftconvolver::TwoStageFFTConvolver>>(asset, block);
  if (name == "wdl-div")
    return MakeWdl(asset.weights, block);
  throw std::runtime_error("Unknown engine");
}
struct Audio
{
  std::array<std::vector<double>, 4> data;
  double *input[2], *output[2];
  Audio(int size)
  {
    for (auto& channel : data)
      channel.resize(size);
    for (int ch = 0; ch < 2; ++ch)
    {
      input[ch] = data[ch].data();
      output[ch] = data[ch + 2].data();
    }
  }
};
static void Prime(Engine& engine, int block)
{
  Audio audio(block);
  // Exercise delayed partitions and reserve lazily allocated WDL/reference storage offline.
  for (int i = 0; i < 32768; i += block)
    engine.Process(audio.input, audio.output, block);
}
static std::array<std::vector<double>, 2> Render(Engine& engine, const std::array<std::vector<double>, 2>& input,
                                                 bool variable, int maxBlock)
{
  Audio audio(maxBlock);
  std::array<std::vector<double>, 2> output;
  for (auto& ch : output)
    ch.resize(input[0].size());
  const std::array<int, 8> blocks{1, 7, 31, 64, 13, 128, 3, 32};
  for (size_t pos = 0, tick = 0; pos < input[0].size(); ++tick)
  {
    const int count = static_cast<int>(
      std::min(input[0].size() - pos,
               static_cast<size_t>(variable ? std::min(maxBlock, blocks[tick % blocks.size()]) : maxBlock)));
    for (int ch = 0; ch < 2; ++ch)
      std::copy_n(input[ch].data() + pos, count, audio.input[ch]);
    tracking = true;
    engine.Process(audio.input, audio.output, count);
    tracking = false;
    for (int ch = 0; ch < 2; ++ch)
      std::copy_n(audio.output[ch], count, output[ch].data() + pos);
    pos += count;
  }
  return output;
}
static json Audit(const json& config)
{
  const auto asset = LoadAsset(config);
  const auto name = config.at("engine").get<std::string>();
  const int rate = config.at("rate"), block = config.at("buffer");
  const int length = static_cast<int>(asset.weights.size()) + 4096;
  std::array<std::vector<double>, 2> input;
  for (auto& ch : input)
    ch.resize(length);
  auto reference = Make("direct", asset, block, rate);
  auto candidate = Make(name, asset, block, rate);
  Prime(*reference, block);
  Prime(*candidate, block);
  input[0][13] = 1.;
  allocations = releases = 0;
  const auto impulse = Render(*candidate, input, true, block);
  double impulseError = 0., crosstalk = 0.;
  for (int i = 0; i < length; ++i)
  {
    const double expected = i >= 13 && i - 13 < asset.weights.size() ? asset.weights[i - 13] : 0.;
    impulseError = std::max(impulseError, std::abs(impulse[0][i] - expected));
    crosstalk = std::max(crosstalk, std::abs(impulse[1][i]));
  }
  // Recreating/preparing clears history without changing the IR. No runtime reset API assumption.
  candidate = Make(name, asset, block, rate);
  Prime(*candidate, block);
  std::mt19937 generator(100);
  for (int i = 0; i < 4096; ++i)
    for (int ch = 0; ch < 2; ++ch)
      input[ch][i] =
        .15 * (2. * static_cast<double>(generator()) / generator.max() - 1.) + .1 * std::sin(.071 * i + ch);
  const auto expected = Render(*reference, input, false, block);
  allocations = releases = 0;
  const auto actual = Render(*candidate, input, true, block);
  const size_t allocs = allocations, frees = releases;
  double maxError = 0., errorEnergy = 0., referenceEnergy = 0.;
  for (int ch = 0; ch < 2; ++ch)
    for (int i = 0; i < length; ++i)
    {
      Require(std::isfinite(actual[ch][i]), "Non-finite output");
      const double error = actual[ch][i] - expected[ch][i];
      maxError = std::max(maxError, std::abs(error));
      errorEnergy += error * error;
      referenceEnergy += expected[ch][i] * expected[ch][i];
    }
  const double relativeError = std::sqrt(errorEnergy / std::max(1.e-30, referenceEnergy));
  // 1e-5 FS absolute and 1e-4 relative RMS are experiment gates, not listening claims.
  return {{"max_error", maxError},
          {"relative_rms_error", relativeError},
          {"impulse_error", impulseError},
          {"crosstalk", crosstalk},
          {"latency_samples", candidate->Latency()},
          {"allocations", allocs},
          {"releases", frees},
          {"effective_taps", asset.weights.size()},
          {"pass", maxError < 1.e-5 && relativeError < 1.e-4 && impulseError < 1.e-5 && crosstalk == 0.
                     && candidate->Latency() == 0}};
}
static json Stats(std::vector<double> times, double budget)
{
  const double mean = std::accumulate(times.begin(), times.end(), 0.) / times.size();
  const auto misses = std::count_if(times.begin(), times.end(), [&](double time) { return time > budget; });
  std::sort(times.begin(), times.end());
  return {{"mean_us", mean},
          {"cpu_percent", mean / budget * 100.},
          {"p99_us", times[static_cast<size_t>(std::ceil(.99 * times.size())) - 1]},
          {"max_us", times.back()},
          {"deadline_misses", misses},
          {"blocks", times.size()}};
}
static json Measure(const json& config)
{
  const auto asset = LoadAsset(config);
  const auto name = config.at("engine").get<std::string>();
  const int rate = config.at("rate"), block = config.at("buffer");
  const auto start = Clock::now();
  auto engine = Make(name, asset, block, rate);
  Prime(*engine, block);
  const double prepareMs = std::chrono::duration<double, std::milli>(Clock::now() - start).count();
  Audio audio(block);
  const int warmBlocks = static_cast<int>(std::ceil(config.value("warmup", .5) * rate / block));
  const int blocks = static_cast<int>(std::ceil(config.value("seconds", 3.) * rate / block));
  std::vector<double> times, signal, silence;
  times.reserve(blocks);
  signal.reserve(blocks);
  silence.reserve(blocks);
  double peak = 0.;
  size_t nonfinite = 0, position = 0;
  allocations = releases = 0;
  for (int tick = -warmBlocks; tick < blocks; ++tick)
  {
    const bool quiet = tick >= blocks * .75;
    for (int i = 0; i < block; ++i)
    {
      const double t = static_cast<double>(position + i) / rate;
      audio.input[0][i] = quiet ? 0. : .2 * std::sin(691.15038 * t) + .05 * std::sin(11058.406 * t);
      audio.input[1][i] = quiet ? 0. : .18 * std::sin(1035.546 * t + .3) + .03 * std::sin(16568.76 * t);
      for (int ch = 0; ch < 2; ++ch)
        audio.output[ch][i] = std::numeric_limits<double>::quiet_NaN();
    }
    tracking = true;
    const auto before = Clock::now();
    engine->Process(audio.input, audio.output, block);
    const auto after = Clock::now();
    tracking = false;
    if (tick >= 0)
    {
      const double us = std::chrono::duration<double, std::micro>(after - before).count();
      times.push_back(us);
      (quiet ? silence : signal).push_back(us);
    }
    for (int ch = 0; ch < 2; ++ch)
      for (int i = 0; i < block; ++i)
        if (!std::isfinite(audio.output[ch][i]))
          ++nonfinite;
        else
          peak = std::max(peak, std::abs(audio.output[ch][i]));
    position += block;
  }
  return {{"timing", Stats(times, 1.e6 * block / rate)},
          {"signal", Stats(signal, 1.e6 * block / rate)},
          {"silence", Stats(silence, 1.e6 * block / rate)},
          {"prepare_ms", prepareMs},
          {"allocations", allocations},
          {"releases", releases},
          {"nonfinite", nonfinite},
          {"peak", peak},
          {"effective_taps", asset.weights.size()},
          {"latency_samples", engine->Latency()}};
}
int main(int argc, char** argv)
try
{
  if (argc == 2 && std::string(argv[1]) == "--build-info")
  {
    std::cout << json{{"fft_preserve_tail", bool(IR_FFT_PRESERVE_TAIL)},
                      {"fft_sse", fftconvolver::SSEEnabled()},
                      {"wdl_fft_real_bytes", 4},
                      {"fft_backend", "bundled Ooura; double transforms, float spectra"},
                      {"two_stage_tail_partition", 1024}}
                   .dump()
              << '\n';
    return 0;
  }
  Require(argc == 3, "Usage: nam-ir-comparison --verify|--measure CONFIG.json");
  std::ifstream stream(argv[2]);
  auto config = json::parse(stream);
  const std::string mode = argv[1];
  Require(mode == "--verify" || mode == "--measure", "Unknown operation");
  auto result = mode == "--verify" ? Audit(config) : Measure(config);
  std::cout << result.dump() << '\n';
  return 0;
}
catch (const std::exception& error)
{
  tracking = false;
  std::cout << json{{"error", error.what()}}.dump() << '\n';
  return 1;
}
