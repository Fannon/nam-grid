#pragma once
#include "FixedNAMGrid.h"

// Included after the stage harness's prepare()/allocation counters.
void TestFixedGrid()
{
  const nlohmann::json linear{{"version", "0.5.4"},
                              {"architecture", "Linear"},
                              {"config", {{"receptive_field", 1}, {"bias", true}}},
                              {"weights", {1., 0.}},
                              {"sample_rate", 48000.}};
  std::array<FixedNAMGrid::Controls, 16> controls;
  std::array<double, 512> data{};
  FixedNAMGrid grid;
  std::array<double, 16> gains{}, biases{};
  for (int slot = 0; slot < 16; ++slot)
  {
    auto model = linear;
    gains[slot] = 1.02 + .03 * slot;
    biases[slot] = .005 * (slot + 1);
    model["weights"] = {gains[slot], biases[slot]};
    grid.stages[slot].Publish(prepare(model, 48000.));
  }
  for (bool bypassSome : {false, true})
    for (double blend : {0., .37, 1.})
    {
      for (int slot = 0; slot < 16; ++slot)
        controls[slot].enabled = !bypassSome || slot % 4 != 3;
      controls[4].inputDB = 6.020599913;
      controls[4].trimDB = -6.020599913;
      grid.ResetStopped(48000., 512);
      auto expectedRow = [&](double input, int first) {
        for (int slot = first; slot < first + 4; ++slot)
          if (controls[slot].enabled)
            input = (input * std::pow(10., controls[slot].inputDB * .05) * gains[slot] + biases[slot])
                    * std::pow(10., controls[slot].trimDB * .05);
        return input;
      };
      const auto pre = expectedRow(.05, 0);
      const auto expected = expectedRow((1. - blend) * expectedRow(pre, 4) + blend * expectedRow(pre, 8), 12);
      data.fill(.05);
      allocations = deletions = 0;
      tracking = true;
      Eigen::internal::set_is_malloc_allowed(false);
      grid.Process(data.data(), 512, controls, blend);
      Eigen::internal::set_is_malloc_allowed(true);
      tracking = false;
      assert(allocations == 0 && deletions == 0);
      for (double sample : data)
        assert(std::abs(sample - expected) < 1e-5);
    }
  // Empty lanes don't double correlated input, at any blend or during blend automation.
  FixedNAMGrid empty;
  empty.ResetStopped(48000., 512);
  for (double blend : {0., .2, .5, 1., 0.})
  {
    data.fill(.125);
    empty.Process(data.data(), 512, controls, blend);
    for (double sample : data)
      assert(sample == .125);
  }

  // The steeper lane split: a fourth-order Linkwitz-Riley pair. Empty lanes still reconstruct
  // the signal, because the low and high outputs sum to a flat-magnitude allpass; the released
  // first-order pair remains bit-identical and is what a rig without the setting gets.
  for (double rate : {44100., 48000., 96000.})
  {
    const double frequency = 250.;
    const auto render = [&](bool steep, double tone, double blend, bool onlyA) {
      FixedNAMGrid grid;
      grid.ResetStopped(rate, 512);
      std::array<FixedNAMGrid::Controls, FixedNAMGrid::kSlots> routingControls{};
      grid_performance::Routing routing;
      routing.crossover = true; routing.frequency = frequency; routing.linkwitzRiley = steep;
      routing.muteB = onlyA;
      std::vector<double> output(static_cast<size_t>(rate * .5));
      for (int pos = 0; pos < static_cast<int>(output.size()); pos += 512)
      {
        const int n = std::min(512, static_cast<int>(output.size()) - pos);
        for (int i = 0; i < n; ++i)
          data[i] = .1 * std::sin(2. * grid_fx::pi * tone * (pos + i) / rate);
        grid.Process(data.data(), n, routingControls, blend, nullptr, routing);
        std::copy_n(data.data(), n, output.data() + pos);
      }
      return output;
    };
    const auto rms = [&](const std::vector<double>& x) {
      double sum = 0.;
      for (size_t i = x.size() / 2; i < x.size(); ++i) sum += x[i] * x[i];
      return std::sqrt(sum / (x.size() - x.size() / 2));
    };
    for (double tone : {60., frequency, 1000., 5000.})
    {
      const double dry = .1 / std::sqrt(2.);
      for (bool steep : {false, true})
        assert(std::abs(rms(render(steep, tone, .5, false)) - dry) < .005 * dry);
    }
    // A fourth-order low pass is far steeper: near 20x the split, lane A keeps little of the tone.
    {
      const double released = rms(render(false, 5000., 0., true)), steep = rms(render(true, 5000., 0., true));
      assert(steep < .02 * released);
    }
    // LR4's phase is allpass, not identical: at the split the two lanes sum inverted, while the
    // released complementary pair reproduces the input sample for sample.
    {
      const auto exactDry = render(false, frequency, .5, false);
      const auto steep = render(true, frequency, .5, false);
      double releasedError = 0., steepError = 0.;
      for (size_t i = steep.size() / 2; i < steep.size(); ++i)
      {
        const double dry = .1 * std::sin(2. * grid_fx::pi * frequency * i / rate);
        releasedError = std::max(releasedError, std::abs(exactDry[i] - dry));
        steepError = std::max(steepError, std::abs(steep[i] + dry));
      }
      assert(releasedError < 1.e-9);
      assert(steepError < .01 * .1);
    }
    // The released path is bit-identical to its own definition: a one-pole low pass whose
    // complementary high pass and the A/B blend are rebuilt here in the test.
    {
      const double blend = .37, coefficient = std::exp(-6.283185307179586 * std::min(.4 * rate, frequency) / rate);
      std::array<double, 2> low{};
      const auto output = render(false, 300., blend, false);
      for (size_t i = 0; i < output.size(); ++i)
      {
        const double dry = .1 * std::sin(2. * grid_fx::pi * 300. * i / rate);
        low[0] = dry + coefficient * (low[0] - dry);
        const double laneA = dry + (low[0] - dry), laneB = dry - low[0];
        const double expected = 2. * ((1. - blend) * laneA + blend * laneB);
        assert(output[i] == expected);
      }
    }
  }

  // Returning to Steep after silence must not replay histories frozen by Gentle.
  // Low crossover frequencies retain energy well beyond the 5 ms slope fade.
  for (double rate : {44100., 48000., 96000.})
    for (double frequency : {40., 60.})
      for (int block : {37, 512})
        for (bool stereo : {false, true})
        {
          FixedNAMGrid split;
          split.ResetStopped(rate, block);
          std::array<FixedNAMGrid::Controls, FixedNAMGrid::kSlots> routingControls{};
          grid_performance::Routing routing;
          routing.crossover = routing.linkwitzRiley = true;
          routing.frequency = frequency;
          int position = 0;
          const auto render = [&](double seconds, bool tone) {
            double peak = 0.;
            const int frames = static_cast<int>(rate * seconds);
            std::array<double, 512> left{}, right{};
            for (int done = 0; done < frames; done += block)
            {
              const int n = std::min(block, frames - done);
              for (int i = 0; i < n; ++i)
              {
                const double phase = 2. * grid_fx::pi * frequency * (position + i) / rate;
                left[i] = tone ? .2 * std::sin(phase) : 0.;
                right[i] = tone ? .15 * std::sin(phase + .7) : 0.;
              }
              split.Process(left.data(), n, routingControls, .5, stereo ? right.data() : nullptr, routing);
              for (int i = 0; i < n; ++i)
                peak = std::max({peak, std::abs(left[i]), stereo ? std::abs(right[i]) : 0.});
              position += n;
            }
            return peak;
          };
          render(1., true);
          allocations = deletions = 0;
          tracking = true;
          Eigen::internal::set_is_malloc_allowed(false);
          routing.linkwitzRiley = false;
          render(1.1, false);
          routing.linkwitzRiley = true;
          const double resumedPeak = render(.1, false);
          Eigen::internal::set_is_malloc_allowed(true);
          tracking = false;
          assert(allocations == 0 && deletions == 0);
          assert(resumedPeak < 1.e-12);
        }

  const auto nano = fixture::model(3, 123), standard = fixture::model(8, 456);
  for (double rate : {48000., 44100., 96000.})
  {
    FixedNAMGrid a2grid;
    a2grid.stages[0].Publish(prepare(nano, rate));
    a2grid.stages[4].Publish(prepare(nano, rate));
    a2grid.stages[8].Publish(prepare(standard, rate));
    a2grid.stages[12].Publish(prepare(standard, rate));
    a2grid.ResetStopped(rate, 512);
    controls = {};
    auto pre = prepare(nano, rate), a = prepare(nano, rate), b = prepare(standard, rate),
         post = prepare(standard, rate);
    std::array<double, 512> split{}, branchA{}, branchB{}, mixed{}, expected{};
    for (int block = 0; block < 12; ++block)
    {
      const int n = std::array<int, 6>{1, 3, 127, 64, 256, 512}[block % 6];
      for (int i = 0; i < n; ++i)
        data[i] = .1 * std::sin((block * 512 + i) * .03);
      double* in = data.data();
      double* fork = split.data();
      double* laneA = branchA.data();
      double* laneB = branchB.data();
      double* merge = mixed.data();
      double* out = expected.data();
      pre->model->process(&in, &fork, n);
      a->model->process(&fork, &laneA, n);
      b->model->process(&fork, &laneB, n);
      for (int i = 0; i < n; ++i)
        mixed[i] = .7 * branchA[i] + .3 * branchB[i];
      post->model->process(&merge, &out, n);
      allocations = deletions = 0;
      tracking = true;
      Eigen::internal::set_is_malloc_allowed(false);
      a2grid.Process(data.data(), n, controls, .3);
      Eigen::internal::set_is_malloc_allowed(true);
      tracking = false;
      assert(allocations == 0 && deletions == 0);
      for (int i = 0; i < n; ++i)
        assert(std::isfinite(data[i]) && std::abs(data[i] - expected[i]) < 1e-6);
    }
    // Unequal lane delays: bypassed models supply exact delayed dry signals for an impulse oracle.
    FixedNAMGrid aligned;
    for (int slot : {0, 4, 5, 8, 12})
      aligned.stages[slot].Publish(prepare(slot == 8 ? standard : nano, rate));
    for (int maxBlock : {512, 1})
    {
      aligned.ResetStopped(rate, maxBlock);
      controls = {};
      for (auto& control : controls)
        control.enabled = false;
      std::array<int, 16> latencies{};
      for (int slot = 0; slot < 16; ++slot)
        latencies[slot] = aligned.stages[slot].Latency();
      const auto delay = FixedNAMGrid::TotalLatency(latencies);
      int position = 0;
      while (position < 1024)
      {
        const auto frames = std::min(maxBlock, 1024 - position);
        data.fill(0.);
        if (position == 0)
          data[0] = 1.;
        allocations = deletions = 0;
        tracking = true;
        Eigen::internal::set_is_malloc_allowed(false);
        aligned.Process(data.data(), frames, controls, .5);
        Eigen::internal::set_is_malloc_allowed(true);
        tracking = false;
        assert(allocations == 0 && deletions == 0);
        for (int i = 0; i < frames; ++i)
          assert(data[i] == (position + i == delay ? 1. : 0.));
        position += frames;
      }
    }
  }
  // Batch only an independent input prefix and compare with the original
  // quantum schedule. Cover every cut point, stereo, odd callback boundaries,
  // moving downstream controls and taps on either side of the cut. No audio
  // allocation or destruction is permitted in either path.
  for (int prefix = 0; prefix <= 4; ++prefix)
  {
    FixedNAMGrid reference, batched;
    for (int slot : {0, 1, 3, 4, 8, 12})
    {
      reference.stages[slot].Publish(prepare(linear, 48000.));
      batched.stages[slot].Publish(prepare(linear, 48000.));
    }
    reference.ResetStopped(48000., 512); batched.ResetStopped(48000., 512);
    controls = {};
    std::array<double, 512> l{}, r{}, expectedL{}, expectedR{};
    int position = 0;
    for (int block = 0; block < 16; ++block)
    {
      const int n = std::array<int, 5>{1, 3, 37, 64, 128}[block % 5];
      const int first = 16 - position % 16;
      const int tap0 = std::array<int, 8>{-1, 0, 1, 3, 4, 8, 12, 15}[block % 8];
      const int tap1 = std::array<int, 8>{15, 12, 8, 4, 3, 1, 0, -1}[block % 8];
      reference.SetTap(tap0); batched.SetTap(tap0);
      reference.SetTap(tap1, 1); batched.SetTap(tap1, 1);
      for (int i = 0; i < n; ++i)
      {
        expectedL[i] = l[i] = .1 * std::sin((position + i) * .03);
        expectedR[i] = r[i] = .07 * std::cos((position + i) * .07);
      }
      controls[0].inputDB = block > 5 ? 3. : 0.; // Same first-quantum gain ramp.
      allocations = deletions = 0; tracking = true;
      Eigen::internal::set_is_malloc_allowed(false);
      if (prefix) batched.ProcessInputPrefix(l.data(), n, controls, prefix, first, r.data());
      for (int offset = 0; offset < n;)
      {
        const int count = std::min(16 - (position + offset) % 16, n - offset);
        // The first non-prefix destination moves every quantum.
        controls[prefix].trimDB = (position + offset) % 5 - 2.;
        const double blend = .3 + .1 * std::sin((position + offset) * .001);
        reference.Process(expectedL.data() + offset, count, controls, blend, expectedR.data() + offset);
        batched.Process(l.data() + offset, count, controls, blend, r.data() + offset, {}, prefix);
        for (int instance = 0; instance < 2; ++instance)
        {
          const int tap = instance ? tap1 : tap0;
          if (tap < 0) continue;
          const auto* actualL = batched.TapLeft(instance) + (tap < prefix ? offset : 0);
          const auto* actualR = batched.TapRight(instance) + (tap < prefix ? offset : 0);
          for (int i = 0; i < count; ++i)
          {
            assert(std::abs(actualL[i] - reference.TapLeft(instance)[i]) < 1.e-12);
            assert(std::abs(actualR[i] - reference.TapRight(instance)[i]) < 1.e-12);
          }
        }
        offset += count;
      }
      Eigen::internal::set_is_malloc_allowed(true); tracking = false;
      assert(allocations == 0 && deletions == 0);
      for (int i = 0; i < n; ++i)
      {
        assert(std::abs(l[i] - expectedL[i]) < 1.e-12);
        assert(std::abs(r[i] - expectedR[i]) < 1.e-12);
      }
      position += n;
    }
  }
  std::cout << "16-slot order/gain/bypass, parallel blend, A2 references and aligned impulses passed\n";
}

void TestRealGrid(int count, char** files)
{
  assert(count <= 4);
  const std::array<int, 4> positions{0, 4, 8, 12};
  FixedNAMGrid grid;
  std::array<FixedNAMGrid::Controls, 16> controls;
  for (int i = 0; i < count; ++i)
  {
    auto model = nam::get_dsp(std::filesystem::u8path(files[i]));
    assert(model->NumInputChannels() == 1 && model->NumOutputChannels() == 1);
    auto prepared = std::make_unique<NAMStage::Prepared>();
    prepared->model = std::make_unique<ResamplingNAM>(std::move(model), 48000.);
    prepared->Reset(48000., 512);
    grid.stages[positions[i]].Publish(std::move(prepared));
  }
  grid.ResetStopped(48000., 512);
  std::array<double, 512> audio{};
  double energy = 0.;
  for (int block = 0; block < 64; ++block)
  {
    const int frames = std::array<int, 6>{1, 3, 127, 64, 256, 512}[block % 6];
    for (int i = 0; i < frames; ++i)
      audio[i] = .03 * std::sin((block * 512 + i) * .03);
    allocations = deletions = 0;
    tracking = true;
    Eigen::internal::set_is_malloc_allowed(false);
    grid.Process(audio.data(), frames, controls, .5);
    Eigen::internal::set_is_malloc_allowed(true);
    tracking = false;
    assert(allocations == 0 && deletions == 0);
    for (int i = 0; i < frames; ++i)
    {
      assert(std::isfinite(audio[i]));
      energy += audio[i] * audio[i];
    }
  }
  assert(energy > 0.);
  std::cout << "Real-file input / A / B / output grid probe: " << count
            << " models, finite nonzero audio, no allocations\n";
}
