#pragma once
#include "FixedNAMGrid.h"
#include "LevelReadout.h"
#include "LevelAnalysis.h"

void TestDSPReviewFixes()
{
  LevelReadout reading;
  reading.Receive(.1, .5); reading.Tick(1., false);
  assert(reading.average == .1 && reading.peak == .5);
  reading.Receive(.1, .2); reading.Tick(.5, false);
  assert(reading.peak == .5 && reading.hold == 0.);
  reading.Tick(1., false);
  assert(std::abs(reading.peak - .5 * std::pow(10., -12. / 20.)) < 1.e-12);
  reading.Tick(1., false); assert(reading.peak == reading.average);
  reading.Receive(.05, .8); assert(reading.hold == 1.5 && reading.peak == .8);
  reading.Receive(std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::infinity());
  reading.Tick(10., true); assert(reading.average == 0. && std::isfinite(reading.peak));
  for (double rate : {44100., 48000., 96000., 192000.})
  {
    LevelAnalysis average;
    average.Reset(rate);
    const int duration = static_cast<int>(rate * 3.);
    int packets = 0;
    for (int offset = 0; offset < duration;)
    {
      const int frames = std::min(duration - offset, std::array<int, 4>{1, 3, 64, 127}[offset % 4]);
      for (int i = 0; i < frames; ++i)
      {
        const double x = .4 * std::sin(2. * grid_fx::pi * 1000. * (offset + i) / rate);
        if (average.Push(x)) ++packets;
      }
      offset += frames;
    }
    assert(packets == duration / static_cast<int>(std::lround(rate / 30.)));
    assert(std::abs(20. * std::log10(average.rms) - 20. * std::log10(.4 / std::sqrt(2.))) < .03);
    assert(std::abs(average.peak - .4) < .001);
    average.Reset(rate);
    for (int i = 0; i < duration; ++i) average.Push(.25);
    assert(std::abs(average.rms - .25) < .0001 && average.peak == .25);
    average.Push(std::numeric_limits<double>::infinity());
    average.Push(std::numeric_limits<double>::quiet_NaN());
    assert(std::isfinite(average.rms) && std::isfinite(average.peak));
    // A long FIR must not replay pre-bypass audio when re-enabled into silence.
    // Use both channels and small, varying blocks so the suspension boundary
    // cannot hide behind a large bypass-fade block.
    NAMStage cabinet;
    auto prepared = std::make_unique<NAMStage::Prepared>();
    prepared->effect = SlotEffect::CabinetIR;
    dsp::ImpulseResponse::IRData data;
    data.mRawAudio.assign(8192, 0.f);
    data.mRawAudio.back() = 1.f;
    data.mRawAudioSampleRate = rate;
    prepared->ir = std::make_unique<dsp::ImpulseResponse>(data, rate);
    cabinet.Publish(std::move(prepared)); cabinet.ResetStopped(rate, 127);
    std::array<double, 127> left{}, right{};
    double activePeak = 0.;
    for (int phase = 0; phase < 3; ++phase)
    {
      const int duration = 8192 + static_cast<int>(rate * .005) + 1024;
      for (int pos = 0, block = 0; pos < duration;)
      {
        const int n = std::min(duration - pos, std::array<int, 4>{1, 3, 64, 127}[block++ % 4]);
        left.fill(phase == 0 ? .25 : 0.); right.fill(phase == 0 ? -.125 : 0.);
        allocations = deletions = 0; tracking = true; Eigen::internal::set_is_malloc_allowed(false);
        cabinet.Process(left.data(), n, phase != 1, 0., 0., false, 0., 0, 1., {}, {}, {}, right.data());
        Eigen::internal::set_is_malloc_allowed(true); tracking = false;
        assert(allocations == 0 && deletions == 0);
        for (int i = 0; i < n; ++i)
        {
          if (phase == 0) activePeak = std::max(activePeak, std::abs(left[i]));
          if (phase == 2) assert(left[i] == 0. && right[i] == 0.);
        }
        pos += n;
      }
    }
    assert(activePeak > .001);
    // The maximum swell rises over two seconds rather than clamping at 1.2 s.
    SlotAutoSwell slow; slow.Reset(rate);
    UtilityControls slowControls; slowControls.rise = 2000.; slowControls.threshold = -72.;
    std::vector<double> slowAudio(static_cast<int>(rate * 2.1), .2);
    slow.Process(slowAudio.data(), static_cast<int>(slowAudio.size()), slowControls);
    assert(std::abs(slowAudio[static_cast<int>(rate)] / .2 - .5) < .002);
    assert(std::abs(slowAudio.back() - .2) < 1.e-10);
    std::array<double, 2> signal{.25, -.125};
    cabinet.Process(signal.data(), 2, false, 0., 0., false, 0., 0, 1.);
    const auto bypassed = cabinet.TakeMeter();
    assert(bypassed.input == 0.f && bypassed.output == 0.f && bypassed.reduction == 0.f);
    NAMStage empty; empty.ResetStopped(rate, 2);
    empty.Process(signal.data(), 2, true, 0., 0., false, 0., 0, 1.);
    const auto unloaded = empty.TakeMeter();
    assert(unloaded.input == 0.f && unloaded.output == 0.f && unloaded.reduction == 0.f);

    // Check the actual reserved read range, rather than just finite reverb output.
    SlotReverb reverb;
    reverb.Reset(rate);
    for (bool rightChannel : {false, true})
    {
      for (double seconds : SlotReverb::kDiffusionSeconds)
        assert(reverb.DiffuserMaxDelay() >= rate * seconds * SlotReverb::kMaximumSize * (rightChannel ? SlotReverb::kRightScale : 1.));
      for (auto effect : {SlotEffect::PlateReverb, SlotEffect::CloudReverb, SlotEffect::HallReverb})
      {
        UtilityControls c; c.reverb[1] = 300.; c.reverb[4] = 100.;
        left.fill(0.); left[0] = 1.;
        reverb.Process(left.data(), 127, effect, c, rightChannel);
        for (double sample : left) assert(std::isfinite(sample));
      }
    }

    // Rise automation should slew, preserving the envelope and its sample clock.
    SlotAutoSwell swell, partitioned;
    swell.Reset(rate); partitioned.Reset(rate);
    UtilityControls c; c.threshold = -72.; c.rise = 1000.;
    std::vector<double> a(static_cast<int>(rate * .025), .2), b = a;
    swell.Process(a.data(), static_cast<int>(a.size()), c);
    partitioned.Process(b.data(), static_cast<int>(b.size()), c);
    const double previous = a.back() / .2;
    c.rise = 10.;
    a.assign(static_cast<int>(rate * .1), .2); b = a;
    swell.Process(a.data(), static_cast<int>(a.size()), c);
    for (int pos = 0, block = 0; pos < b.size();)
    {
      const int n = std::min(static_cast<int>(b.size()) - pos, std::array<int, 4>{1, 3, 64, 127}[block++ % 4]);
      allocations = deletions = 0; tracking = true;
      partitioned.Process(b.data() + pos, n, c);
      tracking = false;
      assert(allocations == 0 && deletions == 0);
      pos += n;
    }
    assert(a.front() / .2 - previous > 0. && a.front() / .2 - previous < 1. / (.05 * rate));
    for (int i = 0; i < a.size(); ++i) assert(a[i] == b[i] && a[i] >= 0. && a[i] <= .2);
    // Threshold and Release changes also keep the clock when partitioned.
    for (int change = 0; change < 3; ++change)
    {
      c.threshold = change == 0 ? -12. : -72.; c.release = change == 1 ? 20. : 800.;
      a.assign(static_cast<int>(rate * .1), change == 1 ? 0. : .05); b = a;
      swell.Process(a.data(), static_cast<int>(a.size()), c);
      for (int i = 0; i < b.size(); ++i) partitioned.Process(b.data() + i, 1, c);
      for (int i = 0; i < a.size(); ++i) assert(a[i] == b[i] && std::isfinite(a[i]));
    }
  }

  // The same crossover automation has the same response in wall-clock time.
  // Select only lane A so complementary reconstruction cannot mask the smoothing.
  double reference = 0.;
  for (double rate : {44100., 48000., 96000., 192000.})
  {
    FixedNAMGrid grid; grid.ResetStopped(rate, 512);
    std::array<FixedNAMGrid::Controls, FixedNAMGrid::kSlots> controls{};
    grid_performance::Routing routing; routing.crossover = true; routing.frequency = 250.;
    double zero = 0.; grid.Process(&zero, 1, controls, 0., nullptr, routing);
    routing.frequency = 5000.;
    std::array<double, 512> step{}; step.fill(1.);
    const int count = static_cast<int>(std::lround(rate * .0005));
    grid.Process(step.data(), count, controls, 0., nullptr, routing);
    if (reference == 0.) reference = step[count - 1];
    assert(std::abs(step[count - 1] - reference) < .025);
  }

  // First use without Reset is deterministic; Reset clears nonlinear histories.
  AirwindowsTape defaultTape, resetTape;
  resetTape.Reset(48000.);
  std::array<double, 256> dry{}, l{}, r{}, expectedL{}, expectedR{};
  for (int i = 0; i < dry.size(); ++i) dry[i] = .3 * std::sin(i * .13);
  double* inputs[]{dry.data(), dry.data()}, *outputs[]{l.data(), r.data()}, *expected[]{expectedL.data(), expectedR.data()};
  defaultTape.Process(inputs, outputs, 256); resetTape.Process(inputs, expected, 256);
  for (int i = 0; i < dry.size(); ++i)
    assert(l[i] == expectedL[i] && r[i] == expectedR[i] && std::isfinite(l[i]));
  defaultTape.Reset(48000.); defaultTape.Process(inputs, outputs, 256);
  for (int i = 0; i < dry.size(); ++i) assert(l[i] == expectedL[i] && r[i] == expectedR[i]);
}

void TestCaptureSeams()
{
  for (double rate : {44100., 48000., 96000., 192000.})
  {
    const int length = static_cast<int>(.02 * rate);
    {
      // 1/32 at 120 BPM repeats a 62.5 ms window, including at fractional rates.
      assert(grid_tempo::Seconds(120., 0) == .0625);
      const int window = static_cast<int>(.0625 * rate);
      grid_roadmap::Capture synced; synced.Reset(rate);
      UtilityControls c; c.sync = true; c.tempo = 120.; c.division = 0;
      c.stutter = {1000., 100., 1., 100.};
      std::vector<double> x(window * 4);
      for (int i = 0; i < x.size(); ++i) x[i] = .2 * std::sin(i * .017);
      for (int pos = 0, block = 0; pos < x.size();)
      {
        const int n = std::min(static_cast<int>(x.size()) - pos, std::array<int, 4>{1, 3, 64, 127}[block++ % 4]);
        allocations = deletions = 0; tracking = true;
        synced.Process(x.data() + pos, nullptr, n, SlotEffect::Stutter, c);
        tracking = false; assert(allocations == 0 && deletions == 0); pos += n;
      }
      for (int i = window * 2; i < window * 3; ++i) assert(std::abs(x[i] - x[i + window]) < 1.e-9);
    }
    for (auto effect : {SlotEffect::Stutter, SlotEffect::ReverseDelay})
      for (double fadeMS : {.1, 20.})
      {
        grid_roadmap::Capture capture; capture.Reset(rate);
        UtilityControls c; c.stutter = {20., 100., fadeMS, 100.}; c.delay = {20., 0., 12000., 0., 100.};
        std::vector<double> x(length * 8);
        for (int i = 0; i < x.size(); ++i) x[i] = .2 * std::sin(i * .017);
        capture.Process(x.data(), nullptr, static_cast<int>(x.size()), effect, c);
        if (effect == SlotEffect::Stutter)
        {
          for (int i = length * 2; i < length * 3; ++i) assert(std::abs(x[i] - x[i + length]) < 1.e-9);
          const double fade = std::min(length / 4., rate * fadeMS * .001);
          for (int seam : {2 * length - 1, 3 * length - 1, 4 * length - 1})
            assert(std::abs(x[seam] - x[seam - 1]) < .4 / std::max(1., fade) + .004);
        }
        else
          for (int seam = 2 * length - 1; seam < x.size(); seam += length)
            assert(std::abs(x[seam] - x[seam - 1]) < 1.e-14); // wet sample matches; dry/wet arithmetic can round
      }

    grid_roadmap::Capture held; held.Reset(rate, 2.1);
    UtilityControls c; c.freeze = {1., 500., 20., 16000., 100.};
    const int window = static_cast<int>(rate * .5), overlap = static_cast<int>(rate * .02);
    std::vector<double> x(window * 6, 0.);
    for (int i = 0; i < window; ++i) x[i] = .2 * std::sin(i * .017);
    held.Process(x.data(), nullptr, static_cast<int>(x.size()), SlotEffect::Freeze, c);
    double wrongPeriodError = 0.;
    for (int i = window * 3; i < window * 4; ++i)
    {
      assert(std::abs(x[i] - x[i + window - overlap]) < 1.e-9);
      wrongPeriodError = std::max(wrongPeriodError, std::abs(x[i] - x[i + window]));
    }
    assert(wrongPeriodError > .01);

    // Window, sync and Hold changes must remain finite and partition-independent.
    for (auto effect : {SlotEffect::Stutter, SlotEffect::ReverseDelay, SlotEffect::Freeze})
    {
      grid_roadmap::Capture whole, split; whole.Reset(rate); split.Reset(rate);
      for (int change = 0; change < 6; ++change)
      {
        c = {}; c.stutter = {change < 3 ? 10. : 1000., 100., 20., 100.};
        c.delay = {change < 3 ? 20. : 1500., 0., 12000., 0., 100.};
        c.freeze = {change % 2 ? 1. : 0., 50., 20., 16000., 100.};
        c.sync = change == 1 || change == 2; c.tempo = change == 1 ? 30. : 300.; c.division = change == 1 ? 9 : 0;
        std::array<double, 4096> left{}, right{}, expectedLeft{}, expectedRight{};
        for (int i = 0; i < left.size(); ++i)
        {
          left[i] = expectedLeft[i] = .2 * std::sin((i + change * 4096) * .017);
          right[i] = expectedRight[i] = -.17 * std::cos((i + change * 4096) * .031);
        }
        whole.Process(expectedLeft.data(), expectedRight.data(), 4096, effect, c);
        for (int pos = 0, block = 0; pos < 4096;)
        {
          const int n = std::min(4096 - pos, std::array<int, 4>{1, 3, 64, 127}[block++ % 4]);
          allocations = deletions = 0; tracking = true;
          split.Process(left.data() + pos, right.data() + pos, n, effect, c);
          tracking = false;
          assert(allocations == 0 && deletions == 0);
          pos += n;
        }
        for (int i = 0; i < left.size(); ++i)
        {
          assert(std::isfinite(left[i]) && std::isfinite(right[i]) && std::abs(left[i]) < .5 && std::abs(right[i]) < .5);
          assert(left[i] == expectedLeft[i] && right[i] == expectedRight[i]);
        }
      }
    }
  }
  // Tempo sync overrides the free-time knob maximum: preserve eight-second captures.
  for (auto effect : {SlotEffect::ReverseDelay, SlotEffect::Stutter})
  {
    grid_roadmap::Capture capture; capture.Reset(48000.);
    UtilityControls c; c.sync = true; c.tempo = 30.; c.division = 9;
    c.delay[4] = c.stutter[3] = 100.; c.stutter[1] = 100.;
    std::vector<double> x(384000 + 1024, .1);
    capture.Process(x.data(), nullptr, static_cast<int>(x.size()), effect, c);
    for (int i = 0; i < 384000; ++i) assert(x[i] == 0.);
    assert(x.back() > .01);
  }
}
