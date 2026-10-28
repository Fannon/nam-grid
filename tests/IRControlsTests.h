#pragma once
#include "SlotOperations.h"

void TestIRControls()
{
  for (double rate : {44100., 48000., 96000.})
  {
    // Delta IR: independent stereo dry paths and wet-only trims,
    // blending and exact fractional pre-delay have a closed-form reference.
    for (bool stereoIR : {false, true})
    for (bool monoInput : {false, true})
    for (double mix : {0., 25., 100.})
      for (double predelay : {0., 37.5})
      {
        auto prepared = std::make_unique<NAMStage::Prepared>();
        prepared->effect = SlotEffect::CabinetIR;
        AdaptiveIR::IRData data{{1.f}, rate};
        prepared->ir = std::make_unique<AdaptiveIR>(data, rate, 512);
        if (stereoIR)
          prepared->rightIRData = std::make_unique<AdaptiveIR::IRData>(AdaptiveIR::IRData{{-.5f}, rate});
        NAMStage stage;
        stage.Publish(std::move(prepared));
        stage.ResetStopped(rate, 512);
        UtilityControls controls;
        controls.irInput = 6.; controls.irMix = mix;
        controls.irPredelay = predelay * 1000. / rate;
        EQControls eq; eq.irLevel = -3.;
        const double gain = std::pow(10., 3. * .05) * 48000. / rate;
        std::array<double, 512> l{}, r{};
        int position = 0;
        for (int block = 0; block < 20; ++block)
        {
          const int count = std::array<int, 5>{1, 3, 127, 512, 64}[block % 5];
          l.fill(0.); r.fill(0.);
          if (!position) { l[0] = .25; r[0] = -.5; }
          allocations = deletions = 0; tracking = true;
          Eigen::internal::set_is_malloc_allowed(false);
          stage.Process(l.data(), count, true, 30., -30., {}, eq, controls, monoInput ? nullptr : r.data());
          Eigen::internal::set_is_malloc_allowed(true);
          tracking = false;
          assert(stage.Latency() == 0 && allocations == 0 && deletions == 0);
          for (int i = 0; i < count; ++i)
          {
            const int n = position + i;
            const double wet = predelay == 0. ? (n == 0 ? 1. : 0.) : (n == 37 || n == 38 ? .5 : 0.);
            const double dry = n == 0 ? 1. - mix * .01 : 0.;
            const double wetGain = wet * gain * mix * .01;
            const double leftExpected = dry + wetGain * (stereoIR && monoInput ? .25 : 1.);
            const double rightExpected = dry + wetGain * (stereoIR ? -.5 : 1.);
            assert(std::abs(l[i] - .25 * leftExpected) < 2.e-7);
            if (!monoInput) assert(std::abs(r[i] + .5 * rightExpected) < 2.e-7);
          }
          position += count;
        }
        // Automation remains allocation-free, and bypass restores both dry channels.
        controls.irInput = -36.; controls.irMix = 100. - mix;
        controls.irPredelay = 500.; controls.irLowCut = 2000.; controls.irHighCut = 200.;
        for (int b = 0; b < 4; ++b)
        {
          l.fill(.1); r.fill(-.2);
          allocations = deletions = 0; tracking = true;
          stage.Process(l.data(), 512, false, 0., 0., {}, eq, controls, r.data());
          tracking = false;
          assert(allocations == 0 && deletions == 0);
          if (b > 0)
            for (int i = 0; i < 512; ++i) assert(l[i] == .1 && r[i] == -.2);
        }
      }
    const auto magnitude = [&](double hz, double low, double high) {
      SlotIR processor;
      processor.Reset(rate);
      UtilityControls c; c.irLowCut = low; c.irHighCut = high;
      double energy = 0.;
      for (int i = 0; i < static_cast<int>(rate); ++i)
      {
        double x = std::sin(6.283185307179586 * hz * i / rate);
        processor.Process(&x, 1, c);
        if (i >= rate / 2) energy += x * x;
      }
      return std::sqrt(energy / (static_cast<int>(rate) - static_cast<int>(rate / 2)) * 2.);
    };
    assert(magnitude(20., 200., 20000.) < .011);
    assert(std::abs(magnitude(200., 200., 20000.) - std::sqrt(.5)) < .002);
    assert(magnitude(10000., 0., 2000.) < .05);
    assert(std::abs(magnitude(2000., 0., 2000.) - std::sqrt(.5)) < .002);
  }
  // Different channel taps survive repeated preparation in both length modes.
  for (auto mode : {AdaptiveIR::Mode::Cabinet, AdaptiveIR::Mode::Long})
    for (bool mono : {false, true})
    {
      auto p = std::make_unique<NAMStage::Prepared>(); p->effect = SlotEffect::CabinetIR;
      AdaptiveIR::IRData l{{.5f}, 48000.}, r{{}, 48000.};
      const int offset = mode == AdaptiveIR::Mode::Long ? 19001 : 1001;
      r.mRawAudio.assign(offset + 1, 0.f); r.mRawAudio.back() = -.25f;
      p->ir = std::make_unique<AdaptiveIR>(l, 48000., 512, mode);
      p->rightIRData = std::make_unique<AdaptiveIR::IRData>(r);
      NAMStage stage; stage.Publish(std::move(p));
      for (int reset = 0; reset < 2; ++reset)
      {
        stage.ResetStopped(48000., 512);
        std::array<double, 512> left{}, right{};
        UtilityControls controls; EQControls eq; eq.irLevel = 0.;
        for (int start = 0; start < offset + 1024; start += 512)
        {
          left.fill(0.); right.fill(0.);
          if (!start) { left[0] = 1.; right[0] = 2.; }
          allocations = deletions = 0; tracking = true;
          stage.Process(left.data(), 512, true, 0., 0., {}, eq, controls, mono ? nullptr : right.data());
          tracking = false;
          assert(allocations == 0 && deletions == 0 && stage.Latency() == 0);
          for (int i = 0; i < 512; ++i)
          {
            const int n = start + i;
            const double expectedLeft = mono ? (n == 0 ? .25 : n == offset ? -.125 : 0.) : (n == 0 ? .5 : 0.);
            assert(std::abs(left[i] - expectedLeft) < 1.e-6);
            if (!mono) assert(std::abs(right[i] - (n == offset ? -.5 : 0.)) < 1.e-6);
          }
        }
      }
    }
  // Pre-delay and long FIR histories clear on reset and drain completely while bypassed.
  auto prepared = std::make_unique<NAMStage::Prepared>();
  prepared->effect = SlotEffect::CabinetIR;
  AdaptiveIR::IRData data{{}, 48000.};
  data.mRawAudio.assign(48001, 0.f); data.mRawAudio.back() = 1.f;
  prepared->ir = std::make_unique<AdaptiveIR>(data, 48000., 512, AdaptiveIR::Mode::Long);
  NAMStage stage; stage.Publish(std::move(prepared)); stage.ResetStopped(48000., 512);
  UtilityControls c; c.irPredelay = 500.;
  EQControls eq; eq.irLevel = 0.;
  std::array<double, 512> samples{};
  samples[0] = 1.; stage.Process(samples.data(), 512, true, 0., 0., {}, eq, c);
  for (int b = 0; b < 250; ++b)
  {
    samples.fill(0.); stage.Process(samples.data(), 512, false, 0., 0., {}, eq, c);
  }
  for (int b = 0; b < 160; ++b)
  {
    samples.fill(0.); stage.Process(samples.data(), 512, true, 0., 0., {}, eq, c);
    for (double x : samples) assert(std::abs(x) < 1.e-7);
  }
  samples.fill(0.); samples[0] = 1.; stage.Process(samples.data(), 512, true, 0., 0., {}, eq, c);
  stage.ResetStopped(48000., 512);
  for (int b = 0; b < 160; ++b)
  {
    samples.fill(0.); stage.Process(samples.data(), 512, true, 0., 0., {}, eq, c);
    for (double x : samples) assert(x == 0.);
  }
  std::cout << "IR wet controls, stereo, fractional pre-delay, cuts, bypass/drain/reset and realtime safety passed\n";
}
