#pragma once
#include "GridTuner.h"
#include "SlotOperations.h"

void TestTunerTempoUtility()
{
  // Guitar/bass fundamentals with realistic harmonics, DC and changing level.
  std::array<double, grid_tuner::window> signal{};
  for (double rate : {11025., 12000.})
    for (double hz : {41.203, 61.735, 82.407, 110., 146.832, 196., 246.942, 329.628, 440., 659.255})
      for (double level : {.003, .2})
      {
        for (int i = 0; i < signal.size(); ++i)
        {
          const double phase = 6.283185307179586 * hz * i / rate;
          signal[i] = .07 + level * (std::sin(phase) + .45 * std::sin(phase * 2.) + .2 * std::sin(phase * 3.));
        }
        const double found = grid_tuner::Detect(signal.data(), rate);
        assert(found > 0. && std::abs(1200. * std::log2(found / hz)) < 2.);
      }
  signal.fill(.1);
  assert(grid_tuner::Detect(signal.data(), 12000.) == 0.);
  signal[10] = std::numeric_limits<double>::quiet_NaN();
  assert(grid_tuner::Detect(signal.data(), 12000.) == 0.);
  // Exercise the real decimator/worker at each host rate, including right-only input.
  auto tuner = std::make_unique<grid_tuner::Tuner>();
  std::array<double, 512> left{}, right{};
  for (double rate : {44100., 48000., 88200., 96000., 192000.})
  {
    tuner->Enable(true);
    const int blocks = static_cast<int>(std::ceil(rate * .24 / 512.));
    for (int b = 0; b < blocks; ++b)
    {
      left.fill(0.);
      for (int i = 0; i < 512; ++i)
        right[i] = .1 * std::sin(6.283185307179586 * 110. * (b * 512 + i) / rate);
      allocations = deletions = 0;
      tracking = true;
      tuner->Push(left.data(), right.data(), 512, rate);
      tracking = false;
      assert(allocations == 0 && deletions == 0);
      // Simulate host pacing and allow the worker to consume a bounded ring.
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    while (!tuner->Frequency() && std::chrono::steady_clock::now() < deadline)
      std::this_thread::sleep_for(std::chrono::milliseconds(5));
    assert(tuner->Frequency() > 0. && std::abs(1200. * std::log2(tuner->Frequency() / 110.)) < 2.);
    tuner->Enable(false);
    assert(tuner->Frequency() == 0.);
  }

  for (double rate : {44100., 48000., 96000., 192000.})
  {
    // Empty stereo grid preserves channel identity; utilities in opposite lanes pan
    // duplicated mono to independent outputs without correlated-lane gain buildup.
    FixedNAMGrid grid;
    std::array<FixedNAMGrid::Controls, 16> c{};
    grid.ResetStopped(rate, 512);
    left.fill(.1);
    right.fill(-.2);
    grid.Process(left.data(), 512, c, .37, right.data());
    for (int i = 0; i < 512; ++i)
    {
      assert(std::abs(left[i] - .1) < 1.e-12);
      assert(std::abs(right[i] + .2) < 1.e-12);
    }
    SlotConfig utility;
    utility.effect = SlotEffect::Utility;
    grid.stages[4].Publish(PrepareSlot(utility, rate, 512).dsp);
    grid.stages[8].Publish(PrepareSlot(utility, rate, 512).dsp);
    grid.ResetStopped(rate, 512);
    c[4].utility.balance = -100.;
    c[8].utility.balance = 100.;
    c[8].utility.invertR = true;
    left.fill(.2);
    right.fill(.2);
    allocations = deletions = 0;
    tracking = true;
    grid.Process(left.data(), 512, c, .5, right.data());
    tracking = false;
    assert(allocations == 0 && deletions == 0);
    for (int i = 0; i < 512; ++i)
    {
      assert(std::abs(left[i] - .1) < 1.e-12);
      assert(std::abs(right[i] + .1) < 1.e-12);
    }
    c[4].enabled = c[8].enabled = false;
    for (int b = 0; b < 8; ++b)
    {
      left.fill(.2);
      right.fill(-.1);
      grid.Process(left.data(), 512, c, .5, right.data());
    }
    assert(std::abs(left.back() - .2) < 1.e-12 && std::abs(right.back() + .1) < 1.e-12);
  }
  // A stereo input into a mono A2 stage matches a single explicitly downmixed
  // reference. Dry bypass retains both original channels, including resampling delay.
  for (double rate : {48000., 44100., 96000.})
  {
    auto model = fixture::model(3, 123);
    NAMStage stereo, mono;
    stereo.Publish(prepare(model, rate));
    mono.Publish(prepare(model, rate));
    stereo.ResetStopped(rate, 512);
    mono.ResetStopped(rate, 512);
    std::array<double, 512> average{};
    for (int b = 0; b < 6; ++b)
    {
      for (int i = 0; i < 512; ++i)
      {
        left[i] = .1 * std::sin((b * 512 + i) * .05);
        right[i] = .2 * std::cos((b * 512 + i) * .03);
        average[i] = .5 * (left[i] + right[i]);
      }
      allocations = deletions = 0;
      tracking = true;
      Eigen::internal::set_is_malloc_allowed(false);
      stereo.Process(left.data(), 512, true, 0, 0, false, 0, 0, 1, {}, {}, {}, right.data());
      mono.Process(average.data(), 512, true, 0, 0, false, 0, 0, 1);
      Eigen::internal::set_is_malloc_allowed(true);
      tracking = false;
      assert(allocations == 0 && deletions == 0);
      for (int i = 0; i < 512; ++i)
        assert(std::abs(left[i] - average[i]) < 1.e-7 && left[i] == right[i]);
    }
    stereo.ResetStopped(rate, 512);
    left.fill(0.);
    right.fill(0.);
    left[0] = 1.;
    right[0] = -.5;
    stereo.Process(left.data(), 512, false, 0, 0, false, 0, 0, 1, {}, {}, {}, right.data());
    for (int i = 0; i < 512; ++i)
    {
      assert(left[i] == (i == stereo.Latency() ? 1. : 0.));
      assert(right[i] == (i == stereo.Latency() ? -.5 : 0.));
    }
  }
  // Independent IR histories must neither collapse nor leak into the silent side.
  for (double rate : {44100., 48000., 96000.})
  {
    auto prepared = std::make_unique<NAMStage::Prepared>();
    prepared->effect = SlotEffect::CabinetIR;
    dsp::ImpulseResponse::IRData irData;
    irData.mRawAudio = {1.f, .5f, .25f};
    irData.mRawAudioSampleRate = 48000.;
    prepared->ir = std::make_unique<dsp::ImpulseResponse>(irData, rate);
    prepared->Reset(rate, 512);
    NAMStage stage;
    stage.Publish(std::move(prepared));
    stage.ResetStopped(rate, 512);
    double energy = 0.;
    for (int block = 0; block < 4; ++block)
    {
      const int n = std::array<int, 4>{1, 127, 512, 64}[block];
      left.fill(0.);
      right.fill(0.);
      if (block == 0)
        right[0] = 1.;
      allocations = deletions = 0;
      tracking = true;
      Eigen::internal::set_is_malloc_allowed(false);
      stage.Process(left.data(), n, true, 0, 0, false, 0, 0, 1, {}, {}, {}, right.data());
      Eigen::internal::set_is_malloc_allowed(true);
      tracking = false;
      assert(allocations == 0 && deletions == 0);
      for (int i = 0; i < n; ++i)
      {
        assert(left[i] == 0.);
        energy += right[i] * right[i];
      }
    }
    assert(energy > .001);
  }
  // Modulation sync follows the beat, including an Auto Tremolo Character
  // setting that normally accelerates with the envelope when unsynced.
  for (double bpm : {60., 120., 300.})
  {
    constexpr double rate = 48000.;
    SlotConfig cfg;
    cfg.effect = SlotEffect::Tremolo;
    NAMStage stage;
    stage.Publish(PrepareSlot(cfg, rate, 512).dsp);
    stage.ResetStopped(rate, 512);
    UtilityControls c;
    c.sync = true;
    c.tempo = bpm;
    c.division = 4;
    c.modulation = {.31, 100., 0., 100., 100.};
    double phase = 0.;
    for (int block = 0; block < 20; ++block)
    {
      const int n = std::array<int, 4>{1, 127, 512, 64}[block % 4];
      left.fill(.2);
      stage.Process(left.data(), n, true, 0, 0, false, 0, 0, 1, {}, {}, c);
      for (int i = 0; i < n; ++i)
      {
        phase += (bpm / 60.) / rate;
        if (phase >= 1.)
          phase -= 1.;
        const double expected =
          .2 * (1. - .5 * (1. + std::tanh(6. * std::sin(6.283185307179586 * phase)) / std::tanh(6.)));
        assert(std::abs(left[i] - expected) < 1.e-10);
      }
    }
  }
  // Actual first repeat: all divisions at changing tempos, variable block sizes.
  for (double bpm : {30., 60., 90., 120., 300., 0.})
    for (int division : {0, 1, 2, 3, 4, 5, 6, 7, 8})
    {
      constexpr double rate = 12000.;
      SlotConfig cfg;
      cfg.effect = SlotEffect::DigitalDelay;
      NAMStage stage;
      stage.Publish(PrepareSlot(cfg, rate, 512).dsp);
      stage.ResetStopped(rate, 512);
      UtilityControls c;
      c.sync = true;
      c.tempo = bpm;
      c.division = division;
      c.delay = {37., 0., 5000., 0., 100.};
      const int expected = static_cast<int>(std::floor(grid_tempo::Seconds(bpm, division) * rate));
      int first = -1, block = 0;
      for (int pos = 0; pos < expected + 512;)
      {
        const int n = std::min(std::array<int, 4>{1, 127, 512, 64}[block++ % 4], expected + 512 - pos);
        left.fill(0.);
        if (pos == 0)
          left[0] = 1.;
        allocations = deletions = 0;
        tracking = true;
        stage.Process(left.data(), n, true, 0, 0, false, 0, 0, 1, {}, {}, c);
        tracking = false;
        assert(allocations == 0 && deletions == 0);
        for (int i = 0; i < n; ++i)
          if (first < 0 && std::abs(left[i]) > 1.e-8)
            first = pos + i;
        pos += n;
      }
      if (std::abs(first - expected) > 1)
        std::cerr << "tempo=" << bpm << " division=" << division << " expected=" << expected << " first=" << first
                  << "\n";
      assert(std::abs(first - expected) <= 1);
    }
  std::cout << "Tuner accuracy/worker, stereo Utility/A2, synced repeats and callback allocations passed\n";
}
