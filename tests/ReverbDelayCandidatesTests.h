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
  // kGuitarHall2 right-channel FDN must advance c6ZAR (not double-step c6ZBR),
  // otherwise a6ZBR (length 121, odd) writes only odd indices and reads only zero
  // even indices, draining the cross-coupled L/R reverb tail by >5 dB/s.
  {
    auto hall = std::make_unique<airwindows_reverb::kGuitarHall2::Core>();
    hall->Reset(44100.);
    hall->Set({.9, .5, 1., 0., 0., 1.});
    uint32_t seed = 0x12345678u;
    double leftTail = 0., rightTail = 0.;
    for (int i = 0; i < 88200; ++i)
    {
      seed ^= seed << 13; seed ^= seed >> 17; seed ^= seed << 5;
      const double s = i < 4096 ? (static_cast<double>(seed) / 4294967296. - .5) * .5 : 0.;
      double l = s, r = s;
      hall->Tick(l, r);
      if (i >= 44100) { leftTail += l * l; rightTail += r * r; }
    }
    assert(leftTail > 12. && rightTail > 12. && std::abs(rightTail / leftTail - 1.) < .05);
  }
  // SlotReverb & SlotAirReverb low-cut (at 0 -> 0.02 Hz) and pre-delay (at 0 -> 0.002 ms)
  // must transition smoothly instead of hard-stepping at 0.01 Hz / 0.001 ms.
  {
    const auto maxDiffStep = [](const std::vector<double>& a, const std::vector<double>& b) {
      double best = 0.;
      for (size_t i = 1; i < a.size(); ++i)
        best = std::max(best, std::abs((a[i] - b[i]) - (a[i - 1] - b[i - 1])));
      return best;
    };
    // AirPlate low-cut crossing 0.01 Hz on DC input must not inject a full-scale step.
    auto airA = std::make_unique<SlotAirReverb>(), airB = std::make_unique<SlotAirReverb>();
    airA->Reset(rate, SlotEffect::AirPlate); airB->Reset(rate, SlotEffect::AirPlate);
    auto ac = CandidateControls(SlotEffect::AirPlate);
    ac.airPlate = {100., 20., 100., 0., 100., 0., 0.}; // lowCut=0, predelay=0, mix=100%
    std::vector<double> la(static_cast<int>(rate * .2), .3), ra = la, lb = la, rb = la;
    airA->Process(la.data(), ra.data(), static_cast<int>(la.size()), ac);
    airB->Process(lb.data(), rb.data(), static_cast<int>(lb.size()), ac);
    std::vector<double> la2(2048, .3), ra2 = la2, lb2 = la2, rb2 = la2;
    airA->Process(la2.data(), ra2.data(), static_cast<int>(la2.size()), ac);
    ac.airPlate[5] = .02; // just above old .01 Hz threshold
    airB->Process(lb2.data(), rb2.data(), static_cast<int>(lb2.size()), ac);
    assert(maxDiffStep(lb2, la2) < .002);

    // RoomReverb low-cut crossing 0.01 Hz on DC input must not inject a full-scale step.
    auto revA = std::make_unique<SlotReverb>(), revB = std::make_unique<SlotReverb>();
    revA->Reset(rate, SlotEffect::RoomReverb); revB->Reset(rate, SlotEffect::RoomReverb);
    auto rc = CandidateControls(SlotEffect::RoomReverb);
    rc.reverb = {1., 100., 12000., 0., 100.}; rc.reverbHighPass = 0.;
    std::vector<double> rla(static_cast<int>(rate * .2), .3), rra = rla, rlb = rla, rrb = rla;
    revA->Process(rla.data(), rra.data(), static_cast<int>(rla.size()), rc);
    revB->Process(rlb.data(), rrb.data(), static_cast<int>(rlb.size()), rc);
    std::vector<double> rla2(2048, .3), rra2 = rla2, rlb2 = rla2, rrb2 = rla2;
    revA->Process(rla2.data(), rra2.data(), static_cast<int>(rla2.size()), rc);
    rc.reverbHighPass = .02;
    revB->Process(rlb2.data(), rrb2.data(), static_cast<int>(rlb2.size()), rc);
    assert(maxDiffStep(rlb2, rla2) < .002);
  }
  // DiffusedDelay modulation must sweep continuously (not via MovingDelay's 40 ms crossfades,
  // which comb-filter a high tone into deep periodic amplitude dips).
  {
    SlotDiffusedDelay delay; delay.Reset(rate);
    UtilityControls c; c.delay = {50., 0., 18000., 0., 100.}; c.delayMotion = 100.; c.delayDiffusion = 0.;
    const int total = static_cast<int>(rate * 1.5);
    std::vector<double> l(total), r(total);
    for (int i = 0; i < total; ++i) l[i] = r[i] = .2 * std::sin(2. * grid_fx::pi * 6000. * i / rate);
    delay.Process(l.data(), r.data(), total, c);
    double minRms = 1e9, maxRms = 0.;
    const int step = static_cast<int>(.01 * rate);
    for (int pos = static_cast<int>(.2 * rate); pos + step <= total; pos += step)
    {
      double sum = 0.;
      for (int i = pos; i < pos + step; ++i) sum += l[i] * l[i];
      const double rms = std::sqrt(sum / step);
      minRms = std::min(minRms, rms); maxRms = std::max(maxRms, rms);
    }
    assert(maxRms > .05 && minRms / maxRms > .92);

    // With modulation off, large base Time jumps must crossfade taps rather than
    // sweeping the read head across the buffer into a pitch burst.
    delay.Reset(rate);
    c.delay = {50., 0., 18000., 0., 100.}; c.delayMotion = 0.; c.delayDiffusion = 0.;
    const int warm = static_cast<int>(.25 * rate), after = static_cast<int>(.3 * rate);
    std::vector<double> jl(warm + after), jr(warm + after);
    for (int i = 0; i < warm + after; ++i)
      jl[i] = jr[i] = .35 * std::sin(2. * grid_fx::pi * 440. * i / rate);
    delay.Process(jl.data(), jr.data(), warm, c);
    c.delay[0] = 2000.;
    for (int pos = warm; pos < warm + after; pos += 128)
    {
      const int n = std::min(128, warm + after - pos);
      delay.Process(jl.data() + pos, jr.data() + pos, n, c);
    }
    double maxJump = 0.;
    for (int i = warm + 1; i < warm + after; ++i)
      maxJump = std::max(maxJump, std::abs(jl[i] - jl[i - 1]));
    assert(maxJump < .05);
  }
  // Galactic3 must scale its bez clock, predelay, drift and lowpass consistently across
  // 44.1..192 kHz so wet level and tail decay stay matched at high sample rates.
  {
    auto core = std::make_unique<airwindows_reverb::Galactic3::Core>();
    auto measure = [&](double r) {
      core->Reset(r);
      core->Set({.5, .7, .5, 1., .5, 1.});
      const int total = static_cast<int>(r * 2.);
      const int burst = static_cast<int>(r * .1);
      const int e0 = static_cast<int>(r * .3), e1 = static_cast<int>(r * .8);
      const int l0 = static_cast<int>(r * 1.5), l1 = total;
      double early = 0., late = 0.;
      for (int i = 0; i < total; ++i)
      {
        double l = i < burst ? .2 * std::sin(2. * grid_fx::pi * 330. * i / r) : 0.;
        double ri = l;
        core->Tick(l, ri);
        const double p = .5 * (l * l + ri * ri);
        if (i >= e0 && i < e1) early += p;
        if (i >= l0 && i < l1) late += p;
      }
      return std::array<double, 2>{std::sqrt(early / (e1 - e0)), std::sqrt(late / (l1 - l0))};
    };
    const auto ref = measure(44100.);
    assert(ref[0] > 1.e-3 && ref[1] > 1.e-4);
    for (double r : {48000., 96000., 192000.})
    {
      const auto cur = measure(r);
      assert(cur[0] / ref[0] > .75 && cur[0] / ref[0] < 1.33);
      assert(cur[1] / ref[1] > .75 && cur[1] / ref[1] < 1.33);
    }
  }
  std::cout << "Reverb/delay candidates: 44.1-192 kHz, full-range automation, partition invariance, stereo dry, no allocations, head timing and diffusion passed\n";
}
