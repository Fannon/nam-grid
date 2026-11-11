#pragma once

void TestNoisePrintTracking()
{
  // A matching level rise is useful; arbitrary spectral changes, held partials
  // and coherent decays must not arm an upward estimate.
  for (double rate : {44100., 48000., 96000., 192000.})
  {
    noise_print::Tracking follow;
    follow.Reset(rate);
    std::array<double, noise_print::bands> reference, live;
    reference.fill(1.e-6);
    follow.Reference(reference, true);
    auto run = [&](double seconds, bool enabled = true) {
      allocations = deletions = 0; tracking = true;
      for (int i = 0; i < static_cast<int>(seconds * rate); ++i)
      {
        const double scale = follow.Tick(live, enabled);
        assert(std::isfinite(scale) && scale >= 1. && scale <= 4.);
      }
      tracking = false;
      assert(!allocations && !deletions);
    };
    live.fill(6.3e-6); // +8 dB requests more than the +6 dB safety limit.
    run(5.);
    assert(follow.Scale() > 3.8);
    run(1., false);
    assert(follow.Scale() == 1. && !follow.Active());
    follow.Reference(reference, true);
    live.fill(6.3e-6);
    for (int k = 40; k < 60; ++k) live[k] *= 50.; // A sustained harmonic region.
    run(3.);
    assert(follow.Scale() == 1.);
    follow.Reference(reference, true);
    for (int step = 0; step < 80; ++step)
    {
      live.fill(20.e-6 * std::exp(-step * .035));
      run(.05);
    }
    assert(follow.Scale() == 1.);
    reference.fill(1.e-12); reference[50] = 1.e-6;
    follow.Reference(reference, true);
    live = reference;
    for (double& p : live) p *= 4.;
    run(3.);
    assert(follow.Scale() == 1.); // Tonal prints are deliberately ineligible.
    reference.fill(1.e-6);
    follow.Reference(reference, false);
    live.fill(4.e-6);
    run(2.);
    assert(follow.Scale() == 1.);
  }

  // Exercise the real resonator detector and linked stereo gain, not just the
  // control estimator. Partitioning cannot alter its confidence or timing.
  for (double rate : {44100., 48000., 96000.})
  {
    noise_print::Processor fixed, follow, split;
    fixed.Reset(rate); follow.Reset(rate); split.Reset(rate);
    noise_print::Controls c;
    uint32_t seed = 983;
    std::array<double, 128> a{}, b{}, r{}, partitioned{}, sr{};
    double fixedEnergy = 0., followEnergy = 0.;
    noise_print::Profile committed;
    const int blocks = static_cast<int>(8. * rate / a.size());
    for (int block = 0; block < blocks; ++block)
    {
      const double t = block * a.size() / rate;
      c.learn = t < 2.;
      auto adaptive = c; adaptive.trackingMode = 1;
      for (int i = 0; i < a.size(); ++i)
      {
        seed = seed * 1664525u + 1013904223u;
        a[i] = b[i] = partitioned[i] = .002 * (double(seed) / 4294967296. * 2. - 1.) * (t >= 2. ? 2.5118864315 : 1.);
        r[i] = sr[i] = -a[i];
      }
      // A mono detector must agree with identical/anti-phase stereo power.
      allocations = deletions = 0; tracking = true;
      fixed.Process(a.data(), nullptr, a.size(), c);
      follow.Process(b.data(), r.data(), b.size(), adaptive);
      for (int offset = 0; offset < partitioned.size(); offset += 37)
        split.Process(partitioned.data() + offset, sr.data() + offset, std::min(37, int(partitioned.size()) - offset), adaptive);
      tracking = false;
      assert(!allocations && !deletions);
      for (int i = 0; i < b.size(); ++i)
      {
        assert(std::isfinite(b[i]) && b[i] == -r[i] && b[i] == partitioned[i] && r[i] == sr[i]);
        if (t >= 7.) { fixedEnergy += a[i] * a[i]; followEnergy += b[i] * b[i]; }
      }
      if (t > 2.1 && !committed.valid)
      {
        committed = follow.GetProfile();
        follow.TakeChanged();
      }
    }
    assert(committed.valid && follow.GetProfile() == committed && !follow.TakeChanged());
    assert(followEnergy < fixedEnergy * .5); // At least 3 dB extra suppression after a steady rise.
    assert(follow.TrackingScale() > 2. && follow.TrackingScale() <= 4.);
    follow.Reset(rate);
    assert(follow.TrackingScale() == 1. && follow.GetProfile() == committed);
    follow.Restore(committed);
    assert(follow.TrackingScale() == 1.);
  }
  const auto defaults = SlotDefaultValues(SlotEffect::NoisePrint);
  assert(defaults[17 + uNoiseTrackingMode] == 0.);
  assert(noise_print::Controls{}.trackingMode == 0);
}
