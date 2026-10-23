#pragma once
#include "SlotOperations.h"

inline UtilityControls CandidateControls(SlotEffect effect)
{
  const auto v = SlotDefaultValues(effect);
  UtilityControls c;
  for (int i = 0; i < 5; ++i) { c.delay[i] = v[17+uDelayTime+i]; c.reverb[i] = v[17+uReverbDecay+i]; }
  c.delayMotion = v[17+uDelayMotion]; c.delayOutput = v[17+uDelayOutput];
  c.reverbMotion = v[17+uReverbMotion]; c.reverbHighPass = v[17+uReverbHighPass];
  c.tapeHeads = static_cast<int>(v[17+uTapeHeads]); c.delayDiffusion = v[17+uDelayDiffusion];
  for (int i = 0; i < 7; ++i)
  { c.airPlate[i] = v[17+uAirPlateInput+i]; c.airHall[i] = v[17+uAirHallSustain+i]; c.space[i] = v[17+uSpaceReplace+i]; }
  return c;
}

void TestReverbDelayCandidates()
{
  constexpr std::array effects{SlotEffect::RoomReverb, SlotEffect::PlateReverb, SlotEffect::CloudReverb,
    SlotEffect::HallReverb, SlotEffect::MultiHeadDelay, SlotEffect::DiffusedDelay,
    SlotEffect::AirPlate, SlotEffect::AirHall, SlotEffect::GalacticSpace};
  for (double rate : {44100., 48000., 96000., 192000.})
    for (auto effect : effects)
    {
      const auto render = [&](bool tiny, bool dry, bool automate) {
        SlotConfig config; config.effect = effect;
        NAMStage stage; stage.Publish(PrepareSlot(config, rate, 512).dsp); stage.ResetStopped(rate, 512);
        auto c = CandidateControls(effect);
        const int count = static_cast<int>(rate * .45);
        std::array<std::vector<double>, 2> audio{std::vector<double>(count), std::vector<double>(count)};
        for (int i = 0; i < count; ++i)
        { audio[0][i] = .1 * std::sin(i * .071); audio[1][i] = .08 * std::cos(i * .053); }
        if (dry)
        { c.delay[4] = c.reverb[4] = c.airPlate[4] = c.airHall[5] = c.space[5] = 0.; }
        for (int pos = 0, block = 0; pos < count;)
        {
          const int segment = pos / 2048;
          if (automate)
          {
            const bool high = segment % 2;
            const auto extreme = [&](int p) { return high ? kEffectParameters[p].maximum : kEffectParameters[p].minimum; };
            for (int i = 0; i < 5; ++i)
            { c.delay[i] = extreme(uDelayTime+i); c.reverb[i] = extreme(uReverbDecay+i); }
            c.delayMotion = high ? 100. : 0.; c.reverbMotion = high ? 100. : 0.;
            c.delayOutput = high ? 6. : -18.; c.delayDiffusion = high ? 100. : 0.;
            c.tapeHeads = segment % static_cast<int>(effect_modes::tapeHeadNames.size());
            c.reverbHighPass = high ? 500. : 0.;
            for (int i = 0; i < 7; ++i)
            { c.airPlate[i] = extreme(uAirPlateInput+i); c.airHall[i] = extreme(uAirHallSustain+i); c.space[i] = extreme(uSpaceReplace+i); }
          }
          const int boundary = std::min(count, (segment + 1) * 2048);
          const int n = std::min(boundary-pos, tiny ? std::array<int, 5>{1, 3, 64, 127, 256}[block++%5] : 512);
          allocations = deletions = 0; tracking = true; Eigen::internal::set_is_malloc_allowed(false);
          stage.Process(audio[0].data()+pos, n, true, 0., 0., {}, {}, c, audio[1].data()+pos);
          Eigen::internal::set_is_malloc_allowed(true); tracking = false;
          assert(allocations == 0 && deletions == 0); pos += n;
        }
        return audio;
      };
      for (bool automate : {false, true})
      {
        const auto a = render(false, false, automate), b = render(true, false, automate);
        for (int ch = 0; ch < 2; ++ch)
          for (int i = 0; i < a[ch].size(); ++i)
          {
            if (!std::isfinite(b[ch][i]) || std::abs(b[ch][i]) >= 16.)
              std::cerr << "Unbounded candidate " << SlotEffectName(effect) << " rate=" << rate << " sample=" << i << " value=" << b[ch][i] << '\n';
            assert(std::isfinite(b[ch][i]) && std::abs(b[ch][i]) < 16.);
            assert(std::abs(a[ch][i]-b[ch][i]) < 1.e-11);
          }
      }
      const auto dry = render(true, true, false);
      for (int i = 0; i < dry[0].size(); ++i)
      {
        assert(std::abs(dry[0][i]-.1*std::sin(i*.071)) < 1.e-12);
        assert(std::abs(dry[1][i]-.08*std::cos(i*.053)) < 1.e-12);
      }
    }

  // Hold the longest native feedback settings long enough to exercise their
  // tanks, including low-resolution processing. Short automation bursts alone
  // cannot detect a growing tail or a delayed non-finite sample.
  for (double rate : {48000., 96000.})
    for (auto effect : {SlotEffect::AirPlate, SlotEffect::AirHall, SlotEffect::GalacticSpace})
      for (bool lowResolution : {false, true})
      {
        SlotAirReverb reverb; reverb.Reset(rate, effect);
        auto c = CandidateControls(effect);
        c.airPlate[1] = 100.; c.airHall[0] = 95.; c.space[0] = 5.; c.space[3] = 100.;
        c.airPlate[2] = c.airHall[1] = c.space[4] = lowResolution ? 25. : 100.;
        c.airPlate[4] = c.airHall[5] = c.space[5] = 100.;
        std::array<double, 512> left{}, right{};
        double energy = 0.;
        const int count = static_cast<int>(rate * 16.);
        for (int pos = 0; pos < count; pos += 512)
        {
          const int n = std::min(512, count-pos);
          for (int i = 0; i < n; ++i)
          {
            left[i] = pos+i < rate*2. ? .25*std::sin((pos+i)*.071) : 0.;
            right[i] = pos+i < rate*2. ? .2*std::cos((pos+i)*.053) : 0.;
          }
          allocations = deletions = 0; tracking = true; Eigen::internal::set_is_malloc_allowed(false);
          reverb.Process(left.data(), right.data(), n, c);
          Eigen::internal::set_is_malloc_allowed(true); tracking = false;
          assert(allocations == 0 && deletions == 0);
          for (int i = 0; i < n; ++i)
          {
            assert(std::isfinite(left[i]) && std::isfinite(right[i]));
            assert(std::abs(left[i]) < 8. && std::abs(right[i]) < 8.);
            energy += left[i]*left[i]+right[i]*right[i];
          }
        }
        assert(energy > .001);
      }

  // Multi-head timing is checked against actual audio, for every head pattern.
  constexpr double rate = 48000.;
  for (int pattern = 0; pattern < effect_modes::tapeHeadNames.size(); ++pattern)
  {
    SlotMultiHeadDelay delay; delay.Reset(rate);
    UtilityControls c; c.delay = {40., 0., 12000., 0., 100.}; c.delayMotion = 0.; c.tapeHeads = pattern;
    std::vector<double> l(3000), r(3000); l[0] = 1.;
    delay.Process(l.data(), r.data(), static_cast<int>(l.size()), c);
    int first = -1; for (int i = 0; i < l.size(); ++i) if (std::abs(l[i]) > 1.e-9) { first = i; break; }
    const auto preview = effect_preview::Delay(SlotEffect::MultiHeadDelay, c, rate);
    assert(preview.count > 0 && std::abs(first - preview.taps[0].seconds*rate) <= 1.);
    for (double sample : r) assert(sample == 0.);
  }
  // Diffusion adds a stereo reflection cluster; its zero endpoint is a plain echo.
  std::array<std::vector<double>, 2> outputs;
  for (int variant = 0; variant < 2; ++variant)
  {
    SlotDiffusedDelay delay; delay.Reset(rate);
    UtilityControls c; c.delay = {20., 0., 12000., 0., 100.}; c.delayMotion = 0.; c.delayDiffusion = variant * 100.;
    auto& left = outputs[variant]; left.assign(12000, 0.); std::vector<double> right(left.size()); left[0] = 1.;
    delay.Process(left.data(), right.data(), static_cast<int>(left.size()), c);
    double rightEnergy = 0.; for (double x : right) rightEnergy += x*x;
    assert(variant ? rightEnergy > .001 : rightEnergy == 0.);
  }
  double lateClear = 0., lateBlur = 0.;
  for (int i = 2000; i < 12000; ++i)
  { lateClear += outputs[0][i]*outputs[0][i]; lateBlur += outputs[1][i]*outputs[1][i]; }
  assert(lateBlur > .001 && lateClear < 1.e-15);
  std::cout << "Reverb/delay candidates: 44.1-192 kHz, full-range automation, partition invariance, stereo dry, no allocations, head timing and diffusion passed\n";
}
