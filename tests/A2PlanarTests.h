#pragma once
#include "A2Fixture.h"
#include "../NeuralAmpModelerCore/NAM/wavenet/a2_planar.h"
#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstring>
#include <iostream>
#include <typeinfo>
#include <vector>

inline void TestA2Planar()
{
#if defined(NAM_A2_PLANAR)
  using namespace nam::wavenet::a2_fast;
  // Compare against the portable engine, including sub-vector tails, ring wraps
  // and repeated resets that restore the planar engine's cached prewarm state.
  for (int channels : {3, 8})
  {
    const auto model = fixture::model(channels, 0xA2 + channels);
    const auto weights = model["weights"].get<std::vector<float>>();
    auto reference = create_a2_fast_reference_model(channels, weights, 48000.);
    auto planar = create_a2_planar_model(channels, weights, 48000.);
    auto selected = create_a2_fast_config(model["config"], 48000.)->create(weights, 48000.);
    assert(planar && selected && typeid(*selected) == typeid(*planar));
    std::vector<NAM_SAMPLE> input(20000), expected(input.size()), actual(input.size());
    for (double rate : {44100., 48000., 96000.})
    {
      for (size_t i = 0; i < input.size(); ++i)
        input[i] = i < 15000 ? .25 * std::sin(6.283185307179586 * 220. * i / rate)
                              + .1 * std::sin(6.283185307179586 * 1230. * i / rate) : 0.;
      input[0] += .5;
      for (int block : {1, 3, 4, 7, 8, 15, 16, 31, 32, 33, 64, 65, 128, 512})
      {
        reference->ResetAndPrewarm(rate, block);
        planar->ResetAndPrewarm(rate, block);
        for (size_t offset = 0; offset < input.size(); offset += block)
        {
          const int frames = std::min<size_t>(block, input.size() - offset);
          NAM_SAMPLE* in[] = {input.data() + offset};
          NAM_SAMPLE* a[] = {expected.data() + offset};
          NAM_SAMPLE* b[] = {actual.data() + offset};
          reference->process(in, a, frames);
          planar->process(in, b, frames);
        }
        if (std::memcmp(expected.data(), actual.data(), actual.size() * sizeof(NAM_SAMPLE)) != 0)
          std::cerr << "A2 planar mismatch: " << channels << " channels, " << rate << " Hz, " << block << " frames\n";
        assert(std::memcmp(expected.data(), actual.data(), actual.size() * sizeof(NAM_SAMPLE)) == 0);
      }
    }
  }
  std::cout << "A2 planar/reference bit identity, factory dispatch and repeated prewarm passed\n";
#endif
}
