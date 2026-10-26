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
