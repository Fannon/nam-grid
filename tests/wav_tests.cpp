#include "WavFixtures.h"
#include "../AudioDSPTools/dsp/wav.h"
#include <array>
#include <cassert>
#include <cmath>
#include <iostream>
#include <iterator>
#include <limits>

int main(int argc, char** argv)
{
  assert(argc == 2);
  const auto directory = std::filesystem::path(argv[1]);
  std::filesystem::create_directories(directory);
  const auto file = directory / "decoder.wav";
  for (bool extensible : {false, true})
    for (int variant = 0; variant < 4; ++variant)
    {
      const int bits = variant == 0 ? 16 : variant == 1 ? 24 : 32;
      const bool floating = variant == 3;
      std::vector<uint32_t> values;
      if (floating)
        values = {0, 0x3f000000, 0xbf000000, 0x3f7fffff, 0xbf800000};
      else
      {
        const uint32_t sign = uint32_t{1} << (bits - 1);
        values = {0, sign / 2, sign + sign / 2, sign - 1, sign};
      }
      wav_fixture::Write(file, bits, floating, values, -1, -1, extensible);
      std::vector<float> audio;
      double rate = 0.;
      assert(dsp::wav::Load(file.string().c_str(), audio, rate) == dsp::wav::LoadReturnCode::SUCCESS);
      assert(rate == 48000. && audio.size() == 5);
      for (int i = 0; i < 5; ++i)
        assert(std::abs(audio[i] - std::array<double, 5>{0., .5, -.5, 1., -1.}[i]) < .00004);
      for (int bytes : {1, 3, 5, 7, -2})
      {
        // Both alignment and actual-remaining-byte checks are exercised. A
        // matching RIFF size prevents only the outer header from masking a bug.
        const int declared = bytes < 0 ? 128 : bytes;
        wav_fixture::Write(file, bits, floating, values, declared, bytes < 0 ? 2 : bytes, extensible);
        if (declared % (bits / 8) == 0 && bytes >= 0)
          continue;
        audio = {.25f};
        rate = 123.;
        assert(dsp::wav::Load(file.string().c_str(), audio, rate) == dsp::wav::LoadReturnCode::ERROR_INVALID_FILE);
        assert(audio == std::vector<float>{.25f} && rate == 123.);
      }
      for (uint32_t declared : {128u, 0x80000000u, 0xffffffffu})
      {
        wav_fixture::Write(file, bits, floating, values, static_cast<int>(declared), 2, extensible);
        // Write() uses -1 as a convenience default; overwrite the count explicitly.
        std::fstream patch(file, std::ios::binary | std::ios::in | std::ios::out);
        const int dataOffset = extensible ? 76 : 40;
        patch.seekp(dataOffset);
        for (int j = 0; j < 4; ++j)
          patch.put(static_cast<char>(declared >> (8 * j)));
        patch.close();
        assert(dsp::wav::Load(file.string().c_str(), audio, rate) == dsp::wav::LoadReturnCode::ERROR_INVALID_FILE);
      }
      // Every prefix of a valid file must fail until the complete payload exists.
      wav_fixture::Write(file, bits, floating, values, -1, -1, extensible);
      std::ifstream source(file, std::ios::binary);
      const std::vector<char> complete((std::istreambuf_iterator<char>(source)), {});
      source.close();
      const auto truncated = directory / "truncated.wav";
      for (size_t length = 0; length < complete.size() - (complete.size() % 2 && bits == 24 ? 1 : 0); ++length)
      {
        std::ofstream output(truncated, std::ios::binary);
        output.write(complete.data(), length);
        output.close();
        assert(dsp::wav::Load(truncated.string().c_str(), audio, rate) != dsp::wav::LoadReturnCode::SUCCESS);
      }
    }
  // Stereo decoding shares mono conversion and hardening, with frame alignment.
  for (bool extensible : {false, true})
    for (int variant = 0; variant < 4; ++variant)
    {
      const int bits = variant == 0 ? 16 : variant == 1 ? 24 : 32;
      const bool floating = variant == 3;
      const uint32_t half = floating ? 0x3f000000 : uint32_t{1} << (bits - 2);
      const uint32_t negative = floating ? 0xbf000000 : 3 * half;
      for (int channels : {0, 1, 2, 3, 4})
      {
        wav_fixture::Write(file, bits, floating, {half, negative, negative, half}, -1, -1, extensible, channels);
        std::vector<float> left{.25f}, right{.75f}; double rate = 123.;
        const auto result = dsp::wav::LoadIR(file.string().c_str(), left, right, rate);
        if (channels == 1 || channels == 2)
        {
          assert(result == dsp::wav::LoadReturnCode::SUCCESS && rate == 48000.);
          if (channels == 2)
          {
            assert((left == std::vector<float>{.5f, -.5f}));
            assert((right == std::vector<float>{-.5f, .5f}));
            assert(dsp::wav::Load(file.string().c_str(), left, rate) == dsp::wav::LoadReturnCode::ERROR_NOT_MONO);
          }
          else assert(right.empty());
        }
        else
        {
          assert(result == dsp::wav::LoadReturnCode::ERROR_UNSUPPORTED_CHANNELS);
          assert(left == std::vector<float>{.25f} && right == std::vector<float>{.75f} && rate == 123.);
        }
      }
      for (int payload : {1, 2, 3, 5, 7})
      {
        wav_fixture::Write(file, bits, floating, {half, negative}, 8 * bits / 8, payload, extensible, 2);
        std::vector<float> left{.25f}, right{.75f}; double rate = 123.;
        assert(dsp::wav::LoadIR(file.string().c_str(), left, right, rate) == dsp::wav::LoadReturnCode::ERROR_INVALID_FILE);
        assert(left == std::vector<float>{.25f} && right == std::vector<float>{.75f} && rate == 123.);
      }
      // Valid sample alignment but incomplete stereo frame.
      wav_fixture::Write(file, bits, floating, {half, negative, half}, -1, -1, extensible, 2);
      std::vector<float> left, right; double rate = 0.;
      assert(dsp::wav::LoadIR(file.string().c_str(), left, right, rate) == dsp::wav::LoadReturnCode::ERROR_INVALID_FILE);
    }
  // Low-level entry points are callable independently of Load(): do not leave
  // the original overflow reachable through their public declarations.
  for (int decoder = 0; decoder < 4; ++decoder)
    for (int count : {-1, 1, 3, 5, 128})
    {
      std::ofstream raw(file, std::ios::binary);
      raw.put(0);
      raw.put(0);
      raw.close();
      std::ifstream input(file, std::ios::binary);
      std::vector<float> output{.25f};
      switch (decoder)
      {
        case 0: dsp::wav::_LoadSamples16(input, count, output); break;
        case 1: dsp::wav::_LoadSamples24(input, count, output); break;
        case 2: dsp::wav::_LoadSamples32FixedPoint(input, count, output); break;
        case 3: dsp::wav::_LoadSamples32FloatingPoint(input, count, output); break;
      }
      assert(input.fail() && output == std::vector<float>{.25f});
    }
  std::cout
    << "WAV PCM16/24/32/float polarity, extensible, alignment, truncation, headers and transactional outputs passed\n";
}
