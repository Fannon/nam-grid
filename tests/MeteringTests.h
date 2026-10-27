#pragma once
#include "SlotOperations.h"

inline void TestMeteringTransparency()
{
  for (const auto effect : {SlotEffect::ToneEQ, SlotEffect::SmoothComp, SlotEffect::NoisePrint,
                            SlotEffect::LoFiTape, SlotEffect::OutboardChannel, SlotEffect::FrequencyShifter})
  {
    SlotConfig config;
    config.effect = effect;
    NAMStage reference, hidden;
    reference.Publish(PrepareSlot(config, 48000., 128).dsp);
    hidden.Publish(PrepareSlot(config, 48000., 128).dsp);
    reference.ResetStopped(48000., 128);
    hidden.ResetStopped(48000., 128);
    SignalCapture capture;
    hidden.AttachCapture(&capture);
    hidden.EnableMetering(false);
    for (int block = 0; block < 80; ++block)
    {
      const int frames = block % 2 ? 37 : 128;
      std::array<double, 128> l{}, r{};
      for (int i = 0; i < frames; ++i)
      {
        l[i] = .3 * std::sin((block * 128 + i) * .071);
        r[i] = .2 * std::cos((block * 128 + i) * .053);
      }
      auto hiddenL = l, hiddenR = r;
      UtilityControls controls;
      controls.shift[0] = block < 40 ? 110. : -220.;
      allocations = deletions = 0;
      tracking = true;
      reference.Process(l.data(), frames, true, 0., 0., {}, {}, controls, r.data());
      hidden.Process(hiddenL.data(), frames, true, 0., 0., {}, {}, controls, hiddenR.data());
      tracking = false;
      assert(allocations == 0 && deletions == 0);
      assert(l == hiddenL && r == hiddenR);
      assert(hidden.TakeMeter().frames == 0 && capture.count == 0);
    }
    hidden.EnableMetering(true);
    std::array<double, 37> l{}, r{};
    hidden.Process(l.data(), 37, true, 0., 0., {}, {}, {}, r.data());
    assert(hidden.TakeMeter().frames == 37);
    if (ShowsSignalSpectrum(effect)) assert(capture.count == 37);
    hidden.EnableMetering(false);
    assert(hidden.TakeMeter().frames == 0);
  }
  std::cout << "Editor metering: exact audio, no hidden capture/allocation, clean restart passed\n";
}
